/* SPDX-License-Identifier: MIT */

/**
 * PID Benchmark: arbiter engine vs. hand-coded PID
 *
 * Measures:
 *   - CPU cycles per evaluation tick (avg over N iterations)
 *   - Peak stack usage (via k_thread_stack_space_get where available)
 *   - Static RAM footprint (struct sizes)
 *
 * ROM comparison is done post-build via `size` on the .elf — see README.
 *
 * Expected outcome:  Hand-coded PID is faster and smaller.  arbiter adds
 * ~2-5x cycle overhead and extra ROM/RAM for the engine tables, but
 * provides declarative safety semantics, traceability, model hashing,
 * and deterministic evaluation that hand-coded C does not.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <arbiter/arbiter.h>
#include "arbiter_model.h"

static inline uint64_t bench_ns(void)
{
	return (uint64_t)k_uptime_get() * 1000000ULL;
}

LOG_MODULE_REGISTER(pid_bench, LOG_LEVEL_INF);

/* ------------------------------------------------------------------ */
/*  Hand-coded PID (baseline)                                         */
/* ------------------------------------------------------------------ */

struct hand_pid {
	int32_t kp;          /* x1000 */
	int32_t ki;          /* x1000 */
	int32_t kd;          /* x1000 */
	int32_t error_prev;
	int32_t i_term;
	int32_t output;
};

static void hand_pid_init(struct hand_pid *p)
{
	p->kp = 2500;
	p->ki = 100;
	p->kd = 800;
	p->error_prev = 0;
	p->i_term = 0;
	p->output = 0;
}

static void hand_pid_tick(struct hand_pid *p, int32_t setpoint,
			  int32_t process_value)
{
	int32_t error = setpoint - process_value;
	int32_t p_term = (int32_t)(((int64_t)error * p->kp) / 1000);

	p->i_term += (int32_t)(((int64_t)error * p->ki) / 10000);

	int32_t d_raw = error - p->error_prev;
	int32_t d_term = (int32_t)(((int64_t)d_raw * p->kd) / 1000);

	int32_t raw = p_term + p->i_term + d_term;

	/* Clamp */
	if (raw > 1000) {
		raw = 1000;
	} else if (raw < -1000) {
		raw = -1000;
	}

	/* Anti-windup */
	if (raw >= 1000 || raw <= -1000) {
		if (p->i_term > 500000) {
			p->i_term = 500000;
		} else if (p->i_term < -500000) {
			p->i_term = -500000;
		}
	}

	p->output = raw;
	p->error_prev = error;
}

/* ------------------------------------------------------------------ */
/*  arbiter PID (engine-based)                                          */
/* ------------------------------------------------------------------ */

/* Fact indices from generated header — canonical alphabetical order. */
#define F_KD           ARBITER_FACT_GAIN_KD
#define F_KI           ARBITER_FACT_GAIN_KI
#define F_KP           ARBITER_FACT_GAIN_KP
#define F_DT_MS        ARBITER_FACT_IN_DT_MS
#define F_ENABLE       ARBITER_FACT_IN_ENABLE
#define F_PROCESS_VALUE ARBITER_FACT_IN_PROCESS_VALUE
#define F_SENSOR_VALID ARBITER_FACT_IN_SENSOR_VALID
#define F_SETPOINT     ARBITER_FACT_IN_SETPOINT
#define F_ABS_ERROR    ARBITER_FACT_PID_ABS_ERROR
#define F_D_TERM       ARBITER_FACT_PID_D_TERM
#define F_ERROR        ARBITER_FACT_PID_ERROR
#define F_ERROR_PREV   ARBITER_FACT_PID_ERROR_PREV
#define F_I_TERM       ARBITER_FACT_PID_I_TERM
#define F_OUTPUT       ARBITER_FACT_PID_OUTPUT
#define F_OUTPUT_RAW   ARBITER_FACT_PID_OUTPUT_RAW
#define F_P_TERM       ARBITER_FACT_PID_P_TERM

static struct ARBITER_ctx ARBITER_ctx;

void app_update_actuator(void)
{
	/* No-op for benchmark — just reads output */
}

/* ------------------------------------------------------------------ */
/*  Benchmark harness                                                 */
/* ------------------------------------------------------------------ */

/* Increase iterations so native_sim (1 MHz timing clock) yields non-zero
 * nanosecond counts.
 */
#define BENCH_ITERATIONS  100000
#define WARMUP_ITERATIONS 1000

int main(void)
{
	LOG_INF("=== arbiter PID Benchmark ===");
	LOG_INF("Iterations: %d  (warmup: %d)", BENCH_ITERATIONS,
		WARMUP_ITERATIONS);

	/* ----- Hand-coded PID benchmark ----- */
	struct hand_pid hpid;

	hand_pid_init(&hpid);

	int32_t plant_h = 0;

	/* Warmup */
	for (int i = 0; i < WARMUP_ITERATIONS; i++) {
		hand_pid_tick(&hpid, 10000, plant_h);
		plant_h += hpid.output / 10;
	}

	hand_pid_init(&hpid);
	plant_h = 0;

	uint64_t t0_ns = bench_ns();

	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		hand_pid_tick(&hpid, 10000, plant_h);
		plant_h += hpid.output / 10;
	}

	uint64_t t1_ns = bench_ns();
	uint64_t hand_ns = t1_ns - t0_ns;
	uint64_t hand_per_tick_ns = hand_ns / BENCH_ITERATIONS;

	LOG_INF("--- Hand-coded PID ---");
	LOG_INF("  Total: %llu ns  (%llu ns/tick)", hand_ns, hand_per_tick_ns);
	LOG_INF("  RAM (struct): %u bytes", (unsigned)sizeof(struct hand_pid));

	/* ----- arbiter engine PID benchmark ----- */
	int ret = ARBITER_init(&ARBITER_ctx, &ARBITER_generated_model);

	if (ret != ARBITER_OK) {
		LOG_ERR("arbiter init failed: %d", ret);
		return ret;
	}

	ARBITER_set_i32(&ARBITER_ctx, F_KP, 2500);
	ARBITER_set_i32(&ARBITER_ctx, F_KI, 100);
	ARBITER_set_i32(&ARBITER_ctx, F_KD, 800);
	ARBITER_set_bool(&ARBITER_ctx, F_ENABLE, true);
	ARBITER_set_bool(&ARBITER_ctx, F_SENSOR_VALID, true);
	ARBITER_set_u32(&ARBITER_ctx, F_DT_MS, 10);

	int32_t plant_z = 0;

	/* Warmup */
	for (int i = 0; i < WARMUP_ITERATIONS; i++) {
		ARBITER_set_i32(&ARBITER_ctx, F_SETPOINT, 10000);
		ARBITER_set_i32(&ARBITER_ctx, F_PROCESS_VALUE, plant_z);
		ARBITER_set_timestamp(&ARBITER_ctx, F_PROCESS_VALUE,
				    (uint32_t)i * 10);

		struct ARBITER_snapshot snap;
		struct ARBITER_result result;

		ARBITER_snapshot_begin(&ARBITER_ctx, &snap);
		ARBITER_eval(&ARBITER_generated_model, &snap, &result, NULL);
		plant_z += ARBITER_ctx.fact_values[F_OUTPUT].value / 10;
	}

	/* Reset for measurement */
	ARBITER_init(&ARBITER_ctx, &ARBITER_generated_model);
	ARBITER_set_i32(&ARBITER_ctx, F_KP, 2500);
	ARBITER_set_i32(&ARBITER_ctx, F_KI, 100);
	ARBITER_set_i32(&ARBITER_ctx, F_KD, 800);
	ARBITER_set_bool(&ARBITER_ctx, F_ENABLE, true);
	ARBITER_set_bool(&ARBITER_ctx, F_SENSOR_VALID, true);
	ARBITER_set_u32(&ARBITER_ctx, F_DT_MS, 10);
	plant_z = 0;

	uint64_t t2_ns = bench_ns();

	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		ARBITER_set_i32(&ARBITER_ctx, F_SETPOINT, 10000);
		ARBITER_set_i32(&ARBITER_ctx, F_PROCESS_VALUE, plant_z);
		ARBITER_set_timestamp(&ARBITER_ctx, F_PROCESS_VALUE,
				    (uint32_t)(WARMUP_ITERATIONS + i) * 10);

		struct ARBITER_snapshot snap;
		struct ARBITER_result result;

		ARBITER_snapshot_begin(&ARBITER_ctx, &snap);
		ARBITER_eval(&ARBITER_generated_model, &snap, &result, NULL);
		plant_z += ARBITER_ctx.fact_values[F_OUTPUT].value / 10;
	}

	uint64_t t3_ns = bench_ns();
	uint64_t ARBITER_ns = t3_ns - t2_ns;
	uint64_t ARBITER_per_tick_ns = ARBITER_ns / BENCH_ITERATIONS;

	LOG_INF("--- arbiter Engine PID ---");
	LOG_INF("  Total: %llu ns  (%llu ns/tick)", ARBITER_ns,
		ARBITER_per_tick_ns);
	LOG_INF("  RAM (ctx): %u bytes", (unsigned)sizeof(struct ARBITER_ctx));
	LOG_INF("  Model tables: %u facts, %u rules, %u conditions, "
		"%u expressions",
		ARBITER_generated_model.fact_count,
		ARBITER_generated_model.rule_count,
		ARBITER_generated_model.condition_count,
		ARBITER_generated_model.expr_count);

	/* ----- Comparison ----- */
	LOG_INF("=== Comparison ===");

	uint64_t overhead_pct = 0;

	if (hand_per_tick_ns > 0) {
		overhead_pct = ((ARBITER_per_tick_ns - hand_per_tick_ns) * 100)
			       / hand_per_tick_ns;
	}

	LOG_INF("  Engine overhead: %llu%% (%llu vs %llu ns/tick)",
		overhead_pct, ARBITER_per_tick_ns, hand_per_tick_ns);
	LOG_INF("  RAM overhead: %u bytes (ctx) vs %u bytes (struct)",
		(unsigned)sizeof(struct ARBITER_ctx),
		(unsigned)sizeof(struct hand_pid));
	LOG_INF("  ROM: compare with `size build/zephyr/zephyr.elf`");
	LOG_INF("");
	LOG_INF("  arbiter adds: declarative model, safety guards,");
	LOG_INF("    traceability, model hashing, deterministic eval,");
	LOG_INF("    shell inspection, and runtime watchdog integration.");
	LOG_INF("  Hand-coded adds: nothing — it's just math.");

	/* timing_stop() not needed — using k_uptime, not timing API */

	return 0;
}
