/* SPDX-License-Identifier: MIT */

/**
 * Kalman Filter Benchmark: arbiter engine vs. hand-coded 1-D Kalman
 *
 * Measures CPU cycles, RAM, and provides ROM comparison methodology.
 * The hand-coded version is a minimal fixed-point 1-D Kalman filter
 * matching the logic in the kalman_filter sample model.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <arbiter/arbiter.h>
#include "arbiter_model.h"

/* On native_sim the firmware is a native Linux binary: use POSIX
 * clock_gettime(CLOCK_MONOTONIC) for real nanosecond timing.
 * Zephyr's timing API and k_uptime_get_32() both measure simulated
 * Zephyr kernel ticks which do NOT advance during CPU-bound loops.
 */
#ifdef CONFIG_NATIVE_SIMULATOR
/* native_sim's get_host_us_time() (defined in timer_model.c, compiled into
 * the same native_sim binary) calls CLOCK_MONOTONIC_RAW directly from the
 * OS timer model — it is the real host time in µs and is not intercepted
 * by any simulated-time override.
 */
extern uint64_t get_host_us_time(void);
static inline uint64_t bench_ns(void)
{
	return get_host_us_time() * 1000ULL;
}
#else
/* On real hardware fall back to Zephyr timing API */
#include <zephyr/timing/timing.h>
static inline uint64_t bench_ns(void)
{
	return (uint64_t)k_cycle_get_64() * 1000ULL /
		(sys_clock_hw_cycles_per_sec() / 1000000ULL);
}
#endif

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
/*  arbiter Kalman filter (engine-based)                                */
/* ------------------------------------------------------------------ */

/* Fact indices from generated header — canonical alphabetical order. */
#define F_ENABLE       ARBITER_FACT_IN_ENABLE
#define F_MEASUREMENT  ARBITER_FACT_IN_MEASUREMENT
#define F_SENSOR_VALID ARBITER_FACT_IN_SENSOR_VALID
#define F_CORRECTION   ARBITER_FACT_KF_CORRECTION
#define F_DENOM        ARBITER_FACT_KF_DENOM
#define F_INNOVATION   ARBITER_FACT_KF_INNOVATION
#define F_K_GAIN       ARBITER_FACT_KF_K_GAIN
#define F_P_EST        ARBITER_FACT_KF_P_EST
#define F_P_FACTOR     ARBITER_FACT_KF_P_FACTOR
#define F_P_PRED       ARBITER_FACT_KF_P_PRED
#define F_X_EST        ARBITER_FACT_KF_X_EST
#define F_X_PRED       ARBITER_FACT_KF_X_PRED
#define F_Q            ARBITER_FACT_PARAM_Q
#define F_R            ARBITER_FACT_PARAM_R

static struct ARBITER_ctx zctx;

void app_publish_estimate(void)
{
	/* No-op for benchmark */
}

/* ------------------------------------------------------------------ */
/*  Benchmark harness                                                 */
/* ------------------------------------------------------------------ */

/* Increase iterations so native_sim (1 MHz timing clock) yields non-zero
 * nanosecond counts. On real hardware or QEMU the loop completes faster
 * but still reports meaningful relative numbers.
 */
#define BENCH_ITERATIONS  100000
#define WARMUP_ITERATIONS 1000

/* Crude PRNG for deterministic noisy measurements */
static uint32_t prng_seed = 42;

static int32_t prng_noise(int32_t amplitude)
{
	prng_seed = prng_seed * 1103515245u + 12345u;
	return ((int32_t)(prng_seed >> 16) % (2 * amplitude + 1)) - amplitude;
}

int main(void)
{
	LOG_INF("=== arbiter Kalman Filter Benchmark ===");
	LOG_INF("Iterations: %d  (warmup: %d)", BENCH_ITERATIONS,
		WARMUP_ITERATIONS);

#ifdef CONFIG_NATIVE_SIMULATOR
	/* Debug: verify get_host_us_time() returns a non-zero advancing value */
	uint64_t dbg0 = bench_ns();
	uint64_t dbg1 = bench_ns();
	LOG_INF("bench_ns debug: t0_hi=%u t0_lo=%u dt_ns=%llu",
		(uint32_t)(dbg0 >> 32), (uint32_t)(dbg0 & 0xFFFFFFFF),
		dbg1 - dbg0);
#endif

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

	uint64_t t0_ns = bench_ns();

	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		if (i < 600) {
			true_val += 50;
		}
		hand_kf_tick(&hkf, true_val + prng_noise(3000));
	}

	uint64_t t1_ns = bench_ns();
	uint64_t hand_ns = t1_ns - t0_ns;
	uint64_t hand_per_tick = hand_ns / BENCH_ITERATIONS;

	LOG_INF("--- Hand-coded Kalman ---");
	LOG_INF("  Total: %llu ns  (%llu ns/tick)", hand_ns, hand_per_tick);
	LOG_INF("  RAM (struct): %u bytes", (unsigned)sizeof(struct hand_kf));
	LOG_INF("  Final estimate: %d (true: %d)", hkf.x_est, true_val);

	/* ----- arbiter engine Kalman benchmark ----- */
	int ret = ARBITER_init(&zctx, &ARBITER_generated_model);

	if (ret != ARBITER_OK) {
		LOG_ERR("arbiter init failed: %d", ret);
		return ret;
	}

	ARBITER_set_bool(&zctx, F_ENABLE, true);
	ARBITER_set_bool(&zctx, F_SENSOR_VALID, true);

	true_val = 0;
	prng_seed = 42;

	/* Warmup */
	for (int i = 0; i < WARMUP_ITERATIONS; i++) {
		if (i < 60) {
			true_val += 500;
		}

		int32_t meas = true_val + prng_noise(3000);

		ARBITER_set_i32(&zctx, F_MEASUREMENT, meas);
		ARBITER_set_timestamp(&zctx, F_MEASUREMENT, (uint32_t)i * 10);

		struct ARBITER_snapshot snap;
		struct ARBITER_result result;

		ARBITER_snapshot_begin(&zctx, &snap);
		ARBITER_eval(&ARBITER_generated_model, &snap, &result, NULL);
	}

	/* Reset */
	ARBITER_init(&zctx, &ARBITER_generated_model);
	ARBITER_set_bool(&zctx, F_ENABLE, true);
	ARBITER_set_bool(&zctx, F_SENSOR_VALID, true);
	true_val = 0;
	prng_seed = 42;

	uint64_t t2_ns = bench_ns();

	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		if (i < 600) {
			true_val += 50;
		}

		int32_t meas = true_val + prng_noise(3000);

		ARBITER_set_i32(&zctx, F_MEASUREMENT, meas);
		ARBITER_set_timestamp(&zctx, F_MEASUREMENT,
				    (uint32_t)(WARMUP_ITERATIONS + i) * 10);

		struct ARBITER_snapshot snap;
		struct ARBITER_result result;

		ARBITER_snapshot_begin(&zctx, &snap);
		ARBITER_eval(&ARBITER_generated_model, &snap, &result, NULL);
	}

	uint64_t t3_ns = bench_ns();
	uint64_t ARBITER_ns = t3_ns - t2_ns;
	uint64_t ARBITER_per_tick = ARBITER_ns / BENCH_ITERATIONS;

	LOG_INF("--- arbiter Engine Kalman ---");
	LOG_INF("  Total: %llu ns  (%llu ns/tick)", ARBITER_ns, ARBITER_per_tick);
	LOG_INF("  RAM (ctx): %u bytes", (unsigned)sizeof(struct ARBITER_ctx));
	LOG_INF("  Final estimate: %d (true: %d)",
		zctx.fact_values[F_X_EST].value, true_val);
	LOG_INF("  Model: %u facts, %u rules, %u exprs",
		ARBITER_generated_model.fact_count,
		ARBITER_generated_model.rule_count,
		ARBITER_generated_model.expr_count);

	/* ----- Comparison ----- */
	LOG_INF("=== Comparison ===");

	uint64_t overhead_pct = 0;

	if (hand_per_tick > 0) {
		overhead_pct = ((ARBITER_per_tick - hand_per_tick) * 100)
			       / hand_per_tick;
	}

	LOG_INF("  Overhead: %llu%% (%llu vs %llu ns/tick)",
		overhead_pct, ARBITER_per_tick, hand_per_tick);
	LOG_INF("  RAM delta: %u bytes",
		(unsigned)(sizeof(struct ARBITER_ctx) - sizeof(struct hand_kf)));
	LOG_INF("  ROM: compare .elf sizes (see README)");

	/* timing_stop() not needed — we're using k_uptime, not timing API */

	return 0;
}
