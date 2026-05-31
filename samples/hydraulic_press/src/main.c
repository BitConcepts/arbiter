/* SPDX-License-Identifier: MIT */

/**
 * Hydraulic Press Safety Interlock
 *
 * Demonstrates a classic industrial safety interlock using zproj:
 * guard interlocks, two-hand control, overpressure protection,
 * thermal monitoring, and position limits. Modeled after IEC 62061
 * and ISO 13849 patterns.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zproj/zproj.h>

LOG_MODULE_REGISTER(press_sample, LOG_LEVEL_INF);

extern const struct zproj_model zproj_generated_model;

static struct zproj_ctx ctx;

/* Application callbacks */
void app_dump_pressure(void)
{
	LOG_WRN("SAFETY: Dumping hydraulic pressure!");
}

void app_hold_retract(void)
{
	LOG_WRN("SAFETY: Holding position / retracting ram");
}

void app_open_relief_valve(void)
{
	LOG_WRN("SAFETY: Relief valve opened");
}

void app_stop_pump(void)
{
	LOG_INF("POLICY: Hydraulic pump stopped");
}

void app_stop_advance(void)
{
	LOG_INF("POLICY: Ram advance stopped (limit)");
}

int main(void)
{
	LOG_INF("zproj hydraulic press safety interlock");

	int ret = zproj_init(&ctx, &zproj_generated_model);

	if (ret != ZPROJ_OK) {
		LOG_ERR("Init failed: %d", ret);
		return ret;
	}

	struct zproj_snapshot snap;
	struct zproj_result result;
	struct zproj_trace_entry trace_buf[16];
	struct zproj_trace trace;
	uint16_t mode;

	/*
	 * Scenario 1: All interlocks satisfied — ready to press
	 */
	LOG_INF("--- Scenario: Ready to press ---");
	/* Facts are in canonical (alphabetical) order by id */
	zproj_set_bool(&ctx, 0, true);   /* guard.closed */
	zproj_set_bool(&ctx, 1, true);   /* guard.locked */
	zproj_set_bool(&ctx, 2, true);   /* press.cycle_start */
	zproj_set_u32(&ctx, 3, 0);       /* press.cycle_count */
	zproj_set_bool(&ctx, 4, false);  /* press.estop */
	zproj_set_u32(&ctx, 5, 10000);   /* press.force_kn = 1000 kN */
	zproj_set_i32(&ctx, 6, 350);     /* press.oil_temp_c = 35 °C */
	zproj_set_u32(&ctx, 7, 5000);    /* press.pressure_bar = 500 bar */
	zproj_set_i32(&ctx, 8, 1000);    /* press.ram_position_mm = 100 mm */
	zproj_set_bool(&ctx, 9, true);   /* twohand.left */
	zproj_set_bool(&ctx, 10, true);  /* twohand.right */

	zproj_trace_init(&trace, trace_buf, 16);
	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&zproj_generated_model, &snap, &result, &trace);
	zproj_get_mode(&result, &mode);
	LOG_INF("  Mode: %u  Actions: %u", mode, result.requested_action_count);

	/*
	 * Scenario 2: Guard opened mid-cycle
	 */
	LOG_INF("--- Scenario: Guard opened ---");
	zproj_set_bool(&ctx, 0, false);  /* guard.closed = false */

	zproj_trace_reset(&trace);
	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&zproj_generated_model, &snap, &result, &trace);
	zproj_get_mode(&result, &mode);
	LOG_INF("  Mode: %u  Actions: %u", mode, result.requested_action_count);

	/*
	 * Scenario 3: E-stop pressed
	 */
	LOG_INF("--- Scenario: E-stop ---");
	zproj_set_bool(&ctx, 0, true);   /* guard.closed = true */
	zproj_set_bool(&ctx, 4, true);   /* press.estop = true */

	zproj_trace_reset(&trace);
	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&zproj_generated_model, &snap, &result, &trace);
	zproj_get_mode(&result, &mode);
	LOG_INF("  Mode: %u  Actions: %u", mode, result.requested_action_count);

	return 0;
}
