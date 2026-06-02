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

const char *ARBITER_version_string(void)
{
	return ARBITER_VERSION_STRING;
}
