/* SPDX-License-Identifier: MIT */

#include <zproj/zproj.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zproj, CONFIG_ZPROJ_LOG_LEVEL);

int zproj_init(struct zproj_ctx *ctx, const struct zproj_model *model)
{
	if (ctx == NULL || model == NULL) {
		return ZPROJ_EINVAL;
	}

	if (model->facts == NULL || model->rules == NULL) {
		return ZPROJ_EMODEL;
	}

	if (model->fact_count > CONFIG_ZPROJ_MAX_FACTS) {
		LOG_ERR("Model has %u facts, max is %d",
			model->fact_count, CONFIG_ZPROJ_MAX_FACTS);
		return ZPROJ_EMODEL;
	}

	memset(ctx, 0, sizeof(*ctx));
	ctx->model = model;

	/* Initialize fact values from model defaults */
	for (uint16_t i = 0; i < model->fact_count; i++) {
		ctx->fact_values[i].value = model->facts[i].default_value;
		ctx->fact_values[i].prev_value = model->facts[i].default_value;
		ctx->fact_values[i].timestamp_ms = 0;
		ctx->fact_values[i].valid = false;
		ctx->fact_values[i].changed = false;
	}

	/* Set up internal snapshot to reference the fact_values array */
	ctx->snapshot.values = ctx->fact_values;
	ctx->snapshot.count = model->fact_count;
	ctx->snapshot.timestamp_ms = 0;
	ctx->snapshot.frozen = false;

	ctx->last_eval_op_count = 0;
	ctx->initialized = true;

	LOG_INF("zproj initialized: model=%s facts=%u rules=%u",
		model->name ? model->name : "unnamed",
		model->fact_count, model->rule_count);

	return ZPROJ_OK;
}

uint32_t zproj_get_last_eval_op_count(const struct zproj_ctx *ctx)
{
	if (ctx == NULL || !ctx->initialized) {
		return 0;
	}
	return ctx->last_eval_op_count;
}

const char *zproj_version_string(void)
{
	return ZPROJ_VERSION_STRING;
}
