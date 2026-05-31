/* SPDX-License-Identifier: MIT */

/**
 * PID Engine Sample
 *
 * arbiter IS the PID controller. The application only provides:
 *   - setpoint and process_value each tick
 *   - sensor health status
 *   - an actuator callback
 *
 * arbiter's compute engine handles all the math:
 *   error, P/I/D terms, output summing, clamping, anti-windup.
 *
 * This is not a safety supervisor watching a PID — this IS the PID.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <arbiter/arbiter.h>

LOG_MODULE_REGISTER(pid_engine, LOG_LEVEL_INF);

extern const struct ARBITER_model ARBITER_generated_model;

static struct ARBITER_ctx ctx;

/* Simulated plant state */
static int32_t plant_position = 0; /* millideg */
static int32_t actuator_output = 0;

/*
 * Fact indices — alphabetical by id (canonical order from arbiterc).
 * In production, use #defines from the generated header.
 */
enum {
	F_KD = 0,           /* gain.kd */
	F_KI,               /* gain.ki */
	F_KP,               /* gain.kp */
	F_DT_MS,            /* in.dt_ms */
	F_ENABLE,           /* in.enable */
	F_PROCESS_VALUE,    /* in.process_value */
	F_SENSOR_VALID,     /* in.sensor_valid */
	F_SETPOINT,         /* in.setpoint */
	F_ABS_ERROR,        /* pid.abs_error */
	F_D_TERM,           /* pid.d_term */
	F_ERROR,            /* pid.error */
	F_ERROR_PREV,       /* pid.error_prev */
	F_I_TERM,           /* pid.i_term */
	F_OUTPUT,           /* pid.output */
	F_OUTPUT_RAW,       /* pid.output_raw */
	F_P_TERM,           /* pid.p_term */
};

/**
 * Actuator callback — called by arbiter's action dispatcher.
 * Reads the computed output from the context and drives the plant.
 */
void app_update_actuator(void)
{
	actuator_output = ctx.fact_values[F_OUTPUT].value;

	/* Simple first-order plant: position += output * gain */
	plant_position += actuator_output / 10;
}

/**
 * Run one PID tick: feed inputs, evaluate, let arbiter compute everything.
 */
static void pid_tick(int32_t setpoint, uint32_t tick)
{
	/* Feed inputs */
	ARBITER_set_i32(&ctx, F_SETPOINT, setpoint);
	ARBITER_set_i32(&ctx, F_PROCESS_VALUE, plant_position);
	ARBITER_set_timestamp(&ctx, F_PROCESS_VALUE, k_uptime_get_32());
	ARBITER_set_bool(&ctx, F_SENSOR_VALID, true);
	ARBITER_set_bool(&ctx, F_ENABLE, true);
	ARBITER_set_u32(&ctx, F_DT_MS, 10);

	/* Snapshot + evaluate — arbiter computes error, P, I, D, output */
	struct ARBITER_snapshot snap;
	struct ARBITER_result result;

	ARBITER_snapshot_begin(&ctx, &snap);
	ARBITER_eval(&ARBITER_generated_model, &snap, &result, NULL);

	/* The actuator callback was requested — call it */
	app_update_actuator();

	uint16_t mode;

	ARBITER_get_mode(&result, &mode);

	if (tick % 10 == 0) {
		LOG_INF("t=%3u  sp=%6d  pv=%6d  err=%6d  "
			"P=%5d I=%5d D=%5d  out=%5d  mode=%u",
			tick, setpoint, plant_position,
			ctx.fact_values[F_ERROR].value,
			ctx.fact_values[F_P_TERM].value,
			ctx.fact_values[F_I_TERM].value,
			ctx.fact_values[F_D_TERM].value,
			ctx.fact_values[F_OUTPUT].value,
			mode);
	}
}

int main(void)
{
	LOG_INF("=== arbiter PID Engine Demo ===");
	LOG_INF("arbiter computes the entire PID loop.");

	int ret = ARBITER_init(&ctx, &ARBITER_generated_model);

	if (ret != ARBITER_OK) {
		LOG_ERR("Init failed: %d", ret);
		return ret;
	}

	/* Set gains (defaults already loaded, but show tunability) */
	ARBITER_set_i32(&ctx, F_KP, 2500);  /* Kp = 2.5 */
	ARBITER_set_i32(&ctx, F_KI, 100);   /* Ki = 0.1 */
	ARBITER_set_i32(&ctx, F_KD, 800);   /* Kd = 0.8 */

	LOG_INF("Step response: setpoint = 10000 millideg (10 deg)");

	for (uint32_t t = 0; t < 100; t++) {
		pid_tick(10000, t);
		k_sleep(K_MSEC(10));
	}

	LOG_INF("Setpoint change: 10000 -> 5000");

	for (uint32_t t = 100; t < 200; t++) {
		pid_tick(5000, t);
		k_sleep(K_MSEC(10));
	}

	LOG_INF("Final position: %d millideg", plant_position);

	return 0;
}
