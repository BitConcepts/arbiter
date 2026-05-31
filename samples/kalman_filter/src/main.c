/* SPDX-License-Identifier: MIT */

/**
 * Kalman Filter Solver Sample
 *
 * zproj implements a 1-D Kalman filter entirely in ZRM compute
 * expressions.  The application feeds noisy measurements and reads
 * the filtered state estimate.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zproj/zproj.h>

LOG_MODULE_REGISTER(kalman_filter, LOG_LEVEL_INF);

extern const struct zproj_model zproj_generated_model;

static struct zproj_ctx ctx;

/* Fact indices (canonical alphabetical order) */
enum {
	F_MEASUREMENT = 0, /* in.measurement */
	F_ENABLE,          /* in.enable */
	F_SENSOR_VALID,    /* in.sensor_valid */
	F_CORRECTION,      /* kf.correction */
	F_DENOM,           /* kf.denom */
	F_INNOVATION,      /* kf.innovation */
	F_K_GAIN,          /* kf.k_gain */
	F_P_EST,           /* kf.p_est */
	F_P_FACTOR,        /* kf.p_factor */
	F_P_PRED,          /* kf.p_pred */
	F_X_EST,           /* kf.x_est */
	F_X_PRED,          /* kf.x_pred */
	F_Q,               /* param.q */
	F_R,               /* param.r */
};

/* Simulated true value — a slowly varying signal */
static int32_t true_value = 0;

void app_publish_estimate(void)
{
	/* In a real system this pushes the filtered result downstream */
}

/**
 * Crude pseudo-random noise in [-amplitude, +amplitude].
 */
static int32_t noise(int32_t amplitude)
{
	static uint32_t seed = 12345;

	seed = seed * 1103515245u + 12345u;
	return ((int32_t)(seed >> 16) % (2 * amplitude + 1)) - amplitude;
}

int main(void)
{
	LOG_INF("=== zproj Kalman Filter Solver ===");

	int ret = zproj_init(&ctx, &zproj_generated_model);

	if (ret != ZPROJ_OK) {
		LOG_ERR("Init failed: %d", ret);
		return ret;
	}

	zproj_set_bool(&ctx, F_ENABLE, true);
	zproj_set_bool(&ctx, F_SENSOR_VALID, true);

	for (uint32_t t = 0; t < 120; t++) {
		/* Simulated true value: ramp then hold */
		if (t < 60) {
			true_value += 500; /* ramp up 0.5 unit/tick */
		}

		int32_t meas = true_value + noise(3000);

		zproj_set_i32(&ctx, F_MEASUREMENT, meas);
		zproj_set_timestamp(&ctx, F_MEASUREMENT, k_uptime_get_32());

		struct zproj_snapshot snap;
		struct zproj_result result;

		zproj_snapshot_begin(&ctx, &snap);
		zproj_eval(&zproj_generated_model, &snap, &result, NULL);

		if (t % 10 == 0) {
			LOG_INF("t=%3u  true=%6d  meas=%6d  est=%6d  "
				"K=%4d  P=%5d",
				t, true_value, meas,
				ctx.fact_values[F_X_EST].value,
				ctx.fact_values[F_K_GAIN].value,
				ctx.fact_values[F_P_EST].value);
		}

		k_sleep(K_MSEC(10));
	}

	LOG_INF("Final estimate: %d (true: %d)",
		ctx.fact_values[F_X_EST].value, true_value);

	return 0;
}
