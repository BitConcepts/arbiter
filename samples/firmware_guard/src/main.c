/* SPDX-License-Identifier: MIT */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zproj/zproj.h>

LOG_MODULE_REGISTER(firmwareguard, LOG_LEVEL_INF);

extern const struct zproj_model zproj_generated_model;
static struct zproj_ctx ctx;

int main(void)
{
LOG_INF("=== zproj firmware guard ===");
int ret = zproj_init(&ctx, &zproj_generated_model);
if (ret != ZPROJ_OK) {
LOG_ERR("Init failed: %d", ret);
return ret;
}
LOG_INF("Model: %s  Facts: %u  Rules: %u",
zproj_generated_model.name,
zproj_generated_model.fact_count,
zproj_generated_model.rule_count);
return 0;
}
