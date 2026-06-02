/* SPDX-License-Identifier: MIT */

#include <arbiter/arbiter_trace_export.h>
#include <arbiter/arbiter.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(arbiter, CONFIG_ARBITER_LOG_LEVEL);

/* ── Static state (no dynamic allocation) ─────────────────────── */

static const struct ARBITER_trace_transport *active_transport;
static uint16_t seq_counter;

/* ── CRC-8 (polynomial 0x07) ─────────────────────────────────── */

static uint8_t crc8_update(uint8_t crc, const uint8_t *__restrict data,
			   size_t len)
{
	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (uint8_t bit = 0; bit < 8; bit++) {
			if (crc & 0x80) {
				crc = (uint8_t)((crc << 1) ^ ARBITER_TRACE_EXPORT_CRC8_POLY);
			} else {
				crc = (uint8_t)(crc << 1);
			}
		}
	}
	return crc;
}

/* ── Helpers ──────────────────────────────────────────────────── */

static void put_le16(uint8_t *__restrict buf, uint16_t val)
{
	buf[0] = (uint8_t)(val & 0xFF);
	buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

/* ── Public API ───────────────────────────────────────────────── */

int ARBITER_trace_export_init(const struct ARBITER_trace_transport *transport)
{
	if (transport == NULL || transport->send == NULL) {
		return ARBITER_EINVAL;
	}

	active_transport = transport;
	seq_counter = 0;

	LOG_INF("Trace export initialized");
	return ARBITER_OK;
}

int ARBITER_trace_export(const struct ARBITER_trace *trace)
{
	if (trace == NULL) {
		return ARBITER_EINVAL;
	}

	if (active_transport == NULL || active_transport->send == NULL) {
		return ARBITER_EINVAL;
	}

	for (uint16_t i = 0; i < trace->count; i++) {
		const struct ARBITER_trace_entry *e = &trace->entries[i];

		/*
		 * Frame layout:
		 *   [marker 1B][len_le16 2B][seq_le16 2B]
		 *   [rule_id_le16 2B][fired 1B][action_id_le16 2B]
		 *   [n_facts 1B][fact_ids n*2B][crc8 1B]
		 *
		 * Payload length = everything after len field, including crc.
		 */
		uint8_t n_facts = (uint8_t)e->input_fact_count;
		size_t payload_len = 2 + 2 + 1 + 2 + 1 +
				     ((size_t)n_facts * 2) + 1;
		size_t frame_len = 1 + 2 + payload_len;

		/*
		 * Stack-allocated frame buffer.  Maximum frame size with
		 * CONFIG_ARBITER_MAX_TRACE_INPUTS = 8 is:
		 *   1 + 2 + 2 + 2 + 1 + 2 + 1 + 16 + 1 = 28 bytes.
		 * Use a generous upper bound.
		 */
		uint8_t frame[3 + 2 + 2 + 1 + 2 + 1 +
			      (CONFIG_ARBITER_MAX_TRACE_INPUTS * 2) + 1];
		size_t pos = 0;

		/* Marker */
		frame[pos++] = ARBITER_TRACE_EXPORT_MARKER;

		/* Payload length (LE16) */
		put_le16(&frame[pos], (uint16_t)payload_len);
		pos += 2;

		/* Sequence number (LE16) — wraps at UINT16_MAX */
		put_le16(&frame[pos], seq_counter);
		pos += 2;
		seq_counter++;

		/* Rule ID (LE16) */
		put_le16(&frame[pos], e->rule_id);
		pos += 2;

		/* Fired flag */
		frame[pos++] = e->condition_result ? 1U : 0U;

		/* Action ID (LE16) */
		put_le16(&frame[pos], e->action_id);
		pos += 2;

		/* Number of input facts */
		frame[pos++] = n_facts;

		/* Input fact IDs (LE16 each) */
		for (uint8_t f = 0; f < n_facts; f++) {
			put_le16(&frame[pos], e->input_facts[f]);
			pos += 2;
		}

		/* CRC-8 over everything after the marker */
		uint8_t crc = crc8_update(0x00, &frame[1], pos - 1);

		frame[pos++] = crc;

		int ret = active_transport->send(frame, pos,
						 active_transport->user_data);
		if (unlikely(ret < 0)) {
			LOG_ERR("Trace export send failed: %d", ret);
			return ret;
		}
	}

	return ARBITER_OK;
}
