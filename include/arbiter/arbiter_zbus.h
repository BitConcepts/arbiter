/* SPDX-License-Identifier: MIT */

#ifndef ARBITER_ZBUS_H_
#define ARBITER_ZBUS_H_

#include <stdint.h>
#include <arbiter/arbiter_model.h>
#include <zephyr/zbus/zbus.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Inbound message: set a fact value with timestamp. */
struct arbiter_facts_msg {
	arbiter_index_t fact_id;
	int32_t value;
	uint32_t timestamp_ms;
};

/** Outbound message: evaluation result summary. */
struct arbiter_result_msg {
	uint16_t mode;
	uint32_t faults;
	uint16_t action_count;
	uint32_t op_count;
};

/* Channel declarations (defined in arbiter_zbus.c). */
ZBUS_CHAN_DECLARE(arbiter_facts_chan, arbiter_result_chan);

/**
 * @brief Initialize the arbiter zbus integration.
 *
 * @param ctx Initialized arbiter context for fact updates.
 * @return 0 on success, -EINVAL if @p ctx is NULL.
 */
int arbiter_zbus_init(struct ARBITER_ctx *ctx);

/**
 * @brief Publish an evaluation result to the result channel.
 *
 * @param result Evaluation result to publish.
 * @return 0 on success, negative errno on failure.
 */
int arbiter_zbus_publish_result(const struct ARBITER_result *result);

#ifdef __cplusplus
}
#endif

#endif /* ARBITER_ZBUS_H_ */
