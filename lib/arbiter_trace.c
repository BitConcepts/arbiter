/* SPDX-License-Identifier: MIT */

#include <arbiter/arbiter.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(arbiter, CONFIG_ARBITER_LOG_LEVEL);

void ARBITER_trace_init(struct ARBITER_trace *trace,
		      struct ARBITER_trace_entry *buffer,
		      uint16_t capacity)
{
	if (trace == NULL) {
		return;
	}

	trace->entries = buffer;
	trace->count = 0;
	trace->capacity = capacity;
	trace->overflow = false;
}

int ARBITER_trace_record(struct ARBITER_trace *trace,
		       const struct ARBITER_trace_entry *entry)
{
	if (trace == NULL || entry == NULL) {
		return ARBITER_EINVAL;
	}

	if (trace->entries == NULL) {
		return ARBITER_EINVAL;
	}

	if (unlikely(trace->count >= trace->capacity)) {
		trace->overflow = true;
		return ARBITER_ETRACE_FULL;
	}

	/* Struct assignment: compiler emits optimal reg-to-reg moves
	 * or word-sized stores. Avoids memcpy function call overhead
	 * and lets the compiler see through the copy for optimization.
	 */
	trace->entries[trace->count] = *entry;
	trace->count++;

	return ARBITER_OK;
}

const struct ARBITER_trace_entry *ARBITER_trace_get(const struct ARBITER_trace *trace,
						uint16_t index)
{
	if (trace == NULL || index >= trace->count) {
		return NULL;
	}

	return &trace->entries[index];
}

void ARBITER_trace_reset(struct ARBITER_trace *trace)
{
	if (trace == NULL) {
		return;
	}

	trace->count = 0;
	trace->overflow = false;
}
