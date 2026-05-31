/* SPDX-License-Identifier: MIT */

#ifndef ZPROJ_MODEL_H_
#define ZPROJ_MODEL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Supported fact types in ZRM v0. */
enum zproj_fact_type {
	ZPROJ_FACT_BOOL = 0,
	ZPROJ_FACT_INT32,
	ZPROJ_FACT_UINT32,
	ZPROJ_FACT_ENUM,
};

/** Rule classification. */
enum zproj_rule_class {
	ZPROJ_RULE_INFERENCE = 0,
	ZPROJ_RULE_CONSTRAINT,
	ZPROJ_RULE_MODE_GUARD,
	ZPROJ_RULE_SAFETY_GUARD,
	ZPROJ_RULE_OBLIGATION,
	ZPROJ_RULE_ADVISORY,
};

/** Condition comparison operators. */
enum zproj_op {
	ZPROJ_OP_EQ = 0,
	ZPROJ_OP_NE,
	ZPROJ_OP_LT,
	ZPROJ_OP_LE,
	ZPROJ_OP_GT,
	ZPROJ_OP_GE,
	ZPROJ_OP_IN,
	ZPROJ_OP_NOT_IN,
	ZPROJ_OP_STALE,
	ZPROJ_OP_NOT_STALE,
	ZPROJ_OP_CHANGED,
	ZPROJ_OP_DELTA_GT,
	ZPROJ_OP_DELTA_LT,
};

/** Action types. */
enum zproj_action_type {
	ZPROJ_ACTION_CALLBACK = 0,
	ZPROJ_ACTION_LOG,
	ZPROJ_ACTION_NOTIFY,
	ZPROJ_ACTION_SET_FACT,
	ZPROJ_ACTION_SET_MODE,
	ZPROJ_ACTION_RAISE_FAULT,
	ZPROJ_ACTION_CLEAR_FAULT,
};

/** Condition group types. */
enum zproj_cond_group {
	ZPROJ_COND_ALL = 0,
	ZPROJ_COND_ANY,
	ZPROJ_COND_NOT,
};

/** Expression operators for compute engine. */
enum zproj_expr_op {
	ZPROJ_EXPR_ADD = 0,    /**< target = left + right */
	ZPROJ_EXPR_SUB,        /**< target = left - right */
	ZPROJ_EXPR_MUL,        /**< target = left * right */
	ZPROJ_EXPR_DIV,        /**< target = left / right (div-by-zero -> 0) */
	ZPROJ_EXPR_MOD,        /**< target = left % right */
	ZPROJ_EXPR_ABS,        /**< target = |left| (right ignored) */
	ZPROJ_EXPR_NEGATE,     /**< target = -left (right ignored) */
	ZPROJ_EXPR_MIN,        /**< target = min(left, right) */
	ZPROJ_EXPR_MAX,        /**< target = max(left, right) */
	ZPROJ_EXPR_CLAMP,      /**< target = clamp(left, right=lo, scale=hi) */
	ZPROJ_EXPR_SHIFT_R,    /**< target = left >> right */
	ZPROJ_EXPR_SHIFT_L,    /**< target = left << right */
	ZPROJ_EXPR_SCALE,      /**< target = (left * right) / scale (fixed-point) */
	ZPROJ_EXPR_ASSIGN,     /**< target = left (copy fact or literal) */
	ZPROJ_EXPR_ACCUMULATE, /**< target = target + (left * right) / scale */
};

/** Fact definition (compiled model table entry). */
struct zproj_fact_def {
	uint16_t id;
	enum zproj_fact_type type;
	int32_t range_min;
	int32_t range_max;
	int32_t default_value;
	uint16_t stale_after_ms;
	bool safety_relevant;
	const char *name;
};

/** Expression definition — compute engine instruction. */
struct zproj_expr_def {
	uint16_t target_fact_id;  /**< Fact to write the result to. */
	enum zproj_expr_op op;
	uint16_t left_fact_id;    /**< UINT16_MAX = use left_literal. */
	int32_t left_literal;
	uint16_t right_fact_id;   /**< UINT16_MAX = use right_literal. */
	int32_t right_literal;
	int32_t scale;            /**< Divisor for SCALE/ACCUMULATE/CLAMP(hi). */
};

/** Condition definition (compiled model table entry). */
struct zproj_condition_def {
	uint16_t fact_id;
	enum zproj_op op;
	int32_t value;
	enum zproj_cond_group group;
	uint16_t group_index;
	uint16_t next;
};

/** Action definition (compiled model table entry). */
struct zproj_action_def {
	uint16_t id;
	enum zproj_action_type type;
	uint16_t target_fact_id;
	int32_t target_value;
	void (*callback)(void);
	uint16_t must_complete_within_ms;
	bool safe_state_action;
	const char *name;
};

/** Rule definition (compiled model table entry). */
struct zproj_rule_def {
	uint16_t id;
	enum zproj_rule_class rule_class;
	uint16_t condition_start;
	uint16_t condition_count;
	uint16_t action_start;
	uint16_t action_count;
	uint16_t expr_start;      /**< Index into expressions table. */
	uint16_t expr_count;      /**< Number of expressions to evaluate. */
	uint16_t safety_goal_id;
	uint16_t set_mode;
	bool safety_critical;
	const char *name;
	const char *explanation;
};

/** Complete compiled model. */
struct zproj_model {
	const char *name;
	const uint8_t model_hash[32];
	const uint8_t schema_hash[32];
	uint16_t fact_count;
	uint16_t rule_count;
	uint16_t condition_count;
	uint16_t action_count;
	uint16_t expr_count;
	uint16_t mode_count;
	const struct zproj_fact_def *facts;
	const struct zproj_rule_def *rules;
	const struct zproj_condition_def *conditions;
	const struct zproj_action_def *actions;
	const struct zproj_expr_def *expressions;
	const char **mode_names;
};

#ifdef __cplusplus
}
#endif

#endif /* ZPROJ_MODEL_H_ */
