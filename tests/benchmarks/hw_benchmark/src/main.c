/* SPDX-License-Identifier: MIT */

/**
 * Hardware PID Benchmark — cycle-accurate timing on Cortex-M
 *
 * On Cortex-M targets (nucleo_f446re, nucleo_h743zi) this uses the
 * Data Watchpoint and Trace (DWT) unit's CYCCNT register for cycle-exact
 * measurement.  On native_sim it falls back to Zephyr's timing API.
 *
 * Reuses the PID model and hand-coded baseline from pid_benchmark.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <arbiter/arbiter.h>
#include "arbiter_model.h"

LOG_MODULE_REGISTER(hw_bench, LOG_LEVEL_INF);

/* ------------------------------------------------------------------ */
/*  DWT cycle counter (Cortex-M only)                                 */
/* ------------------------------------------------------------------ */

#if defined(CONFIG_CPU_CORTEX_M)

/* CoreDebug and DWT registers — ARMv7-M / ARMv8-M */
#define DWT_CTRL_REG    (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT_REG  (*(volatile uint32_t *)0xE0001004)
#define DEM_CR_REG      (*(volatile uint32_t *)0xE000EDFC)
#define DEM_CR_TRCENA   (1UL << 24)
#define DWT_CTRL_CYCCNTENA (1UL)

static inline void dwt_init(void)
{
	DEM_CR_REG |= DEM_CR_TRCENA;
	DWT_CYCCNT_REG = 0;
	DWT_CTRL_REG |= DWT_CTRL_CYCCNTENA;
}

static inline uint32_t dwt_cycles(void)
{
	return DWT_CYCCNT_REG;
}

#define HW_TIMER_INIT()    dwt_init()
#define HW_TIMER_GET()     dwt_cycles()
#define HW_TIMER_UNIT      "cycles"

#else /* native_sim / other */

/* Fall back to clock_gettime for nanosecond resolution */
struct bench_ts { long tv_sec; long tv_nsec; };
extern int clock_gettime(int, struct bench_ts *);

static inline uint64_t native_ns(void)
{
	struct bench_ts ts;

	clock_gettime(4 /* CLOCK_MONOTONIC_RAW */, &ts);
	return (uint64_t)(unsigned long)ts.tv_sec * 1000000000ULL +
		(uint64_t)(unsigned long)ts.tv_nsec;
}

static uint64_t native_start;

#define HW_TIMER_INIT()    do { native_start = 0; } while (0)
#define HW_TIMER_GET()     ((uint32_t)(native_ns() & 0xFFFFFFFFUL))
#define HW_TIMER_UNIT      "ns"

#endif /* CONFIG_CPU_CORTEX_M */

/* ------------------------------------------------------------------ */
/*  Hand-coded PID (baseline) — same as pid_benchmark                 */
/* ------------------------------------------------------------------ */

struct hand_pid {
	int32_t kp;          /* x1000 */
	int32_t ki;          /* x1000 */
	int32_t kd;          /* x1000 */
	int32_t error_prev;
	int32_t i_term;
	int32_t output;
};

static void hand_pid_init(struct hand_pid *__restrict p)
{
	p->kp = 2500;
	p->ki = 100;
	p->kd = 800;
	p->error_prev = 0;
	p->i_term = 0;
	p->output = 0;
}

static void hand_pid_tick(struct hand_pid *__restrict p, int32_t setpoint,
			  int32_t process_value)
{
	int32_t error = setpoint - process_value;
	int32_t p_term = (int32_t)(((int64_t)error * p->kp) / 1000);

	p->i_term += (int32_t)(((int64_t)error * p->ki) / 10000);

	int32_t d_raw = error - p->error_prev;
	int32_t d_term = (int32_t)(((int64_t)d_raw * p->kd) / 1000);

	int32_t raw = p_term + p->i_term + d_term;

	if (raw > 1000) {
		raw = 1000;
	} else if (raw < -1000) {
		raw = -1000;
	}

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
/*  Fact index aliases                                                */
/* ------------------------------------------------------------------ */

#define F_KD            ARBITER_FACT_GAIN_KD
#define F_KI            ARBITER_FACT_GAIN_KI
#define F_KP            ARBITER_FACT_GAIN_KP
#define F_DT_MS         ARBITER_FACT_IN_DT_MS
#define F_ENABLE        ARBITER_FACT_IN_ENABLE
#define F_PROCESS_VALUE ARBITER_FACT_IN_PROCESS_VALUE
#define F_SENSOR_VALID  ARBITER_FACT_IN_SENSOR_VALID
#define F_SETPOINT      ARBITER_FACT_IN_SETPOINT
#define F_OUTPUT        ARBITER_FACT_PID_OUTPUT

/* ------------------------------------------------------------------ */
/*  Benchmark parameters                                              */
/* ------------------------------------------------------------------ */

#define BENCH_ITERATIONS  10000
#define WARMUP_ITERATIONS 500

static struct ARBITER_ctx arbiter_ctx;

void app_update_actuator(void)
{
	/* No-op for benchmark */
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */

int main(void)
{
	LOG_INF("=== HW PID Benchmark ===");
	LOG_INF("Timer unit: %s", HW_TIMER_UNIT);
	LOG_INF("Iterations: %d  (warmup: %d)", BENCH_ITERATIONS,
		WARMUP_ITERATIONS);

	HW_TIMER_INIT();

	/* ----- Hand-coded PID ----- */
	struct hand_pid hpid;

	hand_pid_init(&hpid);

	int32_t plant_h = 0;

	for (int i = 0; i < WARMUP_ITERATIONS; i++) {
		hand_pid_tick(&hpid, 10000, plant_h);
		plant_h += hpid.output / 10;
	}

	hand_pid_init(&hpid);
	plant_h = 0;

	uint32_t t0 = HW_TIMER_GET();

	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		hand_pid_tick(&hpid, 10000, plant_h);
		plant_h += hpid.output / 10;
	}

	uint32_t t1 = HW_TIMER_GET();
	uint32_t hand_total = t1 - t0;
	uint32_t hand_per_tick = hand_total / BENCH_ITERATIONS;

	LOG_INF("--- Hand-coded PID ---");
	LOG_INF("  Total: %u %s  (%u %s/tick)", hand_total, HW_TIMER_UNIT,
		hand_per_tick, HW_TIMER_UNIT);
	LOG_INF("  RAM (struct): %u bytes", (unsigned)sizeof(struct hand_pid));

	/* ----- arbiter engine PID ----- */
	int ret = ARBITER_init(&arbiter_ctx, &ARBITER_generated_model);

	if (ret != ARBITER_OK) {
		LOG_ERR("arbiter init failed: %d", ret);
		return ret;
	}

	ARBITER_set_i32(&arbiter_ctx, F_KP, 2500);
	ARBITER_set_i32(&arbiter_ctx, F_KI, 100);
	ARBITER_set_i32(&arbiter_ctx, F_KD, 800);
	ARBITER_set_bool(&arbiter_ctx, F_ENABLE, true);
	ARBITER_set_bool(&arbiter_ctx, F_SENSOR_VALID, true);
	ARBITER_set_u32(&arbiter_ctx, F_DT_MS, 10);

	int32_t plant_z = 0;

	for (int i = 0; i < WARMUP_ITERATIONS; i++) {
		ARBITER_set_i32(&arbiter_ctx, F_SETPOINT, 10000);
		ARBITER_set_i32(&arbiter_ctx, F_PROCESS_VALUE, plant_z);
		ARBITER_set_timestamp(&arbiter_ctx, F_PROCESS_VALUE,
				    (uint32_t)i * 10);

		struct ARBITER_snapshot snap;
		struct ARBITER_result result;

		ARBITER_snapshot_begin(&arbiter_ctx, &snap);
		ARBITER_eval(&ARBITER_generated_model, &snap, &result, NULL);
		plant_z += arbiter_ctx.fact_values[F_OUTPUT].value / 10;
	}

	ARBITER_init(&arbiter_ctx, &ARBITER_generated_model);
	ARBITER_set_i32(&arbiter_ctx, F_KP, 2500);
	ARBITER_set_i32(&arbiter_ctx, F_KI, 100);
	ARBITER_set_i32(&arbiter_ctx, F_KD, 800);
	ARBITER_set_bool(&arbiter_ctx, F_ENABLE, true);
	ARBITER_set_bool(&arbiter_ctx, F_SENSOR_VALID, true);
	ARBITER_set_u32(&arbiter_ctx, F_DT_MS, 10);
	plant_z = 0;

	uint32_t t2 = HW_TIMER_GET();

	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		ARBITER_set_i32(&arbiter_ctx, F_SETPOINT, 10000);
		ARBITER_set_i32(&arbiter_ctx, F_PROCESS_VALUE, plant_z);
		ARBITER_set_timestamp(&arbiter_ctx, F_PROCESS_VALUE,
				    (uint32_t)(WARMUP_ITERATIONS + i) * 10);

		struct ARBITER_snapshot snap;
		struct ARBITER_result result;

		ARBITER_snapshot_begin(&arbiter_ctx, &snap);
		ARBITER_eval(&ARBITER_generated_model, &snap, &result, NULL);
		plant_z += arbiter_ctx.fact_values[F_OUTPUT].value / 10;
	}

	uint32_t t3 = HW_TIMER_GET();
	uint32_t arb_total = t3 - t2;
	uint32_t arb_per_tick = arb_total / BENCH_ITERATIONS;

	LOG_INF("--- arbiter Engine PID ---");
	LOG_INF("  Total: %u %s  (%u %s/tick)", arb_total, HW_TIMER_UNIT,
		arb_per_tick, HW_TIMER_UNIT);
	LOG_INF("  RAM (ctx): %u bytes",
		(unsigned)sizeof(struct ARBITER_ctx));
	LOG_INF("  Model: %u facts, %u rules, %u conditions, %u expressions",
		ARBITER_generated_model.fact_count,
		ARBITER_generated_model.rule_count,
		ARBITER_generated_model.condition_count,
		ARBITER_generated_model.expr_count);

	/* ----- Comparison ----- */
	uint32_t overhead_pct = 0;

	if (hand_per_tick > 0) {
		overhead_pct = ((arb_per_tick - hand_per_tick) * 100)
			       / hand_per_tick;
	}

	LOG_INF("=== Comparison ===");
	LOG_INF("  Engine overhead: %u%% (%u vs %u %s/tick)",
		overhead_pct, arb_per_tick, hand_per_tick, HW_TIMER_UNIT);
	LOG_INF("  RAM overhead: %u bytes (ctx) vs %u bytes (struct)",
		(unsigned)sizeof(struct ARBITER_ctx),
		(unsigned)sizeof(struct hand_pid));
	LOG_INF("HW Benchmark complete");

	return 0;
}
