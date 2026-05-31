/* SPDX-License-Identifier: MIT */

#include <arbiter/arbiter.h>
#include <string.h>
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

	if (trace->count >= trace->capacity) {
		trace->overflow = true;
		return ARBITER_ETRACE_FULL;
	}

	memcpy(&trace->entries[trace->count], entry,
	       sizeof(struct ARBITER_trace_entry));
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
