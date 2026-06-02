/* SPDX-License-Identifier: MIT */

#ifndef ARBITER_COMPOSE_H_
#define ARBITER_COMPOSE_H_

/**
 * @defgroup arbiter_compose Multi-Model Composition (REQ-ARCH-037)
 * @ingroup arbiter
 * @{
 * @brief Share facts across multiple models via a common fact bus.
 *
 * The fact bus holds a shared array of fact values.  Each model is
 * "attached" with a mapping table that translates bus fact indices
 * to the model's own fact indices.  ARBITER_compose_sync() pushes
 * bus values into all attached contexts; ARBITER_compose_eval_all()
 * evaluates every attached model in attachment order.
 *
 * All storage is pre-allocated — no malloc.
 */

#include <stdint.h>
#include <stdbool.h>
#include <arbiter/arbiter.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_ARBITER_MAX_MODELS
#define CONFIG_ARBITER_MAX_MODELS 4
#endif

#ifndef CONFIG_ARBITER_COMPOSE_MAX_BUS_FACTS
#define CONFIG_ARBITER_COMPOSE_MAX_BUS_FACTS 64
#endif

/** Maps one bus fact to a model fact. */
struct ARBITER_fact_mapping {
	uint16_t bus_fact_id;    /**< Index into the bus fact array. */
	uint16_t model_fact_id;  /**< Index into the model's own fact array. */
};

/** Per-attachment record (bus ↔ model link). */
struct ARBITER_compose_attachment {
	struct ARBITER_ctx *ctx;                     /**< Model context. */
	const struct ARBITER_fact_mapping *mapping;   /**< Mapping table. */
	uint16_t map_count;                          /**< Number of mappings. */
	bool active;                                 /**< Slot occupied? */
};

/** Shared fact bus. */
struct ARBITER_fact_bus {
	struct ARBITER_fact_value *facts;             /**< Shared fact array. */
	uint16_t max_facts;                          /**< Capacity of facts[]. */

	struct ARBITER_compose_attachment
		attachments[CONFIG_ARBITER_MAX_MODELS]; /**< Attached models. */
	uint16_t attachment_count;                   /**< Active attachments. */
};

/**
 * @brief Initialize a fact bus with a pre-allocated fact array.
 *
 * @param bus       Bus to initialize.
 * @param facts     Pre-allocated array of fact values.
 * @param max_facts Capacity of facts[].
 * @return ARBITER_OK on success, ARBITER_EINVAL if bus or facts is NULL.
 */
int ARBITER_compose_init(struct ARBITER_fact_bus *bus,
			 struct ARBITER_fact_value *facts,
			 uint16_t max_facts);

/**
 * @brief Attach a model context to the bus.
 *
 * @param bus       Initialized fact bus.
 * @param ctx       Model context to attach.
 * @param mapping   Array of bus-to-model fact mappings.
 * @param map_count Number of entries in mapping[].
 * @return ARBITER_OK on success, ARBITER_EOVERFLOW if no slots available.
 */
int ARBITER_compose_attach(struct ARBITER_fact_bus *bus,
			   struct ARBITER_ctx *ctx,
			   const struct ARBITER_fact_mapping *mapping,
			   uint16_t map_count);

/**
 * @brief Detach a model context from the bus.
 *
 * @param bus Bus.
 * @param ctx Context to detach.
 * @return ARBITER_OK on success, ARBITER_EINVAL if not found.
 */
int ARBITER_compose_detach(struct ARBITER_fact_bus *bus,
			   struct ARBITER_ctx *ctx);

/**
 * @brief Set a fact value on the bus.
 *
 * @param bus     Initialized fact bus.
 * @param fact_id Bus fact index.
 * @param value   Value to set.
 * @return ARBITER_OK or ARBITER_ERANGE.
 */
int ARBITER_compose_set_fact(struct ARBITER_fact_bus *bus,
			     uint16_t fact_id, int32_t value);

/**
 * @brief Push bus facts into all attached model contexts.
 *
 * For each attachment, iterates the mapping table and copies the bus
 * fact values into the model context's fact_values[].
 *
 * @param bus Initialized fact bus.
 * @return ARBITER_OK on success.
 */
int ARBITER_compose_sync(struct ARBITER_fact_bus *bus);

/**
 * @brief Evaluate all attached models in attachment order.
 *
 * For each attached model, takes a snapshot, evaluates, and stores
 * the result.  Caller must provide arrays of at least
 * bus->attachment_count entries.
 *
 * @param bus     Initialized fact bus.
 * @param results Pre-allocated array for results (one per attached model).
 * @param traces  Pre-allocated array for traces (one per model, NULL entries OK).
 * @return ARBITER_OK on success.
 */
int ARBITER_compose_eval_all(struct ARBITER_fact_bus *bus,
			     struct ARBITER_result *results,
			     struct ARBITER_trace *traces);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ARBITER_COMPOSE_H_ */
