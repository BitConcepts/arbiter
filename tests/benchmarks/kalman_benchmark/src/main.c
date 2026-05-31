/* SPDX-License-Identifier: MIT */

/**
 * Kalman Filter Benchmark: zproj engine vs. hand-coded 1-D Kalman
 *
 * Measures CPU cycles, RAM, and provides ROM comparison methodology.
 * The hand-coded version is a minimal fixed-point 1-D Kalman filter
 * matching the logic in the kalman_filter sample model.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/timing/timing.h>
#include <zproj/zproj.h>

LOG_MODULE_REGISTER(kf_bench, LOG_LEVEL_INF);

/* ------------------------------------------------------------------ */
/*  Hand-coded 1-D Kalman filter (baseline)                           */
/* ------------------------------------------------------------------ */

struct hand_kf {
	int32_t x_est;       /* State estimate */
	int32_t p_est;       /* Error covariance (x1000) */
	int32_t q;           /* Process noise (x1000) */
	int32_t r;           /* Measurement noise (x1000) */
	int32_t k_gain;      /* Kalman gain (x1000) */
};

static void hand_kf_init(struct hand_kf *kf)
{
	kf->x_est = 0;
	kf->p_est = 10000;
	kf->q = 100;
	kf->r = 5000;
	kf->k_gain = 0;
}

static void hand_kf_tick(struct hand_kf *kf, int32_t measurement)
{
	/* Predict */
	int32_t x_pred = kf->x_est;
	int32_t p_pred = kf->p_est + kf->q;

	/* Kalman gain: K = P_pred / (P_pred + R) scaled x1000 */
	int32_t denom = p_pred + kf->r;

	if (denom == 0) {
		denom = 1;
	}

	kf->k_gain = (int32_t)(((int64_t)p_pred * 1000) / denom);

	/* Update state: x = x_pred + K * (z - x_pred) / 1000 */
	int32_t innovation = measurement - x_pred;
	int32_t correction = (int32_t)(((int64_t)kf->k_gain * innovation)
				       / 1000);
	kf->x_est = x_pred + correction;

	/* Update covariance: P = (1000 - K) * P_pred / 1000 */
	int32_t p_factor = 1000 - kf->k_gain;

	kf->p_est = (int32_t)(((int64_t)p_factor * p_pred) / 1000);
}

/* ------------------------------------------------------------------ */
/*  zproj Kalman filter (engine-based)                                */
/* ------------------------------------------------------------------ */

extern const struct zproj_model zproj_generated_model;

static struct zproj_ctx zctx;

/* Fact indices matching kalman model canonical order */
enum {
	F_MEASUREMENT = 0, F_ENABLE, F_SENSOR_VALID,
	F_CORRECTION, F_DENOM, F_INNOVATION, F_K_GAIN,
	F_P_EST, F_P_FACTOR, F_P_PRED, F_X_EST, F_X_PRED,
	F_Q, F_R,
};

void app_publish_estimate(void)
{
	/* No-op for benchmark */
}

/* ------------------------------------------------------------------ */
/*  Benchmark harness                                                 */
/* ------------------------------------------------------------------ */

#define BENCH_ITERATIONS  1000
#define WARMUP_ITERATIONS 100

/* Crude PRNG for deterministic noisy measurements */
static uint32_t prng_seed = 42;

static int32_t prng_noise(int32_t amplitude)
{
	prng_seed = prng_seed * 1103515245u + 12345u;
	return ((int32_t)(prng_seed >> 16) % (2 * amplitude + 1)) - amplitude;
}

int main(void)
{
	LOG_INF("=== zproj Kalman Filter Benchmark ===");
	LOG_INF("Iterations: %d  (warmup: %d)", BENCH_ITERATIONS,
		WARMUP_ITERATIONS);

	timing_init();
	timing_start();

	/* ----- Hand-coded Kalman benchmark ----- */
	struct hand_kf hkf;
	int32_t true_val = 0;

	hand_kf_init(&hkf);

	prng_seed = 42;

	/* Warmup */
	for (int i = 0; i < WARMUP_ITERATIONS; i++) {
		if (i < 60) {
			true_val += 500;
		}
		hand_kf_tick(&hkf, true_val + prng_noise(3000));
	}

	hand_kf_init(&hkf);
	true_val = 0;
	prng_seed = 42;

	timing_t t0 = timing_counter_get();

	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		if (i < 600) {
			true_val += 50;
		}
		hand_kf_tick(&hkf, true_val + prng_noise(3000));
	}

	timing_t t1 = timing_counter_get();
	uint64_t hand_ns = timing_cycles_to_ns(timing_cycles_get(&t0, &t1));
	uint64_t hand_per_tick = hand_ns / BENCH_ITERATIONS;

	LOG_INF("--- Hand-coded Kalman ---");
	LOG_INF("  Total: %llu ns  (%llu ns/tick)", hand_ns, hand_per_tick);
	LOG_INF("  RAM (struct): %u bytes", (unsigned)sizeof(struct hand_kf));
	LOG_INF("  Final estimate: %d (true: %d)", hkf.x_est, true_val);

	/* ----- zproj engine Kalman benchmark ----- */
	int ret = zproj_init(&zctx, &zproj_generated_model);

	if (ret != ZPROJ_OK) {
		LOG_ERR("zproj init failed: %d", ret);
		return ret;
	}

	zproj_set_bool(&zctx, F_ENABLE, true);
	zproj_set_bool(&zctx, F_SENSOR_VALID, true);

	true_val = 0;
	prng_seed = 42;

	/* Warmup */
	for (int i = 0; i < WARMUP_ITERATIONS; i++) {
		if (i < 60) {
			true_val += 500;
		}

		int32_t meas = true_val + prng_noise(3000);

		zproj_set_i32(&zctx, F_MEASUREMENT, meas);
		zproj_set_timestamp(&zctx, F_MEASUREMENT, (uint32_t)i * 10);

		struct zproj_snapshot snap;
		struct zproj_result result;

		zproj_snapshot_begin(&zctx, &snap);
		zproj_eval(&zproj_generated_model, &snap, &result, NULL);
	}

	/* Reset */
	zproj_init(&zctx, &zproj_generated_model);
	zproj_set_bool(&zctx, F_ENABLE, true);
	zproj_set_bool(&zctx, F_SENSOR_VALID, true);
	true_val = 0;
	prng_seed = 42;

	timing_t t2 = timing_counter_get();

	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		if (i < 600) {
			true_val += 50;
		}

		int32_t meas = true_val + prng_noise(3000);

		zproj_set_i32(&zctx, F_MEASUREMENT, meas);
		zproj_set_timestamp(&zctx, F_MEASUREMENT,
				    (uint32_t)(WARMUP_ITERATIONS + i) * 10);

		struct zproj_snapshot snap;
		struct zproj_result result;

		zproj_snapshot_begin(&zctx, &snap);
		zproj_eval(&zproj_generated_model, &snap, &result, NULL);
	}

	timing_t t3 = timing_counter_get();
	uint64_t zproj_ns = timing_cycles_to_ns(timing_cycles_get(&t2, &t3));
	uint64_t zproj_per_tick = zproj_ns / BENCH_ITERATIONS;

	LOG_INF("--- zproj Engine Kalman ---");
	LOG_INF("  Total: %llu ns  (%llu ns/tick)", zproj_ns, zproj_per_tick);
	LOG_INF("  RAM (ctx): %u bytes", (unsigned)sizeof(struct zproj_ctx));
	LOG_INF("  Final estimate: %d (true: %d)",
		zctx.fact_values[F_X_EST].value, true_val);
	LOG_INF("  Model: %u facts, %u rules, %u exprs",
		zproj_generated_model.fact_count,
		zproj_generated_model.rule_count,
		zproj_generated_model.expr_count);

	/* ----- Comparison ----- */
	LOG_INF("=== Comparison ===");

	uint64_t overhead_pct = 0;

	if (hand_per_tick > 0) {
		overhead_pct = ((zproj_per_tick - hand_per_tick) * 100)
			       / hand_per_tick;
	}

	LOG_INF("  Overhead: %llu%% (%llu vs %llu ns/tick)",
		overhead_pct, zproj_per_tick, hand_per_tick);
	LOG_INF("  RAM delta: %u bytes",
		(unsigned)(sizeof(struct zproj_ctx) - sizeof(struct hand_kf)));
	LOG_INF("  ROM: compare .elf sizes (see README)");

	timing_stop();

	return 0;
}
