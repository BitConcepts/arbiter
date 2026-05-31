/* SPDX-License-Identifier: MIT */

/**
 * Motion Planner Solver Sample
 *
 * zproj computes steering corrections toward a target waypoint
 * while applying obstacle repulsion.  Demonstrates trajectory
 * planning as a reasoning solver with safety-critical e-stop.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zproj/zproj.h>

LOG_MODULE_REGISTER(motion_plan, LOG_LEVEL_INF);

extern const struct zproj_model zproj_generated_model;

static struct zproj_ctx ctx;

enum {
	F_POS_X = 0, F_POS_Y, F_HEADING, F_SENSOR_VALID, F_ENABLE,
	F_WP_X, F_WP_Y,
	F_OBS_DIST, F_OBS_BEARING,
	F_GAIN_STEER, F_GAIN_REPULSE, F_STOP_DIST,
	F_DX, F_DY, F_DISTANCE, F_HEADING_ERR,
	F_STEER_CMD, F_REPULSION, F_OUTPUT, F_SPEED_PCT,
};

/* Simulated robot state */
static int32_t robot_x = 0, robot_y = 0;

void app_update_motors(void)
{
	int32_t steer = ctx.fact_values[F_OUTPUT].value;
	int32_t speed = ctx.fact_values[F_SPEED_PCT].value;

	/* Simple kinematics: move toward waypoint */
	int32_t dx = ctx.fact_values[F_DX].value;
	int32_t dy = ctx.fact_values[F_DY].value;

	if (speed > 0) {
		robot_x += (dx > 0) ? speed : (dx < 0) ? -speed : 0;
		robot_y += (dy > 0) ? speed : (dy < 0) ? -speed : 0;
	}

	LOG_INF("  motors: steer=%d  speed=%d%%  pos=(%d,%d)",
		steer, speed, robot_x, robot_y);
}

void app_halt_motors(void)
{
	LOG_WRN("  !! HALT");
}

void app_waypoint_reached(void)
{
	LOG_INF("  ** WAYPOINT REACHED at (%d, %d)", robot_x, robot_y);
}

int main(void)
{
	LOG_INF("=== zproj Motion Planner Solver ===");

	int ret = zproj_init(&ctx, &zproj_generated_model);

	if (ret != ZPROJ_OK) {
		LOG_ERR("Init failed: %d", ret);
		return ret;
	}

	zproj_set_bool(&ctx, F_ENABLE, true);
	zproj_set_bool(&ctx, F_SENSOR_VALID, true);

	/* Target waypoint at (5000, 3000) mm */
	zproj_set_i32(&ctx, F_WP_X, 5000);
	zproj_set_i32(&ctx, F_WP_Y, 3000);

	/* No obstacle initially */
	zproj_set_i32(&ctx, F_OBS_DIST, 50000);
	zproj_set_i32(&ctx, F_OBS_BEARING, 0);

	for (uint32_t t = 0; t < 80; t++) {
		zproj_set_i32(&ctx, F_POS_X, robot_x);
		zproj_set_i32(&ctx, F_POS_Y, robot_y);
		zproj_set_i32(&ctx, F_HEADING, 0);
		zproj_set_timestamp(&ctx, F_HEADING, k_uptime_get_32());

		/* Simulate obstacle appearing at t=30 */
		if (t >= 30 && t < 50) {
			zproj_set_i32(&ctx, F_OBS_DIST, 1000);
			zproj_set_i32(&ctx, F_OBS_BEARING, 90000);
		} else {
			zproj_set_i32(&ctx, F_OBS_DIST, 50000);
		}

		struct zproj_snapshot snap;
		struct zproj_result result;

		zproj_snapshot_begin(&ctx, &snap);
		zproj_eval(&zproj_generated_model, &snap, &result, NULL);

		if (t % 5 == 0) {
			uint16_t mode;

			zproj_get_mode(&result, &mode);
			LOG_INF("t=%u  dist=%d  steer=%d  speed=%d  mode=%u",
				t,
				ctx.fact_values[F_DISTANCE].value,
				ctx.fact_values[F_OUTPUT].value,
				ctx.fact_values[F_SPEED_PCT].value,
				mode);
		}

		k_sleep(K_MSEC(10));
	}

	return 0;
}
