/* SPDX-License-Identifier: MIT */

/**
 * Valve Sequencer — Process Reasoning Engine
 *
 * zproj reasons through a multi-step batch process:
 *   idle -> fill -> add reagent -> react (timed hold) -> drain -> vent -> complete
 *
 * Each step transition is a reasoning rule with conditions on sensor
 * values and computed state. Safety guards for overpressure, overtemp,
 * and operator abort override at any point.
 *
 * This demonstrates zproj as a stateful sequencer — it manages
 * proc.step as a derived fact, advancing the process through logic.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zproj/zproj.h>

LOG_MODULE_REGISTER(valve_seq, LOG_LEVEL_INF);

extern const struct zproj_model zproj_generated_model;
static struct zproj_ctx ctx;

/* Callbacks */
void app_emergency_vent(void)         { LOG_WRN("SAFETY: Emergency vent!"); }
void app_emergency_cool(void)         { LOG_WRN("SAFETY: Emergency cooling!"); }
void app_close_all_valves(void)       { LOG_INF("VALVES: All closed"); }
void app_open_inlet(void)             { LOG_INF("VALVES: Inlet OPEN"); }
void app_close_inlet_open_reagent(void) { LOG_INF("VALVES: Inlet CLOSE, Reagent OPEN"); }
void app_close_reagent(void)          { LOG_INF("VALVES: Reagent CLOSE"); }
void app_open_outlet(void)            { LOG_INF("VALVES: Outlet OPEN"); }
void app_close_outlet_open_vent(void) { LOG_INF("VALVES: Outlet CLOSE, Vent OPEN"); }

int main(void)
{
	LOG_INF("=== zproj Valve Sequencer Demo ===");

	int ret = zproj_init(&ctx, &zproj_generated_model);

	if (ret != ZPROJ_OK) {
		LOG_ERR("Init failed: %d", ret);
		return ret;
	}

	struct zproj_snapshot snap;
	struct zproj_result result;
	uint16_t mode;

	/* Helper: fact indices (canonical alpha order) */
	/* calc.drain_complete=0, calc.fill_complete=1, calc.reaction_complete=2,
	 * proc.abort_cmd=3, proc.start_cmd=4, proc.step=5, proc.step_timer_ms=6,
	 * sensor.level_pct=7, sensor.pressure_kpa=8, sensor.temp_c=9,
	 * valve.inlet=10, valve.outlet=11, valve.reagent=12, valve.vent=13
	 */

	#define F_FILL_COMPLETE   1
	#define F_REACT_COMPLETE  2
	#define F_ABORT           3
	#define F_START           4
	#define F_STEP            5
	#define F_TIMER           6
	#define F_LEVEL           7
	#define F_PRESSURE        8
	#define F_TEMP            9

	/* Safe starting conditions */
	zproj_set_u32(&ctx, F_PRESSURE, 1010);  /* ~101 kPa (atmospheric) */
	zproj_set_i32(&ctx, F_TEMP, 250);       /* 25 °C */
	zproj_set_bool(&ctx, F_ABORT, false);

	LOG_INF("--- Start sequence ---");
	zproj_set_bool(&ctx, F_START, true);
	zproj_set_u32(&ctx, F_LEVEL, 0);
	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&zproj_generated_model, &snap, &result, NULL);
	zproj_get_mode(&result, &mode);
	LOG_INF("  Step: %d  Mode: %u", ctx.fact_values[F_STEP].value, mode);

	LOG_INF("--- Fill to 85%% ---");
	zproj_set_u32(&ctx, F_LEVEL, 85);
	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&zproj_generated_model, &snap, &result, NULL);
	zproj_get_mode(&result, &mode);
	LOG_INF("  Step: %d  Mode: %u", ctx.fact_values[F_STEP].value, mode);

	LOG_INF("--- Reagent fills to 92%% ---");
	zproj_set_u32(&ctx, F_LEVEL, 92);
	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&zproj_generated_model, &snap, &result, NULL);
	zproj_get_mode(&result, &mode);
	LOG_INF("  Step: %d  Mode: %u", ctx.fact_values[F_STEP].value, mode);

	LOG_INF("--- Reaction timer > 30s ---");
	zproj_set_u32(&ctx, F_TIMER, 35000);
	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&zproj_generated_model, &snap, &result, NULL);
	zproj_get_mode(&result, &mode);
	LOG_INF("  Step: %d  Mode: %u", ctx.fact_values[F_STEP].value, mode);

	LOG_INF("--- Drain to 3%% ---");
	zproj_set_u32(&ctx, F_LEVEL, 3);
	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&zproj_generated_model, &snap, &result, NULL);
	zproj_get_mode(&result, &mode);
	LOG_INF("  Step: %d  Mode: %u", ctx.fact_values[F_STEP].value, mode);

	LOG_INF("--- Vent complete (pressure atmospheric) ---");
	zproj_set_u32(&ctx, F_PRESSURE, 1000);
	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&zproj_generated_model, &snap, &result, NULL);
	zproj_get_mode(&result, &mode);
	LOG_INF("  Step: %d  Mode: %u  SEQUENCE COMPLETE", ctx.fact_values[F_STEP].value, mode);

	return 0;
}
