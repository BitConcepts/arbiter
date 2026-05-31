/* SPDX-License-Identifier: MIT */

#include <zproj/zproj.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zproj, CONFIG_ZPROJ_LOG_LEVEL);

static const struct device *wdt_dev;
static int wdt_channel_id = -1;

int zproj_watchdog_init(const struct device *wdt, uint32_t timeout_ms)
{
	if (wdt == NULL || !device_is_ready(wdt)) {
		LOG_ERR("Watchdog device not ready");
		return ZPROJ_EINVAL;
	}

	struct wdt_timeout_cfg cfg = {
		.window = {
			.min = 0,
			.max = timeout_ms,
		},
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC,
	};

	wdt_channel_id = wdt_install_timeout(wdt, &cfg);
	if (wdt_channel_id < 0) {
		LOG_ERR("Failed to install watchdog timeout: %d",
			wdt_channel_id);
		return wdt_channel_id;
	}

	int ret = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);

	if (ret < 0) {
		LOG_ERR("Failed to setup watchdog: %d", ret);
		return ret;
	}

	wdt_dev = wdt;
	LOG_INF("zproj watchdog initialized (timeout=%u ms)", timeout_ms);

	return ZPROJ_OK;
}

void zproj_watchdog_feed(void)
{
	if (wdt_dev == NULL || wdt_channel_id < 0) {
		return;
	}

	int ret = wdt_feed(wdt_dev, wdt_channel_id);

	if (ret < 0) {
		LOG_ERR("Failed to feed watchdog: %d", ret);
	}
}
