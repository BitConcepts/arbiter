/* SPDX-License-Identifier: MIT */

#include <arbiter/arbiter.h>
#include <zephyr/kernel.h>
#include <zephyr/stats/stats.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(arbiter, CONFIG_ARBITER_LOG_LEVEL);

/* ── STATS section definition ─────────────────────────────────── */

STATS_SECT_START(arbiter_stats)
STATS_SECT_ENTRY(eval_count)
STATS_SECT_ENTRY(eval_latency_us)
STATS_SECT_ENTRY(eval_max_latency_us)
STATS_SECT_ENTRY(rules_fired)
STATS_SECT_ENTRY(faults_active)
STATS_SECT_ENTRY(op_count_last)
STATS_SECT_END;

STATS_NAME_START(arbiter_stats)
STATS_NAME(arbiter_stats, eval_count)
STATS_NAME(arbiter_stats, eval_latency_us)
STATS_NAME(arbiter_stats, eval_max_latency_us)
STATS_NAME(arbiter_stats, rules_fired)
STATS_NAME(arbiter_stats, faults_active)
STATS_NAME(arbiter_stats, op_count_last)
STATS_NAME_END(arbiter_stats);

STATS_SECT_DECL(arbiter_stats) arbiter_stats_inst;

/* ── Helpers ──────────────────────────────────────────────────── */

/**
 * Count the number of bits set in a 32-bit word (fault bitmap).
 */
static uint32_t popcount32(uint32_t v)
{
	uint32_t c = 0;

	for (; v; v &= v - 1) {
		c++;
	}
	return c;
}

/* ── Public API ───────────────────────────────────────────────── */

/**
 * @brief Update arbiter metrics after an evaluation.
 *
 * @param result     Evaluation result.
 * @param latency_us Wall-clock evaluation latency in microseconds.
 */
void arbiter_metrics_update(const struct ARBITER_result *__restrict result,
			    uint32_t latency_us)
{
	if (result == NULL) {
		return;
	}

	STATS_INC(arbiter_stats_inst, eval_count);
	STATS_SET(arbiter_stats_inst, eval_latency_us, latency_us);

	if (latency_us > arbiter_stats_inst.eval_max_latency_us) {
		STATS_SET(arbiter_stats_inst, eval_max_latency_us, latency_us);
	}

	STATS_SET(arbiter_stats_inst, rules_fired,
		  result->requested_action_count);
	STATS_SET(arbiter_stats_inst, faults_active,
		  popcount32(result->raised_faults));
	STATS_SET(arbiter_stats_inst, op_count_last, result->eval_op_count);
}

/**
 * @brief Register arbiter stats with the Zephyr stats subsystem.
 *
 * Called automatically at boot via SYS_INIT.
 */
static int arbiter_metrics_init(void)
{
	int ret = stats_init(&arbiter_stats_inst.s_hdr,
			     STATS_SIZE_INIT_PARMS(arbiter_stats_inst,
						   STATS_SECT_DECL(arbiter_stats)));

	if (ret < 0) {
		LOG_ERR("stats_init failed: %d", ret);
		return ret;
	}

	ret = stats_register("arbiter", &arbiter_stats_inst.s_hdr);
	if (ret < 0) {
		LOG_ERR("stats_register failed: %d", ret);
		return ret;
	}

	LOG_INF("arbiter metrics registered");
	return 0;
}

SYS_INIT(arbiter_metrics_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
