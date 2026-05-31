/* SPDX-License-Identifier: MIT */

#include <zproj/zproj.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zproj, CONFIG_ZPROJ_LOG_LEVEL);

static struct zproj_ctx *runtime_ctx;
static struct zproj_result runtime_result;
static struct zproj_trace_entry runtime_trace_buf[CONFIG_ZPROJ_MAX_TRACE_ENTRIES];
static struct zproj_trace runtime_trace;
static void (*runtime_post_eval_cb)(const struct zproj_result *result);

void zproj_runtime_register(struct zproj_ctx *ctx,
			    void (*post_eval)(const struct zproj_result *))
{
	runtime_ctx = ctx;
	runtime_post_eval_cb = post_eval;
	zproj_trace_init(&runtime_trace, runtime_trace_buf,
			 CONFIG_ZPROJ_MAX_TRACE_ENTRIES);
}

static void zproj_runtime_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("zproj runtime thread started (period=%d ms)",
		CONFIG_ZPROJ_PERIOD_MS);

	while (true) {
		k_sleep(K_MSEC(CONFIG_ZPROJ_PERIOD_MS));

		if (runtime_ctx == NULL || !runtime_ctx->initialized) {
			continue;
		}

		struct zproj_snapshot snap;
		int ret = zproj_snapshot_begin(runtime_ctx, &snap);

		if (ret != ZPROJ_OK) {
			LOG_WRN("Snapshot failed: %d", ret);
			continue;
		}

		zproj_trace_reset(&runtime_trace);
		ret = zproj_eval(runtime_ctx->model, &snap,
				 &runtime_result, &runtime_trace);

		if (ret != ZPROJ_OK) {
			LOG_ERR("Eval failed: %d", ret);
			/* Do NOT feed watchdog on failure */
			continue;
		}

		runtime_ctx->last_eval_op_count = runtime_result.eval_op_count;

#if defined(CONFIG_ZPROJ_WATCHDOG)
		extern void zproj_watchdog_feed(void);
		zproj_watchdog_feed();
#endif

		if (runtime_post_eval_cb != NULL) {
			runtime_post_eval_cb(&runtime_result);
		}
	}
}

K_THREAD_DEFINE(zproj_runtime_tid, CONFIG_ZPROJ_THREAD_STACK_SIZE,
		zproj_runtime_entry, NULL, NULL, NULL,
		CONFIG_ZPROJ_THREAD_PRIORITY, 0, 0);
