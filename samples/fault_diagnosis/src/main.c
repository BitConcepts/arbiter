/* SPDX-License-Identifier: MIT */

/**
 * Fault Diagnosis Solver Sample
 *
 * arbiter maps observed symptom flags to candidate root causes using
 * weighted evidence accumulation.  Each symptom contributes
 * confidence toward one or more diagnoses; the solver selects the
 * highest-confidence cause.
 *
 * Domain: motor drive diagnostics with 6 symptoms × 4 diagnoses.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <arbiter/arbiter.h>

LOG_MODULE_REGISTER(fault_diag, LOG_LEVEL_INF);

extern const struct ARBITER_model ARBITER_generated_model;

static struct ARBITER_ctx ctx;

enum {
	F_OVERCURRENT = 0,
	F_OVERTEMP,
	F_VIBRATION,
	F_SPEED_DEV,
	F_VOLTAGE_SAG,
	F_COMM_TIMEOUT,
	F_ENABLE,
	F_D0_CONF,    /* winding_short */
	F_D1_CONF,    /* bearing_failure */
	F_D2_CONF,    /* supply_fault */
	F_D3_CONF,    /* encoder_fault */
	F_BEST_CONF,
	F_DIAG_ID,
};

static const char *diag_names[] = {
	"winding_short", "bearing_failure",
	"supply_fault",  "encoder_fault",
};

void app_report_diagnosis(void)
{
	int32_t id = ctx.fact_values[F_DIAG_ID].value;
	int32_t conf = ctx.fact_values[F_BEST_CONF].value;

	LOG_INF("  >> DIAGNOSIS: %s  confidence=%d/1000",
		(id >= 0 && id <= 3) ? diag_names[id] : "unknown",
		conf);
	LOG_INF("     scores: winding=%d  bearing=%d  supply=%d  encoder=%d",
		ctx.fact_values[F_D0_CONF].value,
		ctx.fact_values[F_D1_CONF].value,
		ctx.fact_values[F_D2_CONF].value,
		ctx.fact_values[F_D3_CONF].value);
}

static void clear_symptoms(void)
{
	ARBITER_set_bool(&ctx, F_OVERCURRENT, false);
	ARBITER_set_bool(&ctx, F_OVERTEMP, false);
	ARBITER_set_bool(&ctx, F_VIBRATION, false);
	ARBITER_set_bool(&ctx, F_SPEED_DEV, false);
	ARBITER_set_bool(&ctx, F_VOLTAGE_SAG, false);
	ARBITER_set_bool(&ctx, F_COMM_TIMEOUT, false);
}

static void run_eval(const char *label)
{
	LOG_INF("--- %s ---", label);

	struct ARBITER_snapshot snap;
	struct ARBITER_result result;

	ARBITER_snapshot_begin(&ctx, &snap);
	ARBITER_eval(&ARBITER_generated_model, &snap, &result, NULL);
}

int main(void)
{
	LOG_INF("=== arbiter Fault Diagnosis Solver ===");

	int ret = ARBITER_init(&ctx, &ARBITER_generated_model);

	if (ret != ARBITER_OK) {
		LOG_ERR("Init failed: %d", ret);
		return ret;
	}

	ARBITER_set_bool(&ctx, F_ENABLE, true);

	/* Scenario 1: bearing failure symptoms */
	clear_symptoms();
	ARBITER_set_bool(&ctx, F_OVERTEMP, true);
	ARBITER_set_bool(&ctx, F_VIBRATION, true);
	run_eval("Bearing failure pattern");

	/* Scenario 2: winding short symptoms */
	clear_symptoms();
	ARBITER_set_bool(&ctx, F_OVERCURRENT, true);
	ARBITER_set_bool(&ctx, F_OVERTEMP, true);
	run_eval("Winding short pattern");

	/* Scenario 3: supply fault symptoms */
	clear_symptoms();
	ARBITER_set_bool(&ctx, F_VOLTAGE_SAG, true);
	ARBITER_set_bool(&ctx, F_OVERCURRENT, true);
	run_eval("Supply fault pattern");

	/* Scenario 4: encoder fault symptoms */
	clear_symptoms();
	ARBITER_set_bool(&ctx, F_SPEED_DEV, true);
	ARBITER_set_bool(&ctx, F_COMM_TIMEOUT, true);
	run_eval("Encoder fault pattern");

	/* Scenario 5: no symptoms */
	clear_symptoms();
	run_eval("No fault");

	return 0;
}
