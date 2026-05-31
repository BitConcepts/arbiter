/* SPDX-License-Identifier: MIT */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <arbiter/arbiter.h>

LOG_MODULE_REGISTER(battery_sample, LOG_LEVEL_INF);

/* Forward-declare generated model */
extern const struct ARBITER_model ARBITER_generated_model;

static struct ARBITER_ctx ctx;

/* Application callbacks */
void app_disable_load(void)
{
	LOG_WRN("ACTION: Load disabled (battery critical)");
}

void app_disable_charger(void)
{
	LOG_WRN("ACTION: Charger disabled (thermal shutdown)");
}

int main(void)
{
	LOG_INF("arbiter battery policy sample");
	LOG_INF("arbiter version: %s", ARBITER_version_string());

	int ret = ARBITER_init(&ctx, &ARBITER_generated_model);

	if (ret != ARBITER_OK) {
		LOG_ERR("Failed to init arbiter: %d", ret);
		return ret;
	}

	/* Simulate setting fact values */
	ARBITER_set_u32(&ctx, 0, 3500);  /* battery.voltage_mv = 3500 mV */
	ARBITER_set_i32(&ctx, 1, 200);   /* battery.current_ma = 200 mA */
	ARBITER_set_i32(&ctx, 2, 25);    /* battery.temp_c = 25 °C */
	ARBITER_set_bool(&ctx, 3, false); /* charger.enabled = false */

	/* Take snapshot and evaluate */
	struct ARBITER_snapshot snap;
	struct ARBITER_result result;
	struct ARBITER_trace_entry trace_buf[16];
	struct ARBITER_trace trace;

	ARBITER_trace_init(&trace, trace_buf, 16);
	ARBITER_snapshot_begin(&ctx, &snap);
	ARBITER_eval(&ARBITER_generated_model, &snap, &result, &trace);

	uint16_t mode;

	ARBITER_get_mode(&result, &mode);
	LOG_INF("Result: mode=%u faults=0x%08x actions=%u ops=%u",
		mode, result.raised_faults,
		result.requested_action_count, result.eval_op_count);

	/* Print trace */
	for (uint16_t i = 0; i < trace.count; i++) {
		const struct ARBITER_trace_entry *e = ARBITER_trace_get(&trace, i);

		LOG_INF("  rule[%u] %s -> %s",
			e->rule_id,
			e->condition_result ? "FIRED" : "skip",
			e->reason ? e->reason : "");
	}

	return 0;
}
