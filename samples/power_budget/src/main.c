/* SPDX-License-Identifier: MIT */

/**
 * Power Budget Solver Sample
 *
 * arbiter allocates a limited power budget across 4 subsystems,
 * shedding low-priority loads when demand exceeds supply and
 * triggering brownout protection when battery is critical.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <arbiter/arbiter.h>

LOG_MODULE_REGISTER(power_budget, LOG_LEVEL_INF);

extern const struct ARBITER_model ARBITER_generated_model;

static struct ARBITER_ctx ctx;

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

	ARBITER_set_i32(&ctx, F_BUDGET_MW, budget);
	ARBITER_set_i32(&ctx, F_BATTERY_PCT, battery_pct);

	struct ARBITER_snapshot snap;
	struct ARBITER_result result;

	ARBITER_snapshot_begin(&ctx, &snap);
	ARBITER_eval(&ARBITER_generated_model, &snap, &result, NULL);
}

int main(void)
{
	LOG_INF("=== arbiter Power Budget Solver ===");

	int ret = ARBITER_init(&ctx, &ARBITER_generated_model);

	if (ret != ARBITER_OK) {
		LOG_ERR("Init failed: %d", ret);
		return ret;
	}

	ARBITER_set_bool(&ctx, F_ENABLE, true);

	/* Set subsystem demands */
	ARBITER_set_i32(&ctx, F_S0_DEMAND, 8000);  /* radio: 8 W */
	ARBITER_set_i32(&ctx, F_S1_DEMAND, 3000);  /* sensors: 3 W */
	ARBITER_set_i32(&ctx, F_S2_DEMAND, 5000);  /* display: 5 W */
	ARBITER_set_i32(&ctx, F_S3_DEMAND, 10000); /* actuator: 10 W */

	run_scenario("Normal",      30000, 80);
	run_scenario("Constrained", 22000, 50);
	run_scenario("Tight",       15000, 25);
	run_scenario("Brownout",    10000, 3);

	return 0;
}
