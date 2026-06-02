/* SPDX-License-Identifier: MIT */

/**
 * @file arbiter_state.c
 * @brief State machine evaluator — guarded transitions with priority.
 *
 * Evaluates transitions from the current state using the model's
 * condition table.  Highest-priority enabled transition wins.
 * No dynamic allocation.
 */

#include <arbiter/arbiter_state.h>
#include <arbiter/arbiter.h>
#include <string.h>
#include <zephyr/kernel.h>

/* Forward-declare the condition group evaluator from arbiter_eval.c.
 * This is an internal helper — not part of the public API.
 * We reuse it to avoid duplicating condition evaluation logic.
 *
 * For a clean build the linker resolves this from the same library.
 * If needed, this could be extracted into a shared internal header.
 * For now we declare it extern here.
 */

/* ── Internal condition evaluation (simplified for state machine) ─ */

/**
 * Evaluate a range of conditions as an ALL-group.
 * Returns true if all conditions in [start, start+count) are satisfied.
 */
static bool eval_state_conditions(
	const struct ARBITER_condition_def *__restrict conds,
	arbiter_index_t cond_table_count,
	const struct ARBITER_fact_value *__restrict values,
	arbiter_index_t vcount, uint32_t snap_ts,
	arbiter_index_t start, arbiter_index_t count)
{
	if (count == 0) {
		return true;
	}

	for (arbiter_index_t i = 0; i < count; i++) {
		const arbiter_index_t ci = start + i;

		if (unlikely(ci >= cond_table_count)) {
			return false;
		}

		const struct ARBITER_condition_def *__restrict c = &conds[ci];

		if (unlikely(c->fact_id >= vcount)) {
			return false;
		}

		const struct ARBITER_fact_value *__restrict fv =
			&values[c->fact_id];
		const int32_t val = fv->value;
		bool ok;

		switch (c->op) {
		case ARBITER_OP_EQ:
			ok = (val == c->value);
			break;
		case ARBITER_OP_NE:
			ok = (val != c->value);
			break;
		case ARBITER_OP_LT:
			ok = (val < c->value);
			break;
		case ARBITER_OP_LE:
			ok = (val <= c->value);
			break;
		case ARBITER_OP_GT:
			ok = (val > c->value);
			break;
		case ARBITER_OP_GE:
			ok = (val >= c->value);
			break;
		case ARBITER_OP_CHANGED:
			ok = fv->changed;
			break;
		case ARBITER_OP_STALE:
			ok = (fv->timestamp_ms == 0) ||
			     ((snap_ts - fv->timestamp_ms) >
			      (uint32_t)c->value);
			break;
		case ARBITER_OP_NOT_STALE:
			ok = (fv->timestamp_ms != 0) &&
			     ((snap_ts - fv->timestamp_ms) <=
			      (uint32_t)c->value);
			break;
		case ARBITER_OP_IN:
			ok = (val == c->value);
			break;
		case ARBITER_OP_NOT_IN:
			ok = (val != c->value);
			break;
		case ARBITER_OP_DELTA_GT: {
			int32_t d = val - fv->prev_value;
			int32_t mask = d >> 31;

			ok = (((d ^ mask) - mask) > c->value);
			break;
		}
		case ARBITER_OP_DELTA_LT: {
			int32_t d = val - fv->prev_value;
			int32_t mask = d >> 31;

			ok = (((d ^ mask) - mask) < c->value);
			break;
		}
		default:
			ok = false;
			break;
		}

		if (!ok) {
			return false;
		}
	}

	return true;
}

/* ── Public API ───────────────────────────────────────────────── */

int ARBITER_state_eval(const struct ARBITER_model *model,
		       const struct ARBITER_state_def *states,
		       arbiter_index_t state_count,
		       const struct ARBITER_transition_def *transitions,
		       arbiter_index_t trans_count,
		       const struct ARBITER_snapshot *snapshot,
		       arbiter_index_t current_state,
		       struct ARBITER_state_result *result)
{
	if (unlikely(model == NULL || snapshot == NULL || result == NULL)) {
		return ARBITER_EINVAL;
	}
	if (unlikely(transitions == NULL && trans_count > 0)) {
		return ARBITER_EINVAL;
	}

	/* Default: no transition */
	result->next_state = current_state;
	result->transition_index = ARBITER_INDEX_MAX;
	result->on_exit_action = ARBITER_INDEX_MAX;
	result->on_enter_action = ARBITER_INDEX_MAX;
	result->transitioned = false;

	const struct ARBITER_condition_def *__restrict conds =
		model->conditions;
	const arbiter_index_t cond_table_count = model->condition_count;
	const struct ARBITER_fact_value *__restrict values =
		snapshot->values;
	const arbiter_index_t vcount = snapshot->count;
	const uint32_t snap_ts = snapshot->timestamp_ms;

	/*
	 * Find the highest-priority enabled transition from current_state.
	 * We iterate all transitions and track the best match.
	 */
	arbiter_index_t best_priority = 0;
	arbiter_index_t best_idx = ARBITER_INDEX_MAX;
	bool found = false;

	for (arbiter_index_t t = 0; t < trans_count; t++) {
		const struct ARBITER_transition_def *__restrict tr =
			&transitions[t];

		if (tr->source_state != current_state) {
			continue;
		}

		/* Check priority: higher wins, or first if equal */
		if (found && tr->priority < best_priority) {
			continue;
		}

		/* Evaluate conditions */
		bool conds_ok = eval_state_conditions(
			conds, cond_table_count, values, vcount, snap_ts,
			tr->condition_start, tr->condition_count);

		if (!conds_ok) {
			continue;
		}

		/* Evaluate guards */
		bool guards_ok = eval_state_conditions(
			conds, cond_table_count, values, vcount, snap_ts,
			tr->guard_start, tr->guard_count);

		if (!guards_ok) {
			continue;
		}

		/* This transition is enabled and highest priority so far */
		if (!found || tr->priority > best_priority) {
			best_priority = tr->priority;
			best_idx = t;
			found = true;
		}
	}

	if (found) {
		const struct ARBITER_transition_def *__restrict winner =
			&transitions[best_idx];

		result->next_state = winner->target_state;
		result->transition_index = best_idx;
		result->transitioned = true;

		/* Look up on_exit from current state */
		if (states != NULL && current_state < state_count) {
			result->on_exit_action =
				states[current_state].on_exit_action;
		}

		/* Look up on_enter for target state */
		if (states != NULL &&
		    winner->target_state < state_count) {
			result->on_enter_action =
				states[winner->target_state].on_enter_action;
		}
	}

	return ARBITER_OK;
}

int ARBITER_state_get(const struct ARBITER_ctx *ctx,
		      arbiter_index_t *state_id)
{
	if (unlikely(ctx == NULL || state_id == NULL)) {
		return ARBITER_EINVAL;
	}
	if (unlikely(!ctx->initialized)) {
		return ARBITER_EINVAL;
	}

	/*
	 * State is stored as the value of fact_values[0] when
	 * CONFIG_ARBITER_STATE_MACHINE is enabled.  This is the
	 * convention for v0 — a dedicated ctx field would be cleaner
	 * but would change the ABI.
	 */
	*state_id = (arbiter_index_t)ctx->fact_values[0].value;
	return ARBITER_OK;
}

int ARBITER_state_set(struct ARBITER_ctx *ctx, arbiter_index_t state_id)
{
	if (unlikely(ctx == NULL)) {
		return ARBITER_EINVAL;
	}
	if (unlikely(!ctx->initialized)) {
		return ARBITER_EINVAL;
	}

	ctx->fact_values[0].prev_value = ctx->fact_values[0].value;
	ctx->fact_values[0].value = (int32_t)state_id;
	ctx->fact_values[0].changed = true;
	ctx->fact_values[0].valid = true;

	return ARBITER_OK;
}
