/* SPDX-License-Identifier: MIT */

#include <arbiter/arbiter_zbus.h>
#include <arbiter/arbiter.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(arbiter, CONFIG_ARBITER_LOG_LEVEL);

/* ── Channel definitions ──────────────────────────────────────── */

ZBUS_CHAN_DEFINE(arbiter_facts_chan,
		struct arbiter_facts_msg,
		NULL, NULL,
		ZBUS_OBSERVERS(arbiter_facts_sub),
		ZBUS_MSG_INIT(.fact_id = 0, .value = 0, .timestamp_ms = 0));

ZBUS_CHAN_DEFINE(arbiter_result_chan,
		struct arbiter_result_msg,
		NULL, NULL,
		ZBUS_OBSERVERS_EMPTY,
		ZBUS_MSG_INIT(.mode = 0, .faults = 0,
			      .action_count = 0, .op_count = 0));

/* ── Static state ─────────────────────────────────────────────── */

static struct ARBITER_ctx *zbus_ctx;

/* ── Subscriber callback ──────────────────────────────────────── */

static void facts_cb(const struct zbus_channel *chan)
{
	if (zbus_ctx == NULL) {
		return;
	}

	struct arbiter_facts_msg msg;
	int ret = zbus_chan_read(chan, &msg, K_NO_WAIT);

	if (unlikely(ret < 0)) {
		LOG_ERR("zbus facts read failed: %d", ret);
		return;
	}

	ret = ARBITER_set_i32(zbus_ctx, msg.fact_id, msg.value);
	if (unlikely(ret != ARBITER_OK)) {
		LOG_WRN("set_i32 fact %u failed: %d",
			(unsigned int)msg.fact_id, ret);
		return;
	}

	ret = ARBITER_set_timestamp(zbus_ctx, msg.fact_id, msg.timestamp_ms);
	if (unlikely(ret != ARBITER_OK)) {
		LOG_WRN("set_timestamp fact %u failed: %d",
			(unsigned int)msg.fact_id, ret);
	}
}

ZBUS_LISTENER_DEFINE(arbiter_facts_sub, facts_cb);

/* ── Public API ───────────────────────────────────────────────── */

int arbiter_zbus_init(struct ARBITER_ctx *ctx)
{
	if (ctx == NULL) {
		return ARBITER_EINVAL;
	}

	zbus_ctx = ctx;
	LOG_INF("arbiter zbus integration initialized");

	return ARBITER_OK;
}

int arbiter_zbus_publish_result(const struct ARBITER_result *result)
{
	if (result == NULL) {
		return ARBITER_EINVAL;
	}

	struct arbiter_result_msg msg = {
		.mode = result->current_mode,
		.faults = result->raised_faults,
		.action_count = result->requested_action_count,
		.op_count = result->eval_op_count,
	};

	int ret = zbus_chan_pub(&arbiter_result_chan, &msg, K_NO_WAIT);

	if (unlikely(ret < 0)) {
		LOG_ERR("zbus result publish failed: %d", ret);
		return ret;
	}

	return ARBITER_OK;
}
