/* SPDX-License-Identifier: MIT */

/**
 * Mesh Network Router — Reasoning Engine
 *
 * arbiter reasons about network topology, link quality, congestion,
 * and power — computing derived metrics (link margin, parent score,
 * congestion level) and making routing decisions (parent switch,
 * TX power adjustment, sleep mode).
 *
 * Inspired by Thread/OpenThread mesh networking patterns.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <arbiter/arbiter.h>

LOG_MODULE_REGISTER(mesh_router, LOG_LEVEL_INF);

extern const struct ARBITER_model ARBITER_generated_model;
static struct ARBITER_ctx ctx;

/* Callbacks */
void app_start_network_discovery(void) { LOG_WRN("NET: Starting discovery"); }
void app_increase_tx_power(void) { LOG_INF("NET: Increasing TX power"); }
void app_switch_parent(void) { LOG_INF("NET: Switching parent"); }
void app_reduce_tx_rate(void) { LOG_INF("NET: Reducing TX rate"); }
void app_drop_low_priority(void) { LOG_WRN("NET: Dropping low-priority"); }
void app_request_sleepy_mode(void) { LOG_INF("NET: Requesting sleepy mode"); }

static void run_scenario(const char *name,
			 int32_t rssi, uint32_t lqi,
			 uint32_t neighbors, uint32_t route_cost,
			 uint32_t alt_cost, uint32_t queue,
			 uint32_t battery, bool is_router)
{
	LOG_INF("--- %s ---", name);

	/* Feed link metrics — fact indices depend on canonical sort */
	/* Alphabetical: calc.*, link.*, node.*, queue.* */
	ARBITER_set_i32(&ctx, 0, 0);        /* calc.congestion_level (computed) */
	ARBITER_set_i32(&ctx, 1, 0);        /* calc.link_margin (computed) */
	ARBITER_set_i32(&ctx, 2, 0);        /* calc.parent_score (computed) */
	ARBITER_set_bool(&ctx, 3, false);   /* calc.should_switch (computed) */
	ARBITER_set_u32(&ctx, 4, alt_cost); /* link.alt_route_cost */
	ARBITER_set_u32(&ctx, 5, neighbors);/* link.neighbor_count */
	ARBITER_set_u32(&ctx, 6, lqi);      /* link.parent_lqi */
	ARBITER_set_i32(&ctx, 7, rssi);     /* link.parent_rssi */
	ARBITER_set_u32(&ctx, 8, route_cost); /* link.route_cost */
	ARBITER_set_u32(&ctx, 9, battery);  /* node.battery_pct */
	ARBITER_set_u32(&ctx, 10, 0);       /* node.child_count */
	ARBITER_set_bool(&ctx, 11, false);  /* node.is_leader */
	ARBITER_set_bool(&ctx, 12, is_router); /* node.is_router */
	ARBITER_set_u32(&ctx, 13, queue);   /* queue.depth */
	ARBITER_set_u32(&ctx, 14, 0);       /* queue.drop_count */

	struct ARBITER_snapshot snap;
	struct ARBITER_result result;

	ARBITER_snapshot_begin(&ctx, &snap);
	ARBITER_eval(&ARBITER_generated_model, &snap, &result, NULL);

	uint16_t mode;

	ARBITER_get_mode(&result, &mode);
	LOG_INF("  Mode: %u  Actions: %u  margin=%d  score=%d  congest=%d",
		mode, result.requested_action_count,
		ctx.fact_values[1].value,  /* link_margin */
		ctx.fact_values[2].value,  /* parent_score */
		ctx.fact_values[0].value); /* congestion */
}

int main(void)
{
	LOG_INF("=== arbiter Mesh Router Reasoning Engine ===");

	int ret = ARBITER_init(&ctx, &ARBITER_generated_model);

	if (ret != ARBITER_OK) {
		LOG_ERR("Init failed: %d", ret);
		return ret;
	}

	/* Good link, low congestion */
	run_scenario("Nominal",
		     -65, 20000, 5, 4, 8, 10, 80, true);

	/* Weak link */
	run_scenario("Degraded link (RSSI -95 dBm)",
		     -95, 8000, 3, 8, 6, 10, 80, true);

	/* High congestion */
	run_scenario("Congested (queue 80%)",
		     -70, 18000, 4, 5, 7, 210, 80, true);

	/* Better parent available */
	run_scenario("Parent switch (alt cost much lower)",
		     -72, 16000, 4, 12, 3, 30, 80, true);

	/* Isolated */
	run_scenario("Isolated (0 neighbors)",
		     0, 0, 0, 255, 255, 0, 80, false);

	/* Low battery end device */
	run_scenario("Low battery end device",
		     -70, 18000, 4, 5, 7, 10, 10, false);

	return 0;
}
