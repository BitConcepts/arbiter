/* SPDX-License-Identifier: MIT */

#include <zephyr/ztest.h>
#include <zproj/zproj.h>
#include <string.h>

extern int zproj_blob_load(const uint8_t *blob, size_t blob_len,
			   struct zproj_model *model_out);

/* Minimal valid ZRMB header (80 bytes) */
static uint8_t valid_blob[80] = {
	'Z', 'R', 'M', 'B',       /* magic */
	0x01, 0x00,                /* version = 1 */
	0x00, 0x00,                /* flags = 0 */
	0x50, 0x00, 0x00, 0x00,    /* header_len = 80 */
	0x50, 0x00, 0x00, 0x00,    /* total_len = 80 */
	/* 32 bytes model_hash (zeros) */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 32 bytes schema_hash (zeros) */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x00, 0x00, 0x00, 0x00,    /* crc32 = 0 (skip check) */
};

ZTEST_SUITE(zproj_blob, NULL, NULL, NULL, NULL, NULL);

ZTEST(zproj_blob, test_valid_blob)
{
	struct zproj_model model;
	int ret = zproj_blob_load(valid_blob, sizeof(valid_blob), &model);

	zassert_equal(ret, ZPROJ_OK, "Valid blob should load successfully");
}

ZTEST(zproj_blob, test_null_params)
{
	struct zproj_model model;

	zassert_equal(zproj_blob_load(NULL, 80, &model), ZPROJ_EINVAL);
	zassert_equal(zproj_blob_load(valid_blob, 80, NULL), ZPROJ_EINVAL);
}

ZTEST(zproj_blob, test_truncated)
{
	struct zproj_model model;

	zassert_equal(zproj_blob_load(valid_blob, 10, &model), ZPROJ_EMODEL);
}

ZTEST(zproj_blob, test_bad_magic)
{
	uint8_t bad[80];

	memcpy(bad, valid_blob, sizeof(bad));
	bad[0] = 'X';

	struct zproj_model model;

	zassert_equal(zproj_blob_load(bad, sizeof(bad), &model), ZPROJ_EMODEL);
}

ZTEST(zproj_blob, test_bad_version)
{
	uint8_t bad[80];

	memcpy(bad, valid_blob, sizeof(bad));
	bad[4] = 99; /* invalid version */

	struct zproj_model model;

	zassert_equal(zproj_blob_load(bad, sizeof(bad), &model), ZPROJ_EMODEL);
}
