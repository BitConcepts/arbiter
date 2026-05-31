/* SPDX-License-Identifier: MIT */

#ifndef ZPROJ_H_
#define ZPROJ_H_

/**
 * @defgroup zproj zproj Deterministic Reasoning Engine
 * @{
 * @brief Deterministic reasoning and safety-policy engine for Zephyr RTOS.
 *
 * zproj evaluates compiled ZRM (Zephyr Reasoning Model) models against frozen
 * input snapshots, producing deterministic results and explanation traces.
 */

#include <zproj/zproj_version.h>
#include <zproj/zproj_model.h>
#include <zproj/zproj_result.h>
#include <zproj/zproj_trace.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes */
#define ZPROJ_OK                   0
#define ZPROJ_EINVAL             -22
#define ZPROJ_ERANGE             -34
#define ZPROJ_ESTALE           -1001
#define ZPROJ_EMODEL           -1002
#define ZPROJ_EOVERFLOW        -1003
#define ZPROJ_ETRACE_FULL      -1004
#define ZPROJ_ESAFETY_VIOLATION -1005

#ifndef CONFIG_ZPROJ_MAX_FACTS
#define CONFIG_ZPROJ_MAX_FACTS 64
#endif

/** Runtime context holding model reference and live fact state. */
struct zproj_ctx {
	const struct zproj_model *model;
	struct zproj_snapshot snapshot;
	struct zproj_fact_value fact_values[CONFIG_ZPROJ_MAX_FACTS];
	uint32_t last_eval_op_count;
	bool initialized;
};

/**
 * @brief Initialize a zproj context with a compiled model.
 *
 * Validates the model and sets all facts to their default values.
 *
 * @param ctx  Context to initialize.
 * @param model Compiled model (generated C tables or loaded blob).
 * @return ZPROJ_OK on success, ZPROJ_EINVAL if pointers are NULL,
 *         ZPROJ_EMODEL if the model exceeds CONFIG_ZPROJ_MAX_FACTS.
 */
int zproj_init(struct zproj_ctx *ctx, const struct zproj_model *model);

/**
 * @brief Set a boolean fact value.
 *
 * @param ctx     Initialized context.
 * @param fact_id Fact index (0-based).
 * @param value   Boolean value.
 * @return ZPROJ_OK on success, ZPROJ_EINVAL on bad id or type mismatch.
 */
int zproj_set_bool(struct zproj_ctx *ctx, uint16_t fact_id, bool value);

/**
 * @brief Set a signed 32-bit integer fact value.
 */
int zproj_set_i32(struct zproj_ctx *ctx, uint16_t fact_id, int32_t value);

/**
 * @brief Set an unsigned 32-bit integer fact value.
 */
int zproj_set_u32(struct zproj_ctx *ctx, uint16_t fact_id, uint32_t value);

/**
 * @brief Set an enum fact value.
 */
int zproj_set_enum(struct zproj_ctx *ctx, uint16_t fact_id, uint16_t value);

/**
 * @brief Set the timestamp for a fact (for staleness tracking).
 *
 * @param ctx          Initialized context.
 * @param fact_id      Fact index.
 * @param timestamp_ms Timestamp in milliseconds.
 */
int zproj_set_timestamp(struct zproj_ctx *ctx, uint16_t fact_id,
			uint32_t timestamp_ms);

/**
 * @brief Freeze current fact values into a snapshot for evaluation.
 *
 * After this call the snapshot is frozen and safe for deterministic evaluation.
 *
 * @param ctx      Initialized context (source of current values).
 * @param snapshot Destination snapshot.
 * @return ZPROJ_OK on success.
 */
int zproj_snapshot_begin(struct zproj_ctx *ctx,
			 struct zproj_snapshot *snapshot);

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
 * @return ZPROJ_OK on success, or an error code.
 */
int zproj_eval(const struct zproj_model *model,
	       const struct zproj_snapshot *snapshot,
	       struct zproj_result *result,
	       struct zproj_trace *trace);

/**
 * @brief Get the current mode from an evaluation result.
 */
int zproj_get_mode(const struct zproj_result *result, uint16_t *mode_id);

/**
 * @brief Check whether a fault is raised in the evaluation result.
 *
 * @param result   Evaluation result.
 * @param fault_id Fault bit index (0-31).
 * @return 1 if raised, 0 if not, ZPROJ_EINVAL on bad id.
 */
int zproj_fault_is_raised(const struct zproj_result *result,
			   uint16_t fault_id);

/**
 * @brief Get the list of requested actions from an evaluation result.
 */
int zproj_get_requested_actions(const struct zproj_result *result,
				const uint16_t **actions, size_t *count);

/**
 * @brief Get the operation count from the last evaluation.
 */
uint32_t zproj_get_last_eval_op_count(const struct zproj_ctx *ctx);

/**
 * @brief Get the zproj version string.
 */
const char *zproj_version_string(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZPROJ_H_ */
