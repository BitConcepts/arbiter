/* SPDX-License-Identifier: MIT */

#ifndef ARBITER_EVENT_H_
#define ARBITER_EVENT_H_

/**
 * @defgroup arbiter_event Event-Driven Evaluation (REQ-ARCH-036)
 * @ingroup arbiter
 * @{
 * @brief Watch facts for changes and trigger evaluation only when needed.
 *
 * Instead of polling on a fixed period, the event subsystem lets callers
 * mark specific facts as "watched".  When a watched fact changes, a
 * pending flag is set.  The runtime thread (or application) can check
 * ARBITER_event_pending() and skip evaluation cycles when nothing has
 * changed — saving CPU on idle periods.
 *
 * Implementation uses static bitmasks — no dynamic allocation.
 * For models with <= 32 facts a single uint32_t pair is used.
 * For larger models a uint8_t byte-array is used (1 bit per fact).
 */

#include <stdint.h>
#include <stdbool.h>
#include <arbiter/arbiter.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_ARBITER_MAX_FACTS
#define CONFIG_ARBITER_MAX_FACTS 64
#endif

/** Number of bytes needed to hold one bit per fact. */
#define ARBITER_EVENT_MASK_BYTES \
	((CONFIG_ARBITER_MAX_FACTS + 7u) / 8u)

/** Event tracking state — embedded in or alongside an ARBITER_ctx. */
struct ARBITER_event_ctx {
#if CONFIG_ARBITER_MAX_FACTS <= 32
	uint32_t watched;           /**< Bitmask of watched fact ids. */
	uint32_t pending;           /**< Bitmask of changed watched facts. */
#else
	uint8_t watched[ARBITER_EVENT_MASK_BYTES];
	uint8_t pending[ARBITER_EVENT_MASK_BYTES];
#endif
	bool any_pending;           /**< Fast flag: true if any bit in pending is set. */
};

/**
 * @brief Initialize event tracking for a context.
 *
 * Clears all watched and pending flags.
 *
 * @param ectx Event context to initialize.
 * @return ARBITER_OK on success, ARBITER_EINVAL if ectx is NULL.
 */
int ARBITER_event_init(struct ARBITER_event_ctx *ectx);

/**
 * @brief Mark a fact as watched.
 *
 * @param ectx    Event context.
 * @param fact_id Fact index (0-based, < CONFIG_ARBITER_MAX_FACTS).
 * @return ARBITER_OK on success, ARBITER_ERANGE if fact_id out of range.
 */
int ARBITER_watch_fact(struct ARBITER_event_ctx *ectx, uint16_t fact_id);

/**
 * @brief Remove a fact from the watch set.
 *
 * Also clears any pending flag for that fact.
 *
 * @param ectx    Event context.
 * @param fact_id Fact index.
 * @return ARBITER_OK on success, ARBITER_ERANGE if fact_id out of range.
 */
int ARBITER_unwatch_fact(struct ARBITER_event_ctx *ectx, uint16_t fact_id);

/**
 * @brief Notify that a fact has changed.
 *
 * If the fact is watched, sets its pending bit and the fast flag.
 * If the fact is not watched, this is a no-op.
 *
 * @param ectx    Event context.
 * @param fact_id Fact index.
 * @return ARBITER_OK on success, ARBITER_ERANGE if fact_id out of range.
 */
int ARBITER_notify_fact_changed(struct ARBITER_event_ctx *ectx,
				uint16_t fact_id);

/**
 * @brief Check whether any watched fact has changed since last clear.
 *
 * @param ectx Event context.
 * @return true if at least one watched fact has a pending change.
 */
bool ARBITER_event_pending(const struct ARBITER_event_ctx *ectx);

/**
 * @brief Clear all pending flags.
 *
 * Typically called after a successful evaluation cycle.
 *
 * @param ectx Event context.
 * @return ARBITER_OK on success, ARBITER_EINVAL if ectx is NULL.
 */
int ARBITER_event_clear(struct ARBITER_event_ctx *ectx);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ARBITER_EVENT_H_ */
