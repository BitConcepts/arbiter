/* SPDX-License-Identifier: MIT */

#ifndef ARBITER_TRACE_H_
#define ARBITER_TRACE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_ARBITER_MAX_TRACE_INPUTS
#define CONFIG_ARBITER_MAX_TRACE_INPUTS 8
#endif

#ifndef CONFIG_ARBITER_MAX_TRACE_ENTRIES
#define CONFIG_ARBITER_MAX_TRACE_ENTRIES 64
#endif

/** Single trace entry recording one rule evaluation. */
struct ARBITER_trace_entry {
	uint16_t rule_id;
	bool condition_result;
	uint16_t input_facts[CONFIG_ARBITER_MAX_TRACE_INPUTS];
	uint16_t input_fact_count;
	uint16_t action_id;
	const char *reason;
};

/** Bounded trace buffer. */
struct ARBITER_trace {
	struct ARBITER_trace_entry *entries;
	uint16_t count;
	uint16_t capacity;
	bool overflow;
};

/** Initialize a trace buffer with pre-allocated storage. */
void ARBITER_trace_init(struct ARBITER_trace *trace,
		      struct ARBITER_trace_entry *buffer,
		      uint16_t capacity);

/** Record a trace entry. Returns 0 on success, ARBITER_ETRACE_FULL on overflow. */
int ARBITER_trace_record(struct ARBITER_trace *trace,
		       const struct ARBITER_trace_entry *entry);

/** Get trace entry by index. Returns NULL if out of bounds. */
const struct ARBITER_trace_entry *ARBITER_trace_get(const struct ARBITER_trace *trace,
						uint16_t index);

/** Reset trace buffer to empty. */
void ARBITER_trace_reset(struct ARBITER_trace *trace);

#ifdef __cplusplus
}
#endif

#endif /* ARBITER_TRACE_H_ */
