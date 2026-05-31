/* SPDX-License-Identifier: MIT */

#include <arbiter/arbiter.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(arbiter, CONFIG_ARBITER_LOG_LEVEL);

/**
 * Evaluate a single condition against the frozen snapshot.
 * Returns true if the condition is satisfied, false otherwise.
 * Increments *op_count for each operation performed.
 */
static bool eval_condition(const struct ARBITER_condition_def *cond,
			   const struct ARBITER_snapshot *snapshot,
			   uint32_t *op_count)
{
	(*op_count)++;

	if (cond->fact_id >= snapshot->count) {
		return false;
	}

	const struct ARBITER_fact_value *fv = &snapshot->values[cond->fact_id];

	switch (cond->op) {
	case ARBITER_OP_EQ:
		return fv->value == cond->value;
	case ARBITER_OP_NE:
		return fv->value != cond->value;
	case ARBITER_OP_LT:
		return fv->value < cond->value;
	case ARBITER_OP_LE:
		return fv->value <= cond->value;
	case ARBITER_OP_GT:
		return fv->value > cond->value;
	case ARBITER_OP_GE:
		return fv->value >= cond->value;
	case ARBITER_OP_STALE:
		if (fv->timestamp_ms == 0) {
			return true; /* Never updated = stale */
		}
		return (snapshot->timestamp_ms - fv->timestamp_ms) >
		       (uint32_t)cond->value;
	case ARBITER_OP_NOT_STALE:
		if (fv->timestamp_ms == 0) {
			return false;
		}
		return (snapshot->timestamp_ms - fv->timestamp_ms) <=
		       (uint32_t)cond->value;
	case ARBITER_OP_CHANGED:
		return fv->changed;
	case ARBITER_OP_DELTA_GT: {
		int32_t delta = fv->value - fv->prev_value;

		if (delta < 0) {
			delta = -delta;
		}
		return delta > cond->value;
	}
	case ARBITER_OP_DELTA_LT: {
		int32_t delta = fv->value - fv->prev_value;

		if (delta < 0) {
			delta = -delta;
		}
		return delta < cond->value;
	}
	case ARBITER_OP_IN:
	case ARBITER_OP_NOT_IN:
		/* IN/NOT_IN require set membership; simplified to equality
		 * against the stored value for v0. Full set support is a
		 * future extension.
		 */
		if (cond->op == ARBITER_OP_IN) {
			return fv->value == cond->value;
		}
		return fv->value != cond->value;
	default:
		return false;
	}
}

/**
 * Evaluate a group of conditions (ALL, ANY, NOT) for a rule.
 * Conditions for this rule span [start, start+count) in the model's
 * conditions table. All conditions in the group share the same group type.
 *
 * For ALL: short-circuit on first false.
 * For ANY: short-circuit on first true.
 * For NOT: invert the single child condition.
 */
static bool eval_condition_group(const struct ARBITER_model *model,
				 const struct ARBITER_snapshot *snapshot,
				 uint16_t start, uint16_t count,
				 uint32_t *op_count)
{
	if (count == 0) {
		return true; /* No conditions = vacuously true */
	}

	/* Determine group type from the first condition */
	enum ARBITER_cond_group group = model->conditions[start].group;

	switch (group) {
	case ARBITER_COND_ALL:
		for (uint16_t i = 0; i < count; i++) {
			if (!eval_condition(&model->conditions[start + i],
					    snapshot, op_count)) {
				return false; /* Short-circuit */
			}
		}
		return true;

	case ARBITER_COND_ANY:
		for (uint16_t i = 0; i < count; i++) {
			if (eval_condition(&model->conditions[start + i],
					   snapshot, op_count)) {
				return true; /* Short-circuit */
			}
		}
		return false;

	case ARBITER_COND_NOT:
		/* NOT inverts a single child */
		return !eval_condition(&model->conditions[start],
				       snapshot, op_count);

	default:
		return false;
	}
}

/**
 * Resolve an operand: read from a fact or use a literal.
 */
static int32_t resolve_operand(const struct ARBITER_snapshot *snapshot,
			       uint16_t fact_id, int32_t literal)
{
	if (fact_id == UINT16_MAX) {
		return literal;
	}
	if (fact_id < snapshot->count) {
		return snapshot->values[fact_id].value;
	}
	return 0;
}

/**
 * Execute a single compute expression, writing the result into the snapshot.
 * All arithmetic is 32-bit signed integer. Overflow is clamped.
 * Division by zero produces 0.
 */
static void eval_expression(const struct ARBITER_expr_def *expr,
			    struct ARBITER_snapshot *snapshot,
			    uint32_t *op_count)
{
	(*op_count)++;

	if (expr->target_fact_id >= snapshot->count) {
		return;
	}

	int32_t left = resolve_operand(snapshot, expr->left_fact_id,
				       expr->left_literal);
	int32_t right = resolve_operand(snapshot, expr->right_fact_id,
					expr->right_literal);
	int32_t result = 0;

	switch (expr->op) {
	case ARBITER_EXPR_ADD:
		result = left + right;
		break;
	case ARBITER_EXPR_SUB:
		result = left - right;
		break;
	case ARBITER_EXPR_MUL:
		result = left * right;
		break;
	case ARBITER_EXPR_DIV:
		result = (right != 0) ? (left / right) : 0;
		break;
	case ARBITER_EXPR_MOD:
		result = (right != 0) ? (left % right) : 0;
		break;
	case ARBITER_EXPR_ABS:
		result = (left < 0) ? -left : left;
		break;
	case ARBITER_EXPR_NEGATE:
		result = -left;
		break;
	case ARBITER_EXPR_MIN:
		result = (left < right) ? left : right;
		break;
	case ARBITER_EXPR_MAX:
		result = (left > right) ? left : right;
		break;
	case ARBITER_EXPR_CLAMP:
		/* left=value, right=lo, scale=hi */
		if (left < right) {
			result = right;
		} else if (left > expr->scale) {
			result = expr->scale;
		} else {
			result = left;
		}
		break;
	case ARBITER_EXPR_SHIFT_R:
		result = left >> (right & 31);
		break;
	case ARBITER_EXPR_SHIFT_L:
		result = left << (right & 31);
		break;
	case ARBITER_EXPR_SCALE: {
		/* Fixed-point multiply: (left * right) / scale */
		int64_t wide = (int64_t)left * (int64_t)right;

		if (expr->scale != 0) {
			wide /= expr->scale;
		}
		/* Saturate to int32 */
		if (wide > INT32_MAX) {
			result = INT32_MAX;
		} else if (wide < INT32_MIN) {
			result = INT32_MIN;
		} else {
			result = (int32_t)wide;
		}
		break;
	}
	case ARBITER_EXPR_ASSIGN:
		result = left;
		break;
	case ARBITER_EXPR_ACCUMULATE: {
		/* target += (left * right) / scale */
		int32_t current = snapshot->values[expr->target_fact_id].value;
		int64_t wide = (int64_t)left * (int64_t)right;

		if (expr->scale != 0) {
			wide /= expr->scale;
		}
		int64_t acc = (int64_t)current + wide;

		if (acc > INT32_MAX) {
			result = INT32_MAX;
		} else if (acc < INT32_MIN) {
			result = INT32_MIN;
		} else {
			result = (int32_t)acc;
		}
		break;
	}
	default:
		return;
	}

	snapshot->values[expr->target_fact_id].value = result;
	snapshot->values[expr->target_fact_id].valid = true;
}

/**
 * Execute a sequence of compute expressions for a fired rule.
 */
static void eval_expressions(const struct ARBITER_model *model,
			     struct ARBITER_snapshot *snapshot,
			     uint16_t start, uint16_t count,
			     uint32_t *op_count)
{
	if (model->expressions == NULL || count == 0) {
		return;
	}

	for (uint16_t i = 0; i < count; i++) {
		uint16_t ei = start + i;

		if (ei < model->expr_count) {
			eval_expression(&model->expressions[ei],
					snapshot, op_count);
		}
	}
}

/**
 * Record a trace entry for a rule evaluation.
 */
static void record_trace(struct ARBITER_trace *trace,
			 const struct ARBITER_rule_def *rule,
			 const struct ARBITER_model *model,
			 bool condition_result,
			 uint16_t action_id)
{
	if (trace == NULL) {
		return;
	}

	struct ARBITER_trace_entry entry;

	memset(&entry, 0, sizeof(entry));
	entry.rule_id = rule->id;
	entry.condition_result = condition_result;
	entry.action_id = action_id;
	entry.reason = rule->explanation;

	/* Record input fact IDs from the rule's conditions */
	uint16_t input_count = 0;

	for (uint16_t i = 0; i < rule->condition_count &&
	     input_count < CONFIG_ARBITER_MAX_TRACE_INPUTS; i++) {
		uint16_t ci = rule->condition_start + i;

		if (ci < model->condition_count) {
			entry.input_facts[input_count] =
				model->conditions[ci].fact_id;
			input_count++;
		}
	}
	entry.input_fact_count = input_count;

	ARBITER_trace_record(trace, &entry);
}

int ARBITER_eval(const struct ARBITER_model *model,
	       const struct ARBITER_snapshot *snapshot,
	       struct ARBITER_result *result,
	       struct ARBITER_trace *trace)
{
	if (model == NULL || snapshot == NULL || result == NULL) {
		return ARBITER_EINVAL;
	}

	if (!snapshot->frozen) {
		LOG_WRN("Snapshot is not frozen");
		return ARBITER_EINVAL;
	}

	/*
	 * The snapshot is "frozen" for external reads, but the compute
	 * engine writes derived facts into it during evaluation. This
	 * is safe because evaluation order is canonical and each
	 * expression reads only facts that were set before it.
	 *
	 * We cast away const for the mutable snapshot used internally.
	 */
	struct ARBITER_snapshot *snap_mut = (struct ARBITER_snapshot *)snapshot;

	/* Initialize result */
	memset(result, 0, sizeof(*result));
	result->status = ARBITER_OK;

	uint32_t op_count = 0;

	/* Evaluate rules in canonical order (array index order) */
	for (uint16_t r = 0; r < model->rule_count; r++) {
		const struct ARBITER_rule_def *rule = &model->rules[r];

		/* Evaluate the rule's condition group */
		bool fired = eval_condition_group(model, snapshot,
						  rule->condition_start,
						  rule->condition_count,
						  &op_count);

		if (fired) {
			/* Execute compute expressions */
			eval_expressions(model, snap_mut,
					 rule->expr_start,
					 rule->expr_count,
					 &op_count);

			/* Apply mode transition if specified */
			if (rule->set_mode != UINT16_MAX) {
				result->previous_mode = result->current_mode;
				result->current_mode = rule->set_mode;
			}

			/* Record requested actions */
			for (uint16_t a = 0; a < rule->action_count; a++) {
				uint16_t ai = rule->action_start + a;

				if (ai < model->action_count &&
				    result->requested_action_count <
				    CONFIG_ARBITER_MAX_ACTIONS_PER_EVAL) {
					uint16_t act_id =
						model->actions[ai].id;

					switch (model->actions[ai].type) {
					case ARBITER_ACTION_RAISE_FAULT:
						if (model->actions[ai]
						    .target_fact_id < 32) {
							result->raised_faults |=
								BIT(model->actions[ai].target_fact_id);
						}
						break;
					case ARBITER_ACTION_CLEAR_FAULT:
						if (model->actions[ai]
						    .target_fact_id < 32) {
							result->raised_faults &=
								~BIT(model->actions[ai].target_fact_id);
						}
						break;
					default:
						break;
					}

					result->requested_actions[
						result->requested_action_count] =
						act_id;
					result->requested_action_count++;
				}
			}
		}

		/* Record trace */
		record_trace(trace, rule, model, fired,
			     fired && rule->action_count > 0
			     ? model->actions[rule->action_start].id
			     : UINT16_MAX);

		op_count++; /* Count rule evaluation itself */
	}

	result->eval_op_count = op_count;
	return ARBITER_OK;
}

int ARBITER_get_mode(const struct ARBITER_result *result, uint16_t *mode_id)
{
	if (result == NULL || mode_id == NULL) {
		return ARBITER_EINVAL;
	}
	*mode_id = result->current_mode;
	return ARBITER_OK;
}

int ARBITER_fault_is_raised(const struct ARBITER_result *result,
			   uint16_t fault_id)
{
	if (result == NULL) {
		return ARBITER_EINVAL;
	}
	if (fault_id >= 32) {
		return ARBITER_ERANGE;
	}
	return (result->raised_faults & BIT(fault_id)) ? 1 : 0;
}

int ARBITER_get_requested_actions(const struct ARBITER_result *result,
				const uint16_t **actions, size_t *count)
{
	if (result == NULL || actions == NULL || count == NULL) {
		return ARBITER_EINVAL;
	}
	*actions = result->requested_actions;
	*count = result->requested_action_count;
	return ARBITER_OK;
}
