/* SPDX-License-Identifier: MIT */

#ifndef ARBITER_STATE_H_
#define ARBITER_STATE_H_

/**
 * @defgroup arbiter_state State Machine Support (REQ-ARCH-039)
 * @ingroup arbiter
 * @{
 * @brief Explicit state machine with guarded transitions.
 *
 * A model may define a set of states and transitions.  Each transition
 * has a source state, a target state, a condition range (reusing the
 * condition table), an optional guard range, and a priority.  The
 * evaluator finds the highest-priority enabled transition from the
 * current state and returns the target.
 *
 * States can have on_enter / on_exit actions (indices into the action
 * table).  The caller is responsible for dispatching them.
 *
 * All storage is embedded in the compiled model — no dynamic allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <arbiter/arbiter_model.h>
#include <arbiter/arbiter_result.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_ARBITER_MAX_STATES
#define CONFIG_ARBITER_MAX_STATES 16
#endif

#ifndef CONFIG_ARBITER_MAX_TRANSITIONS
#define CONFIG_ARBITER_MAX_TRANSITIONS 32
#endif

/** State definition. */
struct ARBITER_state_def {
	arbiter_index_t id;                /**< State index. */
	arbiter_index_t on_enter_action;   /**< Action to run on entry (INDEX_MAX = none). */
	arbiter_index_t on_exit_action;    /**< Action to run on exit  (INDEX_MAX = none). */
#if !defined(CONFIG_ARBITER_STRINGS) || CONFIG_ARBITER_STRINGS
	const char *name;
#endif
};

/** Transition definition. */
struct ARBITER_transition_def {
	arbiter_index_t source_state;       /**< Source state index. */
	arbiter_index_t target_state;       /**< Target state index. */
	arbiter_index_t condition_start;    /**< Index into model conditions table. */
	arbiter_index_t condition_count;    /**< Number of conditions. */
	arbiter_index_t guard_start;        /**< Index into model conditions table (guards). */
	arbiter_index_t guard_count;        /**< Number of guard conditions. */
	arbiter_index_t priority;           /**< Higher = evaluated first. */
};

/** Result of a state evaluation. */
struct ARBITER_state_result {
	arbiter_index_t next_state;         /**< Target state (unchanged if no transition). */
	arbiter_index_t transition_index;   /**< Which transition fired (INDEX_MAX = none). */
	arbiter_index_t on_exit_action;     /**< Action from departing state (INDEX_MAX = none). */
	arbiter_index_t on_enter_action;    /**< Action from entering state (INDEX_MAX = none). */
	bool transitioned;                  /**< True if a transition fired. */
};

/**
 * @brief Evaluate state transitions from the current state.
 *
 * Iterates all transitions whose source_state matches current_state,
 * in priority order (highest first).  For each candidate, evaluates
 * its conditions and guards against the snapshot.  The first fully-
 * satisfied transition wins.
 *
 * @param model         Compiled model (must have state/transition tables).
 * @param states        State definition table.
 * @param state_count   Number of states.
 * @param transitions   Transition definition table.
 * @param trans_count   Number of transitions.
 * @param snapshot      Frozen fact snapshot.
 * @param current_state Current state index.
 * @param result        Output — populated with the winning transition.
 * @return ARBITER_OK on success, ARBITER_EINVAL on bad arguments.
 */
int ARBITER_state_eval(const struct ARBITER_model *model,
		       const struct ARBITER_state_def *states,
		       arbiter_index_t state_count,
		       const struct ARBITER_transition_def *transitions,
		       arbiter_index_t trans_count,
		       const struct ARBITER_snapshot *snapshot,
		       arbiter_index_t current_state,
		       struct ARBITER_state_result *result);

/**
 * @brief Get the current state stored in a context.
 *
 * State is stored in fact_values[0] by convention when the state
 * machine feature is active, but this accessor reads from a
 * dedicated field added by CONFIG_ARBITER_STATE_MACHINE.
 *
 * @param ctx       Initialized context.
 * @param state_id  Output state index.
 * @return ARBITER_OK or ARBITER_EINVAL.
 */
int ARBITER_state_get(const struct ARBITER_ctx *ctx,
		      arbiter_index_t *state_id);

/**
 * @brief Set the current state in a context.
 *
 * @param ctx      Initialized context.
 * @param state_id State index to set.
 * @return ARBITER_OK or ARBITER_EINVAL.
 */
int ARBITER_state_set(struct ARBITER_ctx *ctx, arbiter_index_t state_id);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ARBITER_STATE_H_ */
