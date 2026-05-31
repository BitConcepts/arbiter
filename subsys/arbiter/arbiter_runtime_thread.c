/* SPDX-License-Identifier: MIT */

#include <arbiter/arbiter.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(arbiter, CONFIG_ARBITER_LOG_LEVEL);

static struct ARBITER_ctx *runtime_ctx;
static struct ARBITER_result runtime_result;
static struct ARBITER_trace_entry runtime_trace_buf[CONFIG_ARBITER_MAX_TRACE_ENTRIES];
static struct ARBITER_trace runtime_trace;
static void (*runtime_post_eval_cb)(const struct ARBITER_result *result);

void ARBITER_runtime_register(struct ARBITER_ctx *ctx,
			    void (*post_eval)(const struct ARBITER_result *))
{
	runtime_ctx = ctx;
	runtime_post_eval_cb = post_eval;
	ARBITER_trace_init(&runtime_trace, runtime_trace_buf,
			 CONFIG_ARBITER_MAX_TRACE_ENTRIES);
}

static void ARBITER_runtime_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("arbiter runtime thread started (period=%d ms)",
		CONFIG_ARBITER_PERIOD_MS);

	while (true) {
		k_sleep(K_MSEC(CONFIG_ARBITER_PERIOD_MS));

		if (runtime_ctx == NULL || !runtime_ctx->initialized) {
			continue;
		}

		struct ARBITER_snapshot snap;
		int ret = ARBITER_snapshot_begin(runtime_ctx, &snap);

		if (ret != ARBITER_OK) {
			LOG_WRN("Snapshot failed: %d", ret);
			continue;
		}

		ARBITER_trace_reset(&runtime_trace);
		ret = ARBITER_eval(runtime_ctx->model, &snap,
				 &runtime_result, &runtime_trace);

		if (ret != ARBITER_OK) {
			LOG_ERR("Eval failed: %d", ret);
			/* Do NOT feed watchdog on failure */
			continue;
		}

		runtime_ctx->last_eval_op_count = runtime_result.eval_op_count;

#if defined(CONFIG_ARBITER_WATCHDOG)
		extern void ARBITER_watchdog_feed(void);
		ARBITER_watchdog_feed();
#endif

		if (runtime_post_eval_cb != NULL) {
			runtime_post_eval_cb(&runtime_result);
		}
	}
}

K_THREAD_DEFINE(ARBITER_runtime_tid, CONFIG_ARBITER_THREAD_STACK_SIZE,
		ARBITER_runtime_entry, NULL, NULL, NULL,
		CONFIG_ARBITER_THREAD_PRIORITY, 0, 0);
