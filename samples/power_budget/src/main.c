/* SPDX-License-Identifier: MIT */

/**
 * Power Budget Solver Sample
 *
 * zproj allocates a limited power budget across 4 subsystems,
 * shedding low-priority loads when demand exceeds supply and
 * triggering brownout protection when battery is critical.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zproj/zproj.h>

LOG_MODULE_REGISTER(power_budget, LOG_LEVEL_INF);

extern const struct zproj_model zproj_generated_model;

static struct zproj_ctx ctx;

enum {
	F_BUDGET_MW = 0,
	F_ENABLE,
	F_BATTERY_PCT,
	/* sub0 — radio */
	F_S0_DEMAND, F_S0_PRIO, F_S0_ALLOC,
	/* sub1 — sensors */
	F_S1_DEMAND, F_S1_PRIO, F_S1_ALLOC,
	/* sub2 — display */
	F_S2_DEMAND, F_S2_PRIO, F_S2_ALLOC,
	/* sub3 — actuator */
	F_S3_DEMAND, F_S3_PRIO, F_S3_ALLOC,
	/* solver state */
	F_TOTAL_DEMAND, F_REMAINING, F_OVERCOMMIT,
};

void app_apply_power_config(void)
{
	LOG_INF("  alloc: radio=%d  sensors=%d  display=%d  actuator=%d  "
		"remaining=%d mW",
		ctx.fact_values[F_S0_ALLOC].value,
		ctx.fact_values[F_S1_ALLOC].value,
		ctx.fact_values[F_S2_ALLOC].value,
		ctx.fact_values[F_S3_ALLOC].value,
		ctx.fact_values[F_REMAINING].value);
}

void app_brownout_shutdown(void)
{
	LOG_ERR("  !! BROWNOUT — all loads shed");
}

static void run_scenario(const char *label, int32_t budget,
			 int32_t battery_pct)
{
	LOG_INF("--- %s: budget=%d mW, battery=%d%% ---",
		label, budget, battery_pct);

	zproj_set_i32(&ctx, F_BUDGET_MW, budget);
	zproj_set_i32(&ctx, F_BATTERY_PCT, battery_pct);

	struct zproj_snapshot snap;
	struct zproj_result result;

	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&zproj_generated_model, &snap, &result, NULL);
}

int main(void)
{
	LOG_INF("=== zproj Power Budget Solver ===");

	int ret = zproj_init(&ctx, &zproj_generated_model);

	if (ret != ZPROJ_OK) {
		LOG_ERR("Init failed: %d", ret);
		return ret;
	}

	zproj_set_bool(&ctx, F_ENABLE, true);

	/* Set subsystem demands */
	zproj_set_i32(&ctx, F_S0_DEMAND, 8000);  /* radio: 8 W */
	zproj_set_i32(&ctx, F_S1_DEMAND, 3000);  /* sensors: 3 W */
	zproj_set_i32(&ctx, F_S2_DEMAND, 5000);  /* display: 5 W */
	zproj_set_i32(&ctx, F_S3_DEMAND, 10000); /* actuator: 10 W */

	run_scenario("Normal",      30000, 80);
	run_scenario("Constrained", 22000, 50);
	run_scenario("Tight",       15000, 25);
	run_scenario("Brownout",    10000, 3);

	return 0;
}
