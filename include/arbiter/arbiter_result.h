/* SPDX-License-Identifier: MIT */

#ifndef ARBITER_RESULT_H_
#define ARBITER_RESULT_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_ARBITER_MAX_ACTIONS_PER_EVAL
#define CONFIG_ARBITER_MAX_ACTIONS_PER_EVAL 16
#endif

/** Runtime value of a single fact. */
struct ARBITER_fact_value {
	int32_t value;
	int32_t prev_value;
	uint32_t timestamp_ms;
	bool valid;
	bool changed;
};

/** Frozen fact snapshot for deterministic evaluation. */
struct ARBITER_snapshot {
	struct ARBITER_fact_value *values;
	uint16_t count;
	uint32_t timestamp_ms;
	bool frozen;
};

/** Evaluation result. */
struct ARBITER_result {
	uint16_t current_mode;
	uint16_t previous_mode;
	uint32_t raised_faults;
	uint16_t requested_actions[CONFIG_ARBITER_MAX_ACTIONS_PER_EVAL];
	uint16_t requested_action_count;
	uint32_t eval_op_count;
	int status;
};

#ifdef __cplusplus
}
#endif

#endif /* ARBITER_RESULT_H_ */
