/* SPDX-License-Identifier: MIT */

#include <zephyr/ztest.h>
#include <arbiter/arbiter.h>
#include <string.h>

extern int ARBITER_blob_load(const uint8_t *blob, size_t blob_len,
			   struct ARBITER_model *model_out);

/* Minimal valid ZRMB header (84 bytes) */
static uint8_t valid_blob[84] = {
	'Z', 'R', 'M', 'B',       /* magic */
	0x01, 0x00,                /* version = 1 */
	0x00, 0x00,                /* flags = 0 */
	0x54, 0x00, 0x00, 0x00,    /* header_len = 84 */
	0x54, 0x00, 0x00, 0x00,    /* total_len = 84 */
	/* 32 bytes model_hash (zeros) */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 32 bytes schema_hash (zeros) */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x00, 0x00, 0x00, 0x00,    /* crc32 = 0 (skip check) */
};

ZTEST_SUITE(ARBITER_blob, NULL, NULL, NULL, NULL, NULL);

ZTEST(ARBITER_blob, test_valid_blob)
{
	struct ARBITER_model model;
	int ret = ARBITER_blob_load(valid_blob, sizeof(valid_blob), &model);

	zassert_equal(ret, ARBITER_OK, "Valid blob should load successfully");
}

ZTEST(ARBITER_blob, test_null_params)
{
	struct ARBITER_model model;

	zassert_equal(ARBITER_blob_load(NULL, sizeof(valid_blob), &model), ARBITER_EINVAL);
	zassert_equal(ARBITER_blob_load(valid_blob, sizeof(valid_blob), NULL), ARBITER_EINVAL);
}

ZTEST(ARBITER_blob, test_truncated)
{
	struct ARBITER_model model;

	zassert_equal(ARBITER_blob_load(valid_blob, 10, &model), ARBITER_EMODEL);
}

ZTEST(ARBITER_blob, test_bad_magic)
{
	uint8_t bad[84];

	memcpy(bad, valid_blob, sizeof(bad));
	bad[0] = 'X';

	struct ARBITER_model model;

	zassert_equal(ARBITER_blob_load(bad, sizeof(bad), &model), ARBITER_EMODEL);
}

ZTEST(ARBITER_blob, test_bad_version)
{
	uint8_t bad[84];

	memcpy(bad, valid_blob, sizeof(bad));
	bad[4] = 99; /* invalid version */

	struct ARBITER_model model;

	zassert_equal(ARBITER_blob_load(bad, sizeof(bad), &model), ARBITER_EMODEL);
}
