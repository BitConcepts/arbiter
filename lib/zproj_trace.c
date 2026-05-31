/* SPDX-License-Identifier: MIT */

#include <zproj/zproj.h>
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zproj, CONFIG_ZPROJ_LOG_LEVEL);

void zproj_trace_init(struct zproj_trace *trace,
		      struct zproj_trace_entry *buffer,
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

int zproj_trace_record(struct zproj_trace *trace,
		       const struct zproj_trace_entry *entry)
{
	if (trace == NULL || entry == NULL) {
		return ZPROJ_EINVAL;
	}

	if (trace->entries == NULL) {
		return ZPROJ_EINVAL;
	}

	if (trace->count >= trace->capacity) {
		trace->overflow = true;
		return ZPROJ_ETRACE_FULL;
	}

	memcpy(&trace->entries[trace->count], entry,
	       sizeof(struct zproj_trace_entry));
	trace->count++;

	return ZPROJ_OK;
}

const struct zproj_trace_entry *zproj_trace_get(const struct zproj_trace *trace,
						uint16_t index)
{
	if (trace == NULL || index >= trace->count) {
		return NULL;
	}

	return &trace->entries[index];
}

void zproj_trace_reset(struct zproj_trace *trace)
{
	if (trace == NULL) {
		return;
	}

	trace->count = 0;
	trace->overflow = false;
}
