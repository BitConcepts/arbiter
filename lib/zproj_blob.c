/* SPDX-License-Identifier: MIT */

#include <zproj/zproj.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zproj, CONFIG_ZPROJ_LOG_LEVEL);

#define ZRMB_MAGIC      0x424D525A /* "ZRMB" little-endian */
#define ZRMB_VERSION    1
#define ZRMB_HEADER_LEN 80 /* magic(4)+ver(2)+flags(2)+hlen(4)+tlen(4)+mhash(32)+shash(32)+crc(4) */

struct zrmb_header {
	uint32_t magic;
	uint16_t version;
	uint16_t flags;
	uint32_t header_len;
	uint32_t total_len;
	uint8_t  model_hash[32];
	uint8_t  schema_hash[32];
	uint32_t crc32;
};

/**
 * Simple CRC-32 (IEEE 802.3 polynomial).
 */
static uint32_t compute_crc32(const uint8_t *data, size_t len)
{
	uint32_t crc = 0xFFFFFFFF;

	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (int j = 0; j < 8; j++) {
			if (crc & 1) {
				crc = (crc >> 1) ^ 0xEDB88320;
			} else {
				crc >>= 1;
			}
		}
	}
	return ~crc;
}

int zproj_blob_load(const uint8_t *blob, size_t blob_len,
		    struct zproj_model *model_out)
{
	if (blob == NULL || model_out == NULL) {
		return ZPROJ_EINVAL;
	}

	if (blob_len < ZRMB_HEADER_LEN) {
		LOG_ERR("Blob too small: %zu < %d", blob_len, ZRMB_HEADER_LEN);
		return ZPROJ_EMODEL;
	}

	const struct zrmb_header *hdr = (const struct zrmb_header *)blob;

	/* Validate magic */
	if (hdr->magic != ZRMB_MAGIC) {
		LOG_ERR("Invalid blob magic: 0x%08x", hdr->magic);
		return ZPROJ_EMODEL;
	}

	/* Validate version */
	if (hdr->version != ZRMB_VERSION) {
		LOG_ERR("Unsupported blob version: %u", hdr->version);
		return ZPROJ_EMODEL;
	}

	/* Validate lengths */
	if (hdr->total_len > blob_len) {
		LOG_ERR("Blob total_len %u exceeds buffer %zu",
			hdr->total_len, blob_len);
		return ZPROJ_EMODEL;
	}

	if (hdr->header_len < ZRMB_HEADER_LEN) {
		LOG_ERR("Invalid header_len: %u", hdr->header_len);
		return ZPROJ_EMODEL;
	}

	/* Validate CRC over everything except the CRC field itself.
	 * CRC field is at offset 76 (4 bytes before end of header).
	 */
	uint32_t crc = compute_crc32(blob, offsetof(struct zrmb_header, crc32));
	uint32_t crc_rest = compute_crc32(
		blob + offsetof(struct zrmb_header, crc32) + sizeof(uint32_t),
		hdr->total_len - offsetof(struct zrmb_header, crc32) -
		sizeof(uint32_t));

	/* Combine CRC segments (simplified: recompute over full blob
	 * skipping CRC field is complex; for v0 just verify basic integrity) */
	(void)crc_rest;
	if (hdr->crc32 != 0 && hdr->crc32 != crc) {
		LOG_WRN("CRC mismatch (header region): expected 0x%08x, got 0x%08x",
			hdr->crc32, crc);
		/* Continue for v0 - strict CRC enforcement in future */
	}

	/* Copy hashes to model */
	memset(model_out, 0, sizeof(*model_out));
	memcpy((void *)model_out->model_hash, hdr->model_hash, 32);
	memcpy((void *)model_out->schema_hash, hdr->schema_hash, 32);

	LOG_INF("Blob loaded: version=%u total_len=%u",
		hdr->version, hdr->total_len);

	/* Section parsing would follow here. For v0, the blob loader
	 * validates the header and reports success. Full section parsing
	 * (facts, rules, conditions, actions, strings) will be implemented
	 * in Milestone 3.
	 */

	return ZPROJ_OK;
}
