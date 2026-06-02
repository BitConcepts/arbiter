/* SPDX-License-Identifier: MIT */

#ifndef ARBITER_MODEL_H_
#define ARBITER_MODEL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Index type: uint8_t for nano/micro profiles, uint16_t for standard/full.
 * Driven by CONFIG_ARBITER_8BIT_INDEX from Kconfig.
 */
#if defined(CONFIG_ARBITER_8BIT_INDEX) && CONFIG_ARBITER_8BIT_INDEX
typedef uint8_t arbiter_index_t;
#define ARBITER_INDEX_MAX UINT8_MAX
#else
typedef uint16_t arbiter_index_t;
#define ARBITER_INDEX_MAX UINT16_MAX
#endif

/** Supported fact types in ARB v0. */
enum ARBITER_fact_type {
	ARBITER_FACT_BOOL = 0,
	ARBITER_FACT_INT32,
	ARBITER_FACT_UINT32,
	ARBITER_FACT_ENUM,
};

/** Rule classification. */
enum ARBITER_rule_class {
	ARBITER_RULE_INFERENCE = 0,
	ARBITER_RULE_CONSTRAINT,
	ARBITER_RULE_MODE_GUARD,
	ARBITER_RULE_SAFETY_GUARD,
	ARBITER_RULE_OBLIGATION,
	ARBITER_RULE_ADVISORY,
};

/** Condition comparison operators. */
enum ARBITER_op {
	ARBITER_OP_EQ = 0,
	ARBITER_OP_NE,
	ARBITER_OP_LT,
	ARBITER_OP_LE,
	ARBITER_OP_GT,
	ARBITER_OP_GE,
	ARBITER_OP_IN,
	ARBITER_OP_NOT_IN,
	ARBITER_OP_STALE,
	ARBITER_OP_NOT_STALE,
	ARBITER_OP_CHANGED,
	ARBITER_OP_DELTA_GT,
	ARBITER_OP_DELTA_LT,
};

/** Action types. */
enum ARBITER_action_type {
	ARBITER_ACTION_CALLBACK = 0,
	ARBITER_ACTION_LOG,
	ARBITER_ACTION_NOTIFY,
	ARBITER_ACTION_SET_FACT,
	ARBITER_ACTION_SET_MODE,
	ARBITER_ACTION_RAISE_FAULT,
	ARBITER_ACTION_CLEAR_FAULT,
};

/** Condition group types. */
enum ARBITER_cond_group {
	ARBITER_COND_ALL = 0,
	ARBITER_COND_ANY,
	ARBITER_COND_NOT,
};

/** Expression operators for compute engine. */
enum ARBITER_expr_op {
	ARBITER_EXPR_ADD = 0,    /**< target = left + right */
	ARBITER_EXPR_SUB,        /**< target = left - right */
	ARBITER_EXPR_MUL,        /**< target = left * right */
	ARBITER_EXPR_DIV,        /**< target = left / right (div-by-zero -> 0) */
	ARBITER_EXPR_MOD,        /**< target = left % right */
	ARBITER_EXPR_ABS,        /**< target = |left| (right ignored) */
	ARBITER_EXPR_NEGATE,     /**< target = -left (right ignored) */
	ARBITER_EXPR_MIN,        /**< target = min(left, right) */
	ARBITER_EXPR_MAX,        /**< target = max(left, right) */
	ARBITER_EXPR_CLAMP,      /**< target = clamp(left, right=lo, scale=hi) */
	ARBITER_EXPR_SHIFT_R,    /**< target = left >> right */
	ARBITER_EXPR_SHIFT_L,    /**< target = left << right */
	ARBITER_EXPR_SCALE,      /**< target = (left * right) / scale (fixed-point) */
	ARBITER_EXPR_ASSIGN,     /**< target = left (copy fact or literal) */
	ARBITER_EXPR_ACCUMULATE, /**< target = target + (left * right) / scale */
};

/** Fact definition (compiled model table entry). */
struct ARBITER_fact_def {
	arbiter_index_t id;
	enum ARBITER_fact_type type;
	int32_t range_min;
	int32_t range_max;
	int32_t default_value;
	arbiter_index_t stale_after_ms;
	bool safety_relevant;
#if !defined(CONFIG_ARBITER_STRINGS) || CONFIG_ARBITER_STRINGS
	const char *name;
#endif
};

/** Expression definition — compute engine instruction. */
struct ARBITER_expr_def {
	arbiter_index_t target_fact_id;  /**< Fact to write the result to. */
	enum ARBITER_expr_op op;
	arbiter_index_t left_fact_id;    /**< ARBITER_INDEX_MAX = use left_literal. */
	int32_t left_literal;
	arbiter_index_t right_fact_id;   /**< ARBITER_INDEX_MAX = use right_literal. */
	int32_t right_literal;
	int32_t scale;            /**< Divisor for SCALE/ACCUMULATE/CLAMP(hi). */
};

/** Condition definition (compiled model table entry). */
struct ARBITER_condition_def {
	arbiter_index_t fact_id;
	enum ARBITER_op op;
	int32_t value;
	enum ARBITER_cond_group group;
};

/** Action definition (compiled model table entry). */
struct ARBITER_action_def {
	arbiter_index_t id;
	enum ARBITER_action_type type;
	arbiter_index_t target_fact_id;
	int32_t target_value;
	void (*callback)(void);
	arbiter_index_t must_complete_within_ms;
	bool safe_state_action;
#if !defined(CONFIG_ARBITER_STRINGS) || CONFIG_ARBITER_STRINGS
	const char *name;
#endif
};

/** Rule definition (compiled model table entry). */
struct ARBITER_rule_def {
	arbiter_index_t id;
	enum ARBITER_rule_class rule_class;
	arbiter_index_t condition_start;
	arbiter_index_t condition_count;
	arbiter_index_t action_start;
	arbiter_index_t action_count;
	arbiter_index_t expr_start;      /**< Index into expressions table. */
	arbiter_index_t expr_count;      /**< Number of expressions to evaluate. */
	arbiter_index_t safety_goal_id;
	arbiter_index_t set_mode;
	bool safety_critical;
#if !defined(CONFIG_ARBITER_STRINGS) || CONFIG_ARBITER_STRINGS
	const char *name;
	const char *explanation;
#endif
};

/** Complete compiled model. */
struct ARBITER_model {
	const char *name;
	uint8_t version[3]; /**< Model version: [major, minor, patch]. */
	const uint8_t model_hash[32];
	const uint8_t schema_hash[32];
	arbiter_index_t fact_count;
	arbiter_index_t rule_count;
	arbiter_index_t condition_count;
	arbiter_index_t action_count;
	arbiter_index_t expr_count;
	arbiter_index_t mode_count;
	const struct ARBITER_fact_def *facts;
	const struct ARBITER_rule_def *rules;
	const struct ARBITER_condition_def *conditions;
	const struct ARBITER_action_def *actions;
	const struct ARBITER_expr_def *expressions;
	const char **mode_names;
#if defined(CONFIG_ARBITER_FPGA_OFFLOAD) && CONFIG_ARBITER_FPGA_OFFLOAD
	const struct ARBITER_hw_offload_ops *offload_ops;
#endif
};

#ifdef __cplusplus
}
#endif

#endif /* ARBITER_MODEL_H_ */
