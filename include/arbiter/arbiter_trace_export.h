/* SPDX-License-Identifier: MIT */

#ifndef ARBITER_TRACE_EXPORT_H_
#define ARBITER_TRACE_EXPORT_H_

#include <stdint.h>
#include <stddef.h>
#include <arbiter/arbiter_trace.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Frame start marker for trace export protocol. */
#define ARBITER_TRACE_EXPORT_MARKER 0xAB

/** CRC-8 polynomial used by the trace export framing. */
#define ARBITER_TRACE_EXPORT_CRC8_POLY 0x07

/**
 * Transport abstraction for remote trace export.
 *
 * The engine serialises each trace entry into a binary frame and
 * calls @c send to push it to a transport (UART, network, etc.).
 */
struct ARBITER_trace_transport {
	/**
	 * @brief Send a binary frame.
	 *
	 * @param buf       Serialized frame bytes.
	 * @param len       Length of @p buf in bytes.
	 * @param user_data Opaque pointer passed through from the transport.
	 * @return 0 on success, negative errno on failure.
	 */
	int (*send)(const uint8_t *buf, size_t len, void *user_data);

	/** Opaque user data forwarded to @c send. */
	void *user_data;
};

/**
 * @brief Initialize the remote trace exporter.
 *
 * Registers the transport and resets the internal sequence counter.
 *
 * @param transport Transport to use for sending frames.
 * @return 0 on success, -EINVAL if @p transport or its send callback is NULL.
 */
int ARBITER_trace_export_init(const struct ARBITER_trace_transport *transport);

/**
 * @brief Export all entries from a trace buffer.
 *
 * Each entry is serialised into a binary frame:
 *   [0xAB][len_le16][seq_u16][rule_id_u16][fired_u8]
 *   [action_id_u16][n_facts_u8][fact_ids...][crc8]
 *
 * @param trace Trace buffer to export.
 * @return 0 on success, negative errno on transport failure.
 */
int ARBITER_trace_export(const struct ARBITER_trace *trace);

#ifdef __cplusplus
}
#endif

#endif /* ARBITER_TRACE_EXPORT_H_ */
