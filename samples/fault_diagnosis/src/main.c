/* SPDX-License-Identifier: MIT */

/**
 * Fault Diagnosis Solver Sample
 *
 * zproj maps observed symptom flags to candidate root causes using
 * weighted evidence accumulation.  Each symptom contributes
 * confidence toward one or more diagnoses; the solver selects the
 * highest-confidence cause.
 *
 * Domain: motor drive diagnostics with 6 symptoms × 4 diagnoses.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zproj/zproj.h>

LOG_MODULE_REGISTER(fault_diag, LOG_LEVEL_INF);

extern const struct zproj_model zproj_generated_model;

static struct zproj_ctx ctx;

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
	zproj_set_bool(&ctx, F_OVERCURRENT, false);
	zproj_set_bool(&ctx, F_OVERTEMP, false);
	zproj_set_bool(&ctx, F_VIBRATION, false);
	zproj_set_bool(&ctx, F_SPEED_DEV, false);
	zproj_set_bool(&ctx, F_VOLTAGE_SAG, false);
	zproj_set_bool(&ctx, F_COMM_TIMEOUT, false);
}

static void run_eval(const char *label)
{
	LOG_INF("--- %s ---", label);

	struct zproj_snapshot snap;
	struct zproj_result result;

	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&zproj_generated_model, &snap, &result, NULL);
}

int main(void)
{
	LOG_INF("=== zproj Fault Diagnosis Solver ===");

	int ret = zproj_init(&ctx, &zproj_generated_model);

	if (ret != ZPROJ_OK) {
		LOG_ERR("Init failed: %d", ret);
		return ret;
	}

	zproj_set_bool(&ctx, F_ENABLE, true);

	/* Scenario 1: bearing failure symptoms */
	clear_symptoms();
	zproj_set_bool(&ctx, F_OVERTEMP, true);
	zproj_set_bool(&ctx, F_VIBRATION, true);
	run_eval("Bearing failure pattern");

	/* Scenario 2: winding short symptoms */
	clear_symptoms();
	zproj_set_bool(&ctx, F_OVERCURRENT, true);
	zproj_set_bool(&ctx, F_OVERTEMP, true);
	run_eval("Winding short pattern");

	/* Scenario 3: supply fault symptoms */
	clear_symptoms();
	zproj_set_bool(&ctx, F_VOLTAGE_SAG, true);
	zproj_set_bool(&ctx, F_OVERCURRENT, true);
	run_eval("Supply fault pattern");

	/* Scenario 4: encoder fault symptoms */
	clear_symptoms();
	zproj_set_bool(&ctx, F_SPEED_DEV, true);
	zproj_set_bool(&ctx, F_COMM_TIMEOUT, true);
	run_eval("Encoder fault pattern");

	/* Scenario 5: no symptoms */
	clear_symptoms();
	run_eval("No fault");

	return 0;
}
