/* SPDX-License-Identifier: MIT */

/**
 * @file arbiter_event.c
 * @brief Event-driven evaluation — watch/notify bitmask implementation.
 *
 * Uses compile-time-sized bitmasks: uint32_t for <= 32 facts,
 * uint8_t array for larger models.  Zero dynamic allocation.
 */

#include <arbiter/arbiter_event.h>
#include <string.h>
#include <zephyr/kernel.h>

/* ── Bitmask helpers (compile-time selected) ──────────────────── */

#if CONFIG_ARBITER_MAX_FACTS <= 32

static inline void mask_set(uint32_t *mask, uint16_t bit)
{
	*mask |= BIT(bit);
}

static inline void mask_clear(uint32_t *mask, uint16_t bit)
{
	*mask &= ~BIT(bit);
}

static inline bool mask_test(uint32_t mask, uint16_t bit)
{
	return (mask & BIT(bit)) != 0u;
}

static inline bool mask_any(uint32_t mask)
{
	return mask != 0u;
}

#else /* byte-array path for > 32 facts */

static inline void bmask_set(uint8_t *arr, uint16_t bit)
{
	arr[bit >> 3] |= (uint8_t)(1u << (bit & 7u));
}

static inline void bmask_clear(uint8_t *arr, uint16_t bit)
{
	arr[bit >> 3] &= (uint8_t)~(1u << (bit & 7u));
}

static inline bool bmask_test(const uint8_t *arr, uint16_t bit)
{
	return (arr[bit >> 3] & (uint8_t)(1u << (bit & 7u))) != 0u;
}

static inline bool bmask_any(const uint8_t *arr, size_t bytes)
{
	for (size_t i = 0; i < bytes; i++) {
		if (arr[i] != 0u) {
			return true;
		}
	}
	return false;
}

#endif /* CONFIG_ARBITER_MAX_FACTS <= 32 */

/* ── Public API ───────────────────────────────────────────────── */

int ARBITER_event_init(struct ARBITER_event_ctx *ectx)
{
	if (unlikely(ectx == NULL)) {
		return ARBITER_EINVAL;
	}

	memset(ectx, 0, sizeof(*ectx));
	return ARBITER_OK;
}

int ARBITER_watch_fact(struct ARBITER_event_ctx *ectx, uint16_t fact_id)
{
	if (unlikely(ectx == NULL)) {
		return ARBITER_EINVAL;
	}
	if (unlikely(fact_id >= CONFIG_ARBITER_MAX_FACTS)) {
		return ARBITER_ERANGE;
	}

#if CONFIG_ARBITER_MAX_FACTS <= 32
	mask_set(&ectx->watched, fact_id);
#else
	bmask_set(ectx->watched, fact_id);
#endif

	return ARBITER_OK;
}

int ARBITER_unwatch_fact(struct ARBITER_event_ctx *ectx, uint16_t fact_id)
{
	if (unlikely(ectx == NULL)) {
		return ARBITER_EINVAL;
	}
	if (unlikely(fact_id >= CONFIG_ARBITER_MAX_FACTS)) {
		return ARBITER_ERANGE;
	}

#if CONFIG_ARBITER_MAX_FACTS <= 32
	mask_clear(&ectx->watched, fact_id);
	mask_clear(&ectx->pending, fact_id);

	/* Recompute fast flag */
	ectx->any_pending = mask_any(ectx->pending);
#else
	bmask_clear(ectx->watched, fact_id);
	bmask_clear(ectx->pending, fact_id);

	ectx->any_pending = bmask_any(ectx->pending,
				      ARBITER_EVENT_MASK_BYTES);
#endif

	return ARBITER_OK;
}

int ARBITER_notify_fact_changed(struct ARBITER_event_ctx *ectx,
				uint16_t fact_id)
{
	if (unlikely(ectx == NULL)) {
		return ARBITER_EINVAL;
	}
	if (unlikely(fact_id >= CONFIG_ARBITER_MAX_FACTS)) {
		return ARBITER_ERANGE;
	}

#if CONFIG_ARBITER_MAX_FACTS <= 32
	if (mask_test(ectx->watched, fact_id)) {
		mask_set(&ectx->pending, fact_id);
		ectx->any_pending = true;
	}
#else
	if (bmask_test(ectx->watched, fact_id)) {
		bmask_set(ectx->pending, fact_id);
		ectx->any_pending = true;
	}
#endif

	return ARBITER_OK;
}

bool ARBITER_event_pending(const struct ARBITER_event_ctx *ectx)
{
	if (unlikely(ectx == NULL)) {
		return false;
	}
	return ectx->any_pending;
}

int ARBITER_event_clear(struct ARBITER_event_ctx *ectx)
{
	if (unlikely(ectx == NULL)) {
		return ARBITER_EINVAL;
	}

#if CONFIG_ARBITER_MAX_FACTS <= 32
	ectx->pending = 0u;
#else
	memset(ectx->pending, 0, sizeof(ectx->pending));
#endif
	ectx->any_pending = false;

	return ARBITER_OK;
}
