/* SPDX-License-Identifier: MIT */

/*
 * arbiter core evaluator — performance-critical hot path.
 *
 * Optimization techniques applied (all Zephyr-kosher):
 *   - __attribute__((hot, flatten)) on ARBITER_eval: compiler inlines
 *     every helper and optimizes the whole eval as a single function.
 *   - __attribute__((always_inline)) on all inner helpers: guarantees
 *     zero function-call overhead regardless of -O level.
 *   - __restrict on non-aliasing pointers: enables register reuse,
 *     eliminates redundant loads through aliased pointers.
 *   - likely()/unlikely() on all branches: helps CPU branch predictor
 *     and lets GCC lay out hot paths contiguously.
 *   - Branchless abs via XOR trick: eliminates branch misprediction
 *     in DELTA_GT/DELTA_LT (3 ALU ops vs. conditional branch).
 *   - Cached model-> pointers: avoid repeated indirection through
 *     the model struct on every iteration.
 *   - Local op counter: avoids pointer-aliased store on every
 *     operation; written back once at the end.
 *   - Targeted zero-init: writes only 6 result fields instead of
 *     full memset over CONFIG_ARBITER_MAX_ACTIONS_PER_EVAL * 2 bytes.
 *   - Trace-NULL fast path: skips record_trace call entirely when
 *     trace is NULL (common production deployment).
 *   - Switch case reordering: most-frequent opcodes first so the
 *     compiler's jump table or cascade falls through faster.
 *   - Single-condition special case: avoids loop setup for the most
 *     common case in safety-guard rules.
 *   - ARBITER_INDEX_MAX sentinel: profile-aware (uint8/uint16).
 */

#include <arbiter/arbiter.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(arbiter, CONFIG_ARBITER_LOG_LEVEL);

/* ── Compiler hints ───────────────────────────────────────────── */

#ifndef ARBITER_ALWAYS_INLINE
#define ARBITER_ALWAYS_INLINE static inline __attribute__((always_inline))
#endif

/* ── Operand resolution ───────────────────────────────────────── */

/**
 * Resolve an operand to a value.  ARBITER_INDEX_MAX => use literal.
 * Always inlined: compiles to a single conditional load (CMP + LDR).
 */
ARBITER_ALWAYS_INLINE int32_t resolve_operand(
	const struct ARBITER_fact_value *__restrict values,
	arbiter_index_t vcount,
	arbiter_index_t fact_id, int32_t literal)
{
	if (fact_id == ARBITER_INDEX_MAX) {
		return literal;
	}
	if (likely(fact_id < vcount)) {
		return values[fact_id].value;
	}
	return 0;
}

/* ── Condition evaluator ──────────────────────────────────────── */

/**
 * Evaluate one condition against cached fact values.
 * No pointer-to-pointer indirection -- values[] and timestamp are
 * passed directly so the compiler can keep them in registers.
 */
/* Per-condition hysteresis state bitmask (static — survives across evals). */
static uint32_t hyst_state[CONFIG_ARBITER_MAX_HYSTERESIS_CONDITIONS / 32 + 1];

ARBITER_ALWAYS_INLINE bool eval_condition(
	const struct ARBITER_condition_def *__restrict cond,
	const struct ARBITER_fact_value *__restrict values,
	arbiter_index_t vcount, uint32_t snap_ts,
	arbiter_index_t cond_index)
{
	if (unlikely(cond->fact_id >= vcount)) {
		return false;
	}

	const struct ARBITER_fact_value *__restrict fv =
		&values[cond->fact_id];
	const int32_t val = fv->value;

	switch (cond->op) {
	/* Comparison ops -- ordered by typical frequency */
	case ARBITER_OP_EQ:
		return val == cond->value;
	case ARBITER_OP_GT:
		return val > cond->value;
	case ARBITER_OP_LT:
		return val < cond->value;
	case ARBITER_OP_GE:
		return val >= cond->value;
	case ARBITER_OP_LE:
		return val <= cond->value;
	case ARBITER_OP_NE:
		return val != cond->value;

	/* Boolean / change ops */
	case ARBITER_OP_CHANGED:
		return fv->changed;

	/* Staleness ops -- collapsed branches */
	case ARBITER_OP_STALE:
		return (fv->timestamp_ms == 0) ||
		       ((snap_ts - fv->timestamp_ms) > (uint32_t)cond->value);
	case ARBITER_OP_NOT_STALE:
		return (fv->timestamp_ms != 0) &&
		       ((snap_ts - fv->timestamp_ms) <= (uint32_t)cond->value);

	/* Delta ops -- branchless abs via XOR trick:
	 *   mask = d >> 31          (all 1s if negative, 0 if positive)
	 *   abs  = (d ^ mask) - mask
	 * On Cortex-M: ASR+EOR+SUB = 3 cycles, zero branches.
	 */
	case ARBITER_OP_DELTA_GT: {
		int32_t d = val - fv->prev_value;
		int32_t mask = d >> 31;

		return ((d ^ mask) - mask) > cond->value;
	}
	case ARBITER_OP_DELTA_LT: {
		int32_t d = val - fv->prev_value;
		int32_t mask = d >> 31;

		return ((d ^ mask) - mask) < cond->value;
	}

	/* Set membership (v0: equality) */
	case ARBITER_OP_IN:
		return val == cond->value;
	case ARBITER_OP_NOT_IN:
		return val != cond->value;

	/* Hysteresis: rising = value, falling = aux_value.
	 * State persists in a static bitmask across evaluations.
	 */
	case ARBITER_OP_HYSTERESIS: {
		const int32_t rising = cond->value;
		const int32_t falling = cond->aux_value;
		bool prev_state = false;

		if (likely(cond_index <
			   CONFIG_ARBITER_MAX_HYSTERESIS_CONDITIONS)) {
			prev_state = (hyst_state[cond_index / 32] >>
				      (cond_index & 31)) & 1u;
		}

		bool result;

		if (val >= rising) {
			result = true;
		} else if (val <= falling) {
			result = false;
		} else {
			result = prev_state;
		}

		if (likely(cond_index <
			   CONFIG_ARBITER_MAX_HYSTERESIS_CONDITIONS)) {
			if (result) {
				hyst_state[cond_index / 32] |=
					(1u << (cond_index & 31));
			} else {
				hyst_state[cond_index / 32] &=
					~(1u << (cond_index & 31));
			}
		}
		return result;
	}

	default:
		return false;
	}
}

/**
 * Evaluate a condition group.  Special-cases count <= 1 to avoid
 * loop setup (most safety_guard rules have exactly 1 condition).
 */
ARBITER_ALWAYS_INLINE bool eval_condition_group(
	const struct ARBITER_condition_def *__restrict conds,
	const struct ARBITER_fact_value *__restrict values,
	arbiter_index_t vcount, uint32_t snap_ts,
	arbiter_index_t start, arbiter_index_t count)
{
	if (count == 0) {
		return true;
	}

	const enum ARBITER_cond_group group = conds[start].group;

	/* Fast path: single condition -- skip loop entirely */
	if (likely(count == 1)) {
		bool r = eval_condition(&conds[start], values,
					vcount, snap_ts, start);
		return (group == ARBITER_COND_NOT) ? !r : r;
	}

	/* ALL is the overwhelmingly common group type */
	if (likely(group == ARBITER_COND_ALL)) {
		for (arbiter_index_t i = 0; i < count; i++) {
			if (!eval_condition(&conds[start + i], values,
					    vcount, snap_ts,
					    start + i)) {
				return false;
			}
		}
		return true;
	}

	if (group == ARBITER_COND_ANY) {
		for (arbiter_index_t i = 0; i < count; i++) {
			if (eval_condition(&conds[start + i], values,
					   vcount, snap_ts,
					   start + i)) {
				return true;
			}
		}
		return false;
	}

	/* ARBITER_COND_NOT: invert single child */
	return !eval_condition(&conds[start], values, vcount, snap_ts, start);
}

/* ── Expression evaluator ─────────────────────────────────────── */

/**
 * Execute one compute expression.  Operates directly on the values
 * array -- no snapshot struct indirection in the inner loop.
 *
 * Switch cases ordered by frequency: ASSIGN and simple arithmetic
 * first (PID, Kalman models hit these 80%+ of the time).
 */
/**
 * Linear interpolation in a lookup table.
 * Clamps to table endpoints when input is outside range.
 */
ARBITER_ALWAYS_INLINE int32_t table_lookup(
	const struct ARBITER_table_def *__restrict tbl,
	int32_t input)
{
	if (unlikely(tbl == NULL || tbl->count == 0)) {
		return 0;
	}
	const uint16_t n = tbl->count;
	const int32_t *__restrict keys = tbl->keys;
	const int32_t *__restrict vals = tbl->values;

	/* Clamp below minimum */
	if (input <= keys[0]) {
		return vals[0];
	}
	/* Clamp above maximum */
	if (input >= keys[n - 1]) {
		return vals[n - 1];
	}
	/* Binary-ish scan for bracket (tables are small, linear is fine) */
	for (uint16_t i = 1; i < n; i++) {
		if (input <= keys[i]) {
			/* Linear interpolation between [i-1] and [i] */
			int32_t k0 = keys[i - 1];
			int32_t k1 = keys[i];
			int32_t v0 = vals[i - 1];
			int32_t v1 = vals[i];
			int32_t dk = k1 - k0;

			if (dk == 0) {
				return v0;
			}
			/* lerp: v0 + (v1-v0)*(input-k0)/(k1-k0) */
			int64_t num = (int64_t)(v1 - v0) *
				      (int64_t)(input - k0);
			return v0 + (int32_t)(num / dk);
		}
	}
	return vals[n - 1];
}

ARBITER_ALWAYS_INLINE void eval_expression(
	const struct ARBITER_expr_def *__restrict expr,
	struct ARBITER_fact_value *__restrict values,
	arbiter_index_t vcount,
	const struct ARBITER_model *__restrict model)
{
	const arbiter_index_t tid = expr->target_fact_id;

	if (unlikely(tid >= vcount)) {
		return;
	}

	const int32_t left = resolve_operand(values, vcount,
					     expr->left_fact_id,
					     expr->left_literal);
	const int32_t right = resolve_operand(values, vcount,
					      expr->right_fact_id,
					      expr->right_literal);
	int32_t result;

	switch (expr->op) {
	case ARBITER_EXPR_ASSIGN:
		result = left;
		break;
	case ARBITER_EXPR_ADD:
		result = left + right;
		break;
	case ARBITER_EXPR_SUB:
		result = left - right;
		break;
	case ARBITER_EXPR_MUL:
		result = left * right;
		break;
	case ARBITER_EXPR_SCALE: {
		int64_t w = (int64_t)left * (int64_t)right;

		if (expr->scale != 0) {
			w /= expr->scale;
		}
		result = (w > INT32_MAX) ? INT32_MAX :
			 (w < INT32_MIN) ? INT32_MIN : (int32_t)w;
		break;
	}
	case ARBITER_EXPR_ACCUMULATE: {
		int64_t w = (int64_t)left * (int64_t)right;

		if (expr->scale != 0) {
			w /= expr->scale;
		}
		int64_t acc = (int64_t)values[tid].value + w;

		result = (acc > INT32_MAX) ? INT32_MAX :
			 (acc < INT32_MIN) ? INT32_MIN : (int32_t)acc;
		break;
	}
	case ARBITER_EXPR_DIV:
		result = likely(right != 0) ? (left / right) : 0;
		break;
	case ARBITER_EXPR_MOD:
		result = likely(right != 0) ? (left % right) : 0;
		break;
	case ARBITER_EXPR_CLAMP:
		result = (left < right) ? right :
			 (left > expr->scale) ? expr->scale : left;
		break;
	case ARBITER_EXPR_MIN:
		result = (left < right) ? left : right;
		break;
	case ARBITER_EXPR_MAX:
		result = (left > right) ? left : right;
		break;
	case ARBITER_EXPR_ABS: {
		/* Branchless abs: same XOR trick as DELTA ops */
		int32_t mask = left >> 31;

		result = (left ^ mask) - mask;
		break;
	}
	case ARBITER_EXPR_NEGATE:
		result = -left;
		break;
	case ARBITER_EXPR_SHIFT_R:
		result = left >> (right & 31);
		break;
	case ARBITER_EXPR_SHIFT_L:
		result = left << (right & 31);
		break;
	case ARBITER_EXPR_LOOKUP: {
		/* scale field stores the table index */
		const uint16_t tbl_idx = (uint16_t)expr->scale;

		if (likely(model->tables != NULL &&
			   tbl_idx < model->table_count)) {
			result = table_lookup(
				&model->tables[tbl_idx], left);
		} else {
			result = 0;
		}
		break;
	}
	default:
		return;
	}

	values[tid].value = result;
	values[tid].valid = true;
}

/* ── Trace recording (cold path) ──────────────────────────────── */

/**
 * Record a trace entry.  Marked cold -- only reached when tracing
 * is enabled and should not pollute the icache of the hot eval loop.
 */
static __attribute__((cold, noinline)) void record_trace(
	struct ARBITER_trace *__restrict trace,
	const struct ARBITER_rule_def *__restrict rule,
	const struct ARBITER_condition_def *__restrict conds,
	arbiter_index_t cond_table_count,
	bool condition_result, arbiter_index_t action_id)
{
	struct ARBITER_trace_entry entry = {
		.rule_id = rule->id,
		.condition_result = condition_result,
		.action_id = action_id,
#if !defined(CONFIG_ARBITER_STRINGS) || CONFIG_ARBITER_STRINGS
		.reason = rule->explanation,
#endif
	};

	arbiter_index_t n = 0;
	const arbiter_index_t climit = rule->condition_count;

	for (arbiter_index_t i = 0;
	     i < climit && n < CONFIG_ARBITER_MAX_TRACE_INPUTS; i++) {
		arbiter_index_t ci = rule->condition_start + i;

		if (likely(ci < cond_table_count)) {
			entry.input_facts[n++] = conds[ci].fact_id;
		}
	}
	entry.input_fact_count = n;

	ARBITER_trace_record(trace, &entry);
}

/* ── Core evaluation loop ─────────────────────────────────────── */

/**
 * Deterministic model evaluation -- the hot path.
 *
 * __attribute__((hot)):    optimize aggressively, place in .text.hot
 *                          for instruction-cache locality.
 * __attribute__((flatten)): inline ALL callees so the compiler sees
 *                          the complete function and can schedule
 *                          registers, hoist invariants, and eliminate
 *                          dead stores across the entire eval.
 */
__attribute__((hot, flatten))
int ARBITER_eval(const struct ARBITER_model *model,
		 const struct ARBITER_snapshot *snapshot,
		 struct ARBITER_result *result,
		 struct ARBITER_trace *trace)
{
	if (unlikely(model == NULL || snapshot == NULL || result == NULL)) {
		return ARBITER_EINVAL;
	}

	if (unlikely(!snapshot->frozen)) {
		LOG_WRN("Snapshot is not frozen");
		return ARBITER_EINVAL;
	}

	/*
	 * Cache all model pointers into locals.  Eliminates repeated
	 * loads through model-> on every loop iteration.  __restrict
	 * tells the compiler these don't alias, enabling full
	 * register allocation and store-to-load forwarding.
	 */
	const struct ARBITER_rule_def *__restrict const rules =
		model->rules;
	const struct ARBITER_condition_def *__restrict const conds =
		model->conditions;
	const struct ARBITER_expr_def *__restrict const exprs =
		model->expressions;
	const struct ARBITER_action_def *__restrict const actions =
		model->actions;
	const arbiter_index_t rule_count = model->rule_count;
	const arbiter_index_t action_count = model->action_count;
	const arbiter_index_t expr_count = model->expr_count;
	const arbiter_index_t cond_table_count = model->condition_count;
	const bool has_exprs = (exprs != NULL);

	/*
	 * Mutable values pointer.  The snapshot is "frozen" for external
	 * readers but the compute engine writes derived facts during
	 * evaluation.  Canonical order guarantees determinism.
	 */
	struct ARBITER_fact_value *__restrict const values =
		((struct ARBITER_snapshot *)snapshot)->values;
	const arbiter_index_t vcount = snapshot->count;
	const uint32_t snap_ts = snapshot->timestamp_ms;

	/*
	 * Targeted zero-init instead of memset(result, 0, sizeof(*result)).
	 * sizeof(ARBITER_result) includes requested_actions[] array
	 * which can be up to 256 x 2 bytes on full profile.  We only
	 * zero the 6 scalar fields -- the array is written before read.
	 */
	result->current_mode = 0;
	result->previous_mode = 0;
	result->raised_faults = 0;
	result->requested_action_count = 0;
	result->eval_op_count = 0;
	result->status = ARBITER_OK;

	/*
	 * Local op counter -- avoids pointer-aliased store on every
	 * condition/expression evaluation.  Written back once.
	 */
	uint32_t ops = 0;

	for (arbiter_index_t r = 0; r < rule_count; r++) {
		const struct ARBITER_rule_def *__restrict rule = &rules[r];

		/* ── Conditions ──────────────────────────────── */
		const bool fired = eval_condition_group(
			conds, values, vcount, snap_ts,
			rule->condition_start, rule->condition_count);

		/* Batch-count: conditions + rule itself */
		ops += (uint32_t)rule->condition_count + 1u;

		if (fired) {
			/* ── Compute expressions ─────────────── */
			if (has_exprs && rule->expr_count > 0) {
				const arbiter_index_t es = rule->expr_start;
				const arbiter_index_t ec = rule->expr_count;

				for (arbiter_index_t i = 0; i < ec; i++) {
					const arbiter_index_t ei = es + i;

				if (likely(ei < expr_count)) {
					eval_expression(
						&exprs[ei],
						values, vcount,
						model);
				}
				}
				ops += ec;
			}

			/* ── Mode transition ─────────────────── */
			if (rule->set_mode != ARBITER_INDEX_MAX) {
				result->previous_mode =
					result->current_mode;
				result->current_mode = rule->set_mode;
			}

			/* ── Actions ─────────────────────────── */
			const arbiter_index_t ac = rule->action_count;
			const arbiter_index_t as = rule->action_start;

			for (arbiter_index_t a = 0; a < ac; a++) {
				const arbiter_index_t ai = as + a;

				if (unlikely(ai >= action_count)) {
					break;
				}
				if (unlikely(
					result->requested_action_count >=
					CONFIG_ARBITER_MAX_ACTIONS_PER_EVAL)) {
					break;
				}

				const struct ARBITER_action_def
					*__restrict act = &actions[ai];

				if (act->type ==
				    ARBITER_ACTION_RAISE_FAULT &&
				    act->target_fact_id < 32) {
					result->raised_faults |=
						BIT(act->target_fact_id);
				} else if (act->type ==
					   ARBITER_ACTION_CLEAR_FAULT &&
					   act->target_fact_id < 32) {
					result->raised_faults &=
						~BIT(act->target_fact_id);
				}

				result->requested_actions[
					result->requested_action_count++]
					= act->id;
			}
		}

		/*
		 * Trace: skip call entirely when trace is NULL.
		 * Production deployments run without trace, so this
		 * saves a function call + stack frame per rule per eval.
		 */
		if (unlikely(trace != NULL)) {
			const arbiter_index_t trace_act =
				(fired && rule->action_count > 0)
				? actions[rule->action_start].id
				: ARBITER_INDEX_MAX;

			record_trace(trace, rule, conds,
				     cond_table_count,
				     fired, trace_act);
		}
	}

	result->eval_op_count = ops;
	return ARBITER_OK;
}

/* ── Query helpers (not performance-critical) ─────────────────── */

int ARBITER_get_mode(const struct ARBITER_result *result, uint16_t *mode_id)
{
	if (unlikely(result == NULL || mode_id == NULL)) {
		return ARBITER_EINVAL;
	}
	*mode_id = result->current_mode;
	return ARBITER_OK;
}

int ARBITER_fault_is_raised(const struct ARBITER_result *result,
			    uint16_t fault_id)
{
	if (unlikely(result == NULL)) {
		return ARBITER_EINVAL;
	}
	if (unlikely(fault_id >= 32)) {
		return ARBITER_ERANGE;
	}
	return (result->raised_faults & BIT(fault_id)) ? 1 : 0;
}

int ARBITER_get_requested_actions(const struct ARBITER_result *result,
				  const uint16_t **actions, size_t *count)
{
	if (unlikely(result == NULL || actions == NULL || count == NULL)) {
		return ARBITER_EINVAL;
	}
	*actions = result->requested_actions;
	*count = result->requested_action_count;
	return ARBITER_OK;
}
