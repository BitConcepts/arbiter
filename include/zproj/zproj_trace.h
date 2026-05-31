/* SPDX-License-Identifier: MIT */

#ifndef ZPROJ_TRACE_H_
#define ZPROJ_TRACE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_ZPROJ_MAX_TRACE_INPUTS
#define CONFIG_ZPROJ_MAX_TRACE_INPUTS 8
#endif

#ifndef CONFIG_ZPROJ_MAX_TRACE_ENTRIES
#define CONFIG_ZPROJ_MAX_TRACE_ENTRIES 64
#endif

/** Single trace entry recording one rule evaluation. */
struct zproj_trace_entry {
	uint16_t rule_id;
	bool condition_result;
	uint16_t input_facts[CONFIG_ZPROJ_MAX_TRACE_INPUTS];
	uint16_t input_fact_count;
	uint16_t action_id;
	const char *reason;
};

/** Bounded trace buffer. */
struct zproj_trace {
	struct zproj_trace_entry *entries;
	uint16_t count;
	uint16_t capacity;
	bool overflow;
};

/** Initialize a trace buffer with pre-allocated storage. */
void zproj_trace_init(struct zproj_trace *trace,
		      struct zproj_trace_entry *buffer,
		      uint16_t capacity);

/** Record a trace entry. Returns 0 on success, ZPROJ_ETRACE_FULL on overflow. */
int zproj_trace_record(struct zproj_trace *trace,
		       const struct zproj_trace_entry *entry);

/** Get trace entry by index. Returns NULL if out of bounds. */
const struct zproj_trace_entry *zproj_trace_get(const struct zproj_trace *trace,
						uint16_t index);

/** Reset trace buffer to empty. */
void zproj_trace_reset(struct zproj_trace *trace);

#ifdef __cplusplus
}
#endif

#endif /* ZPROJ_TRACE_H_ */
