/* SPDX-License-Identifier: MIT */

#include <zproj/zproj.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zproj, CONFIG_ZPROJ_LOG_LEVEL);

/**
 * Evaluate a single condition against the frozen snapshot.
 * Returns true if the condition is satisfied, false otherwise.
 * Increments *op_count for each operation performed.
 */
static bool eval_condition(const struct zproj_condition_def *cond,
			   const struct zproj_snapshot *snapshot,
			   uint32_t *op_count)
{
	(*op_count)++;

	if (cond->fact_id >= snapshot->count) {
		return false;
	}

	const struct zproj_fact_value *fv = &snapshot->values[cond->fact_id];

	switch (cond->op) {
	case ZPROJ_OP_EQ:
		return fv->value == cond->value;
	case ZPROJ_OP_NE:
		return fv->value != cond->value;
	case ZPROJ_OP_LT:
		return fv->value < cond->value;
	case ZPROJ_OP_LE:
		return fv->value <= cond->value;
	case ZPROJ_OP_GT:
		return fv->value > cond->value;
	case ZPROJ_OP_GE:
		return fv->value >= cond->value;
	case ZPROJ_OP_STALE:
		if (fv->timestamp_ms == 0) {
			return true; /* Never updated = stale */
		}
		return (snapshot->timestamp_ms - fv->timestamp_ms) >
		       (uint32_t)cond->value;
	case ZPROJ_OP_NOT_STALE:
		if (fv->timestamp_ms == 0) {
			return false;
		}
		return (snapshot->timestamp_ms - fv->timestamp_ms) <=
		       (uint32_t)cond->value;
	case ZPROJ_OP_CHANGED:
		return fv->changed;
	case ZPROJ_OP_DELTA_GT: {
		int32_t delta = fv->value - fv->prev_value;

		if (delta < 0) {
			delta = -delta;
		}
		return delta > cond->value;
	}
	case ZPROJ_OP_DELTA_LT: {
		int32_t delta = fv->value - fv->prev_value;

		if (delta < 0) {
			delta = -delta;
		}
		return delta < cond->value;
	}
	case ZPROJ_OP_IN:
	case ZPROJ_OP_NOT_IN:
		/* IN/NOT_IN require set membership; simplified to equality
		 * against the stored value for v0. Full set support is a
		 * future extension.
		 */
		if (cond->op == ZPROJ_OP_IN) {
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
static bool eval_condition_group(const struct zproj_model *model,
				 const struct zproj_snapshot *snapshot,
				 uint16_t start, uint16_t count,
				 uint32_t *op_count)
{
	if (count == 0) {
		return true; /* No conditions = vacuously true */
	}

	/* Determine group type from the first condition */
	enum zproj_cond_group group = model->conditions[start].group;

	switch (group) {
	case ZPROJ_COND_ALL:
		for (uint16_t i = 0; i < count; i++) {
			if (!eval_condition(&model->conditions[start + i],
					    snapshot, op_count)) {
				return false; /* Short-circuit */
			}
		}
		return true;

	case ZPROJ_COND_ANY:
		for (uint16_t i = 0; i < count; i++) {
			if (eval_condition(&model->conditions[start + i],
					   snapshot, op_count)) {
				return true; /* Short-circuit */
			}
		}
		return false;

	case ZPROJ_COND_NOT:
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
static int32_t resolve_operand(const struct zproj_snapshot *snapshot,
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
static void eval_expression(const struct zproj_expr_def *expr,
			    struct zproj_snapshot *snapshot,
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
	case ZPROJ_EXPR_ADD:
		result = left + right;
		break;
	case ZPROJ_EXPR_SUB:
		result = left - right;
		break;
	case ZPROJ_EXPR_MUL:
		result = left * right;
		break;
	case ZPROJ_EXPR_DIV:
		result = (right != 0) ? (left / right) : 0;
		break;
	case ZPROJ_EXPR_MOD:
		result = (right != 0) ? (left % right) : 0;
		break;
	case ZPROJ_EXPR_ABS:
		result = (left < 0) ? -left : left;
		break;
	case ZPROJ_EXPR_NEGATE:
		result = -left;
		break;
	case ZPROJ_EXPR_MIN:
		result = (left < right) ? left : right;
		break;
	case ZPROJ_EXPR_MAX:
		result = (left > right) ? left : right;
		break;
	case ZPROJ_EXPR_CLAMP:
		/* left=value, right=lo, scale=hi */
		if (left < right) {
			result = right;
		} else if (left > expr->scale) {
			result = expr->scale;
		} else {
			result = left;
		}
		break;
	case ZPROJ_EXPR_SHIFT_R:
		result = left >> (right & 31);
		break;
	case ZPROJ_EXPR_SHIFT_L:
		result = left << (right & 31);
		break;
	case ZPROJ_EXPR_SCALE: {
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
	case ZPROJ_EXPR_ASSIGN:
		result = left;
		break;
	case ZPROJ_EXPR_ACCUMULATE: {
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
static void eval_expressions(const struct zproj_model *model,
			     struct zproj_snapshot *snapshot,
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
static void record_trace(struct zproj_trace *trace,
			 const struct zproj_rule_def *rule,
			 const struct zproj_model *model,
			 bool condition_result,
			 uint16_t action_id)
{
	if (trace == NULL) {
		return;
	}

	struct zproj_trace_entry entry;

	memset(&entry, 0, sizeof(entry));
	entry.rule_id = rule->id;
	entry.condition_result = condition_result;
	entry.action_id = action_id;
	entry.reason = rule->explanation;

	/* Record input fact IDs from the rule's conditions */
	uint16_t input_count = 0;

	for (uint16_t i = 0; i < rule->condition_count &&
	     input_count < CONFIG_ZPROJ_MAX_TRACE_INPUTS; i++) {
		uint16_t ci = rule->condition_start + i;

		if (ci < model->condition_count) {
			entry.input_facts[input_count] =
				model->conditions[ci].fact_id;
			input_count++;
		}
	}
	entry.input_fact_count = input_count;

	zproj_trace_record(trace, &entry);
}

int zproj_eval(const struct zproj_model *model,
	       const struct zproj_snapshot *snapshot,
	       struct zproj_result *result,
	       struct zproj_trace *trace)
{
	if (model == NULL || snapshot == NULL || result == NULL) {
		return ZPROJ_EINVAL;
	}

	if (!snapshot->frozen) {
		LOG_WRN("Snapshot is not frozen");
		return ZPROJ_EINVAL;
	}

	/*
	 * The snapshot is "frozen" for external reads, but the compute
	 * engine writes derived facts into it during evaluation. This
	 * is safe because evaluation order is canonical and each
	 * expression reads only facts that were set before it.
	 *
	 * We cast away const for the mutable snapshot used internally.
	 */
	struct zproj_snapshot *snap_mut = (struct zproj_snapshot *)snapshot;

	/* Initialize result */
	memset(result, 0, sizeof(*result));
	result->status = ZPROJ_OK;

	uint32_t op_count = 0;

	/* Evaluate rules in canonical order (array index order) */
	for (uint16_t r = 0; r < model->rule_count; r++) {
		const struct zproj_rule_def *rule = &model->rules[r];

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
				    CONFIG_ZPROJ_MAX_ACTIONS_PER_EVAL) {
					uint16_t act_id =
						model->actions[ai].id;

					switch (model->actions[ai].type) {
					case ZPROJ_ACTION_RAISE_FAULT:
						if (model->actions[ai]
						    .target_fact_id < 32) {
							result->raised_faults |=
								BIT(model->actions[ai].target_fact_id);
						}
						break;
					case ZPROJ_ACTION_CLEAR_FAULT:
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
	return ZPROJ_OK;
}

int zproj_get_mode(const struct zproj_result *result, uint16_t *mode_id)
{
	if (result == NULL || mode_id == NULL) {
		return ZPROJ_EINVAL;
	}
	*mode_id = result->current_mode;
	return ZPROJ_OK;
}

int zproj_fault_is_raised(const struct zproj_result *result,
			   uint16_t fault_id)
{
	if (result == NULL) {
		return ZPROJ_EINVAL;
	}
	if (fault_id >= 32) {
		return ZPROJ_ERANGE;
	}
	return (result->raised_faults & BIT(fault_id)) ? 1 : 0;
}

int zproj_get_requested_actions(const struct zproj_result *result,
				const uint16_t **actions, size_t *count)
{
	if (result == NULL || actions == NULL || count == NULL) {
		return ZPROJ_EINVAL;
	}
	*actions = result->requested_actions;
	*count = result->requested_action_count;
	return ZPROJ_OK;
}
