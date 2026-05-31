/* SPDX-License-Identifier: MIT */

#ifndef ARBITER_H_
#define ARBITER_H_

/**
 * @defgroup arbiter arbiter Deterministic Reasoning Engine
 * @{
 * @brief Deterministic reasoning and safety-policy engine for Zephyr RTOS.
 *
 * arbiter evaluates compiled ARB (Zephyr Reasoning Model) models against frozen
 * input snapshots, producing deterministic results and explanation traces.
 */

#include <arbiter/ARBITER_version.h>
#include <arbiter/ARBITER_model.h>
#include <arbiter/ARBITER_result.h>
#include <arbiter/ARBITER_trace.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes */
#define ARBITER_OK                   0
#define ARBITER_EINVAL             -22
#define ARBITER_ERANGE             -34
#define ARBITER_ESTALE           -1001
#define ARBITER_EMODEL           -1002
#define ARBITER_EOVERFLOW        -1003
#define ARBITER_ETRACE_FULL      -1004
#define ARBITER_ESAFETY_VIOLATION -1005

#ifndef CONFIG_ARBITER_MAX_FACTS
#define CONFIG_ARBITER_MAX_FACTS 64
#endif

/** Runtime context holding model reference and live fact state. */
struct ARBITER_ctx {
	const struct ARBITER_model *model;
	struct ARBITER_snapshot snapshot;
	struct ARBITER_fact_value fact_values[CONFIG_ARBITER_MAX_FACTS];
	uint32_t last_eval_op_count;
	bool initialized;
};

/**
 * @brief Initialize a arbiter context with a compiled model.
 *
 * Validates the model and sets all facts to their default values.
 *
 * @param ctx  Context to initialize.
 * @param model Compiled model (generated C tables or loaded blob).
 * @return ARBITER_OK on success, ARBITER_EINVAL if pointers are NULL,
 *         ARBITER_EMODEL if the model exceeds CONFIG_ARBITER_MAX_FACTS.
 */
int ARBITER_init(struct ARBITER_ctx *ctx, const struct ARBITER_model *model);

/**
 * @brief Set a boolean fact value.
 *
 * @param ctx     Initialized context.
 * @param fact_id Fact index (0-based).
 * @param value   Boolean value.
 * @return ARBITER_OK on success, ARBITER_EINVAL on bad id or type mismatch.
 */
int ARBITER_set_bool(struct ARBITER_ctx *ctx, uint16_t fact_id, bool value);

/**
 * @brief Set a signed 32-bit integer fact value.
 */
int ARBITER_set_i32(struct ARBITER_ctx *ctx, uint16_t fact_id, int32_t value);

/**
 * @brief Set an unsigned 32-bit integer fact value.
 */
int ARBITER_set_u32(struct ARBITER_ctx *ctx, uint16_t fact_id, uint32_t value);

/**
 * @brief Set an enum fact value.
 */
int ARBITER_set_enum(struct ARBITER_ctx *ctx, uint16_t fact_id, uint16_t value);

/**
 * @brief Set the timestamp for a fact (for staleness tracking).
 *
 * @param ctx          Initialized context.
 * @param fact_id      Fact index.
 * @param timestamp_ms Timestamp in milliseconds.
 */
int ARBITER_set_timestamp(struct ARBITER_ctx *ctx, uint16_t fact_id,
			uint32_t timestamp_ms);

/**
 * @brief Freeze current fact values into a snapshot for evaluation.
 *
 * After this call the snapshot is frozen and safe for deterministic evaluation.
 *
 * @param ctx      Initialized context (source of current values).
 * @param snapshot Destination snapshot.
 * @return ARBITER_OK on success.
 */
int ARBITER_snapshot_begin(struct ARBITER_ctx *ctx,
			 struct ARBITER_snapshot *snapshot);

/**
 * @brief Evaluate a model against a frozen snapshot.
 *
 * This is the core deterministic evaluator. For a fixed model, snapshot, and
 * runtime version, the result and trace are guaranteed identical.
 *
 * The evaluator does NOT: call drivers, read sensors, allocate memory, sleep,
 * lock mutexes, perform I/O, or execute application callbacks.
 *
 * @param model    Compiled model.
 * @param snapshot Frozen fact snapshot.
 * @param result   Output result (modes, faults, actions).
 * @param trace    Optional trace buffer (NULL to disable tracing).
 * @return ARBITER_OK on success, or an error code.
 */
int ARBITER_eval(const struct ARBITER_model *model,
	       const struct ARBITER_snapshot *snapshot,
	       struct ARBITER_result *result,
	       struct ARBITER_trace *trace);

/**
 * @brief Get the current mode from an evaluation result.
 */
int ARBITER_get_mode(const struct ARBITER_result *result, uint16_t *mode_id);

/**
 * @brief Check whether a fault is raised in the evaluation result.
 *
 * @param result   Evaluation result.
 * @param fault_id Fault bit index (0-31).
 * @return 1 if raised, 0 if not, ARBITER_EINVAL on bad id.
 */
int ARBITER_fault_is_raised(const struct ARBITER_result *result,
			   uint16_t fault_id);

/**
 * @brief Get the list of requested actions from an evaluation result.
 */
int ARBITER_get_requested_actions(const struct ARBITER_result *result,
				const uint16_t **actions, size_t *count);

/**
 * @brief Get the operation count from the last evaluation.
 */
uint32_t ARBITER_get_last_eval_op_count(const struct ARBITER_ctx *ctx);

/**
 * @brief Get the arbiter version string.
 */
const char *ARBITER_version_string(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ARBITER_H_ */
