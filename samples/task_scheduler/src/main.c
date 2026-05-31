/* SPDX-License-Identifier: MIT */

/**
 * Task Scheduler Solver Sample
 *
 * arbiter reasons about 4 task slots, computing urgency scores from
 * priority and deadline proximity.  The engine selects the highest-
 * urgency ready task and detects deadline misses.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <arbiter/arbiter.h>

LOG_MODULE_REGISTER(task_sched, LOG_LEVEL_INF);

extern const struct ARBITER_model ARBITER_generated_model;

static struct ARBITER_ctx ctx;

/* Fact indices (canonical order) */
enum {
	F_TICK = 0,
	F_ENABLE,
	F_BEST_URGENCY,
	F_SELECTED,
	/* task0 block */
	F_T0_READY, F_T0_PRIO, F_T0_DEADLINE, F_T0_SLACK, F_T0_URGENCY,
	/* task1 block */
	F_T1_READY, F_T1_PRIO, F_T1_DEADLINE, F_T1_SLACK, F_T1_URGENCY,
	/* task2 block */
	F_T2_READY, F_T2_PRIO, F_T2_DEADLINE, F_T2_SLACK, F_T2_URGENCY,
	/* task3 block */
	F_T3_READY, F_T3_PRIO, F_T3_DEADLINE, F_T3_SLACK, F_T3_URGENCY,
};

static const char *task_names[] = {"sensor_read", "log_flush",
				   "comms_tx", "watchdog_kick"};

void app_dispatch_task(void)
{
	int32_t sel = ctx.fact_values[F_SELECTED].value;

	if (sel >= 0 && sel <= 3) {
		LOG_INF("  >> DISPATCH: %s (urgency=%d)",
			task_names[sel],
			ctx.fact_values[F_BEST_URGENCY].value);
	}
}

void app_report_deadline_miss(void)
{
	LOG_WRN("  !! DEADLINE MISS detected");
}

int main(void)
{
	LOG_INF("=== arbiter Task Scheduler Solver ===");

	int ret = ARBITER_init(&ctx, &ARBITER_generated_model);

	if (ret != ARBITER_OK) {
		LOG_ERR("Init failed: %d", ret);
		return ret;
	}

	ARBITER_set_bool(&ctx, F_ENABLE, true);

	/* Configure 4 tasks with varying priorities and deadlines */
	/* Task 0: sensor_read — high priority, tight deadline */
	ARBITER_set_bool(&ctx, F_T0_READY, true);
	ARBITER_set_i32(&ctx, F_T0_PRIO, 90);
	ARBITER_set_u32(&ctx, F_T0_DEADLINE, 500);

	/* Task 1: log_flush — low priority, relaxed deadline */
	ARBITER_set_bool(&ctx, F_T1_READY, true);
	ARBITER_set_i32(&ctx, F_T1_PRIO, 20);
	ARBITER_set_u32(&ctx, F_T1_DEADLINE, 5000);

	/* Task 2: comms_tx — medium priority, medium deadline */
	ARBITER_set_bool(&ctx, F_T2_READY, true);
	ARBITER_set_i32(&ctx, F_T2_PRIO, 60);
	ARBITER_set_u32(&ctx, F_T2_DEADLINE, 1000);

	/* Task 3: watchdog_kick — critical priority, short deadline */
	ARBITER_set_bool(&ctx, F_T3_READY, true);
	ARBITER_set_i32(&ctx, F_T3_PRIO, 99);
	ARBITER_set_u32(&ctx, F_T3_DEADLINE, 200);

	for (uint32_t tick = 0; tick < 600; tick += 50) {
		ARBITER_set_u32(&ctx, F_TICK, tick);

		struct ARBITER_snapshot snap;
		struct ARBITER_result result;

		ARBITER_snapshot_begin(&ctx, &snap);
		ARBITER_eval(&ARBITER_generated_model, &snap, &result, NULL);

		LOG_INF("tick=%u  selected=%d  urgency=%d",
			tick,
			ctx.fact_values[F_SELECTED].value,
			ctx.fact_values[F_BEST_URGENCY].value);

		k_sleep(K_MSEC(10));
	}

	return 0;
}
