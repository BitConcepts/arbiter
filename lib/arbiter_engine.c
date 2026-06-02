/* SPDX-License-Identifier: MIT */

#include <arbiter/arbiter.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(arbiter, CONFIG_ARBITER_LOG_LEVEL);

int ARBITER_init(struct ARBITER_ctx *ctx, const struct ARBITER_model *model)
{
	if (ctx == NULL || model == NULL) {
		return ARBITER_EINVAL;
	}

	if (model->facts == NULL || model->rules == NULL) {
		return ARBITER_EMODEL;
	}

	if (model->fact_count > CONFIG_ARBITER_MAX_FACTS) {
		LOG_ERR("Model has %u facts, max is %d",
			model->fact_count, CONFIG_ARBITER_MAX_FACTS);
		return ARBITER_EMODEL;
	}

	/*
	 * Single memset zeros entire ctx (fact_values[], snapshot,
	 * counters, flags). Then patch only the non-zero fields.
	 * This is faster than per-field init for large MAX_FACTS
	 * because memset is often a hardware-optimized word-fill.
	 */
	memset(ctx, 0, sizeof(*ctx));
	ctx->model = model;

	/* Patch only default values — everything else is already 0 */
	const struct ARBITER_fact_def *__restrict facts = model->facts;
	struct ARBITER_fact_value *__restrict fv = ctx->fact_values;

	for (arbiter_index_t i = 0; i < model->fact_count; i++) {
		const int32_t def = facts[i].default_value;

		fv[i].value = def;
		fv[i].prev_value = def;
	}

	/* Set up internal snapshot to reference the fact_values array */
	ctx->snapshot.values = ctx->fact_values;
	ctx->snapshot.count = model->fact_count;
	ctx->snapshot.timestamp_ms = 0;
	ctx->snapshot.frozen = false;

	ctx->last_eval_op_count = 0;
	ctx->initialized = true;

	LOG_INF("arbiter initialized: model=%s facts=%u rules=%u",
		model->name ? model->name : "unnamed",
		model->fact_count, model->rule_count);

	return ARBITER_OK;
}

uint32_t ARBITER_get_last_eval_op_count(const struct ARBITER_ctx *ctx)
{
	if (ctx == NULL || !ctx->initialized) {
		return 0;
	}
	return ctx->last_eval_op_count;
}

int ARBITER_check_version(const struct ARBITER_model *model,
			  uint8_t min_major, uint8_t min_minor)
{
	if (unlikely(model == NULL)) {
		return ARBITER_EINVAL;
	}

	if (model->version[0] > min_major) {
		return ARBITER_OK;
	}
	if (model->version[0] == min_major &&
	    model->version[1] >= min_minor) {
		return ARBITER_OK;
	}

	LOG_WRN("Model version %u.%u.%u < required %u.%u",
		model->version[0], model->version[1], model->version[2],
		min_major, min_minor);
	return ARBITER_EMODEL;
}

#if defined(CONFIG_ARBITER_HOT_SWAP) && CONFIG_ARBITER_HOT_SWAP
int ARBITER_hot_swap(struct ARBITER_ctx *ctx,
		    const struct ARBITER_model *new_model)
{
	if (unlikely(ctx == NULL || new_model == NULL)) {
		return ARBITER_EINVAL;
	}

	if (unlikely(!ctx->initialized)) {
		LOG_ERR("hot_swap: context not initialized");
		return ARBITER_EINVAL;
	}

	/* Validate new model basics */
	if (new_model->facts == NULL || new_model->rules == NULL) {
		LOG_ERR("hot_swap: new model has NULL facts or rules");
		return ARBITER_EMODEL;
	}

	if (new_model->fact_count > CONFIG_ARBITER_MAX_FACTS) {
		LOG_ERR("hot_swap: new model has %u facts, max is %d",
			new_model->fact_count, CONFIG_ARBITER_MAX_FACTS);
		return ARBITER_EMODEL;
	}

	/*
	 * Preserve fact values: for each fact in the new model that also
	 * existed in the old model (same index, same type), keep the
	 * current value. All other facts get their defaults.
	 */
	const struct ARBITER_model *__restrict old_model = ctx->model;
	struct ARBITER_fact_value *__restrict fv = ctx->fact_values;
	arbiter_index_t common = (new_model->fact_count < old_model->fact_count)
			       ? new_model->fact_count : old_model->fact_count;

	/* Reset facts beyond the common range to new defaults */
	for (arbiter_index_t i = common; i < new_model->fact_count; i++) {
		int32_t def = new_model->facts[i].default_value;

		fv[i].value      = def;
		fv[i].prev_value = def;
		fv[i].timestamp_ms = 0;
		fv[i].valid   = false;
		fv[i].changed = false;
	}

	/* For common facts, keep value if type matches; reset otherwise */
	for (arbiter_index_t i = 0; i < common; i++) {
		if (new_model->facts[i].type != old_model->facts[i].type) {
			int32_t def = new_model->facts[i].default_value;

			fv[i].value      = def;
			fv[i].prev_value = def;
			fv[i].timestamp_ms = 0;
			fv[i].valid   = false;
			fv[i].changed = false;
		}
	}

	/* Zero out facts beyond new count */
	for (arbiter_index_t i = new_model->fact_count;
	     i < old_model->fact_count && i < CONFIG_ARBITER_MAX_FACTS; i++) {
		memset(&fv[i], 0, sizeof(fv[i]));
	}

	/* Atomic swap: update model pointer and snapshot metadata */
	ctx->model = new_model;
	ctx->snapshot.count = new_model->fact_count;
	ctx->snapshot.frozen = false;

	LOG_INF("hot_swap: %s -> %s (facts: %u -> %u)",
		old_model->name ? old_model->name : "unnamed",
		new_model->name ? new_model->name : "unnamed",
		old_model->fact_count, new_model->fact_count);

	return ARBITER_OK;
}
#endif /* CONFIG_ARBITER_HOT_SWAP */

const char *ARBITER_version_string(void)
{
	return ARBITER_VERSION_STRING;
}
