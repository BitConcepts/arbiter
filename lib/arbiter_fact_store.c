/* SPDX-License-Identifier: MIT */

#include <arbiter/arbiter.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(arbiter, CONFIG_ARBITER_LOG_LEVEL);

/*
 * Fact setter — inlined validation eliminates the 3-level call chain
 * (set_bool -> set_fact_value -> validate_ctx_and_fact) and lets the
 * compiler optimize the entire path as a single function body.
 *
 * The __attribute__((always_inline)) ensures this happens even at -O1.
 */
static inline __attribute__((always_inline))
int set_fact_value(struct ARBITER_ctx *ctx, arbiter_index_t fact_id,
		   enum ARBITER_fact_type expected_type, int32_t value)
{
	/* Inline validation — no function call overhead */
	if (unlikely(ctx == NULL || !ctx->initialized)) {
		return ARBITER_EINVAL;
	}
	if (unlikely(fact_id >= ctx->model->fact_count)) {
		return ARBITER_ERANGE;
	}

	const struct ARBITER_fact_def *__restrict def =
		&ctx->model->facts[fact_id];

	if (unlikely(def->type != expected_type)) {
		LOG_WRN("Type mismatch for fact %u: expected %d, got %d",
			fact_id, def->type, expected_type);
		return ARBITER_EINVAL;
	}

	/* Range check — only when range is specified (non-zero bounds) */
	if ((expected_type == ARBITER_FACT_INT32 ||
	     expected_type == ARBITER_FACT_UINT32) &&
	    (def->range_min != 0 || def->range_max != 0) &&
	    unlikely(value < def->range_min || value > def->range_max)) {
		LOG_WRN("Value %d out of range [%d, %d] for fact %u",
			value, def->range_min, def->range_max, fact_id);
		return ARBITER_ERANGE;
	}

	struct ARBITER_fact_value *__restrict fv = &ctx->fact_values[fact_id];
	const int32_t old = fv->value;

	fv->prev_value = old;
	fv->value = value;
	fv->valid = true;
	fv->changed = (value != old);

	return ARBITER_OK;
}

int ARBITER_set_bool(struct ARBITER_ctx *ctx, uint16_t fact_id, bool value)
{
	return set_fact_value(ctx, fact_id, ARBITER_FACT_BOOL,
			     value ? 1 : 0);
}

int ARBITER_set_i32(struct ARBITER_ctx *ctx, uint16_t fact_id, int32_t value)
{
	return set_fact_value(ctx, fact_id, ARBITER_FACT_INT32, value);
}

int ARBITER_set_u32(struct ARBITER_ctx *ctx, uint16_t fact_id, uint32_t value)
{
	if (value > INT32_MAX) {
		return ARBITER_EOVERFLOW;
	}
	return set_fact_value(ctx, fact_id, ARBITER_FACT_UINT32,
			     (int32_t)value);
}

int ARBITER_set_enum(struct ARBITER_ctx *ctx, uint16_t fact_id, uint16_t value)
{
	return set_fact_value(ctx, fact_id, ARBITER_FACT_ENUM,
			     (int32_t)value);
}

int ARBITER_set_timestamp(struct ARBITER_ctx *ctx, uint16_t fact_id,
			uint32_t timestamp_ms)
{
	if (unlikely(ctx == NULL || !ctx->initialized)) {
		return ARBITER_EINVAL;
	}
	if (unlikely(fact_id >= ctx->model->fact_count)) {
		return ARBITER_ERANGE;
	}

	ctx->fact_values[fact_id].timestamp_ms = timestamp_ms;
	return ARBITER_OK;
}

int ARBITER_snapshot_begin(struct ARBITER_ctx *ctx,
			 struct ARBITER_snapshot *snapshot)
{
	if (ctx == NULL || !ctx->initialized || snapshot == NULL) {
		return ARBITER_EINVAL;
	}

	/* Copy current fact values into the snapshot */
	snapshot->values = ctx->fact_values;
	snapshot->count = ctx->model->fact_count;
	snapshot->timestamp_ms = k_uptime_get_32();
	snapshot->frozen = true;

	return ARBITER_OK;
}
