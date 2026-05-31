/* SPDX-License-Identifier: MIT */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <arbiter/arbiter.h>

LOG_MODULE_REGISTER(tmrvoter, LOG_LEVEL_INF);

extern const struct ARBITER_model ARBITER_generated_model;
static struct ARBITER_ctx ctx;

int main(void)
{
LOG_INF("=== arbiter tmr voter ===");
int ret = ARBITER_init(&ctx, &ARBITER_generated_model);
if (ret != ARBITER_OK) {
LOG_ERR("Init failed: %d", ret);
return ret;
}
LOG_INF("Model: %s  Facts: %u  Rules: %u",
ARBITER_generated_model.name,
ARBITER_generated_model.fact_count,
ARBITER_generated_model.rule_count);
return 0;
}
