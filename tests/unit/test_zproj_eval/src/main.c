/* SPDX-License-Identifier: MIT */

#include <zephyr/ztest.h>
#include <zproj/zproj.h>

/* Inline test model: 2 facts, 1 rule, 1 condition, 1 action */

static const struct zproj_fact_def test_facts[] = {
	{ .id = 0, .type = ZPROJ_FACT_UINT32, .range_min = 0, .range_max = 100,
	  .default_value = 50, .stale_after_ms = 0, .safety_relevant = false,
	  .name = "sensor.value" },
	{ .id = 1, .type = ZPROJ_FACT_BOOL, .range_min = 0, .range_max = 1,
	  .default_value = 0, .stale_after_ms = 0, .safety_relevant = false,
	  .name = "trigger.active" },
};

static const struct zproj_condition_def test_conditions[] = {
	{ .fact_id = 1, .op = ZPROJ_OP_EQ, .value = 1,
	  .group = ZPROJ_COND_ALL, .group_index = 0, .next = UINT16_MAX },
};

static const struct zproj_action_def test_actions[] = {
	{ .id = 0, .type = ZPROJ_ACTION_CALLBACK, .target_fact_id = 0,
	  .target_value = 0, .callback = NULL, .must_complete_within_ms = 0,
	  .safe_state_action = false, .name = "test_action" },
};

static const struct zproj_rule_def test_rules[] = {
	{ .id = 0, .rule_class = ZPROJ_RULE_INFERENCE,
	  .condition_start = 0, .condition_count = 1,
	  .action_start = 0, .action_count = 1,
	  .safety_goal_id = UINT16_MAX, .set_mode = 1,
	  .safety_critical = false,
	  .name = "rule.trigger", .explanation = "Trigger is active." },
};

static const char *test_modes[] = { "mode.idle", "mode.active" };

static const struct zproj_model test_model = {
	.name = "test_model",
	.model_hash = {0},
	.schema_hash = {0},
	.fact_count = 2,
	.rule_count = 1,
	.condition_count = 1,
	.action_count = 1,
	.mode_count = 2,
	.facts = test_facts,
	.rules = test_rules,
	.conditions = test_conditions,
	.actions = test_actions,
	.mode_names = test_modes,
};

ZTEST_SUITE(zproj_eval, NULL, NULL, NULL, NULL, NULL);

ZTEST(zproj_eval, test_init)
{
	struct zproj_ctx ctx;
	int ret = zproj_init(&ctx, &test_model);

	zassert_equal(ret, ZPROJ_OK, "init should succeed");
	zassert_true(ctx.initialized, "ctx should be initialized");
}

ZTEST(zproj_eval, test_init_null)
{
	zassert_equal(zproj_init(NULL, &test_model), ZPROJ_EINVAL);

	struct zproj_ctx ctx;

	zassert_equal(zproj_init(&ctx, NULL), ZPROJ_EINVAL);
}

ZTEST(zproj_eval, test_set_facts)
{
	struct zproj_ctx ctx;

	zproj_init(&ctx, &test_model);

	zassert_equal(zproj_set_u32(&ctx, 0, 75), ZPROJ_OK);
	zassert_equal(zproj_set_bool(&ctx, 1, true), ZPROJ_OK);

	/* Out of range fact ID */
	zassert_equal(zproj_set_u32(&ctx, 99, 0), ZPROJ_ERANGE);

	/* Type mismatch */
	zassert_equal(zproj_set_bool(&ctx, 0, true), ZPROJ_EINVAL);
}

ZTEST(zproj_eval, test_eval_rule_fires)
{
	struct zproj_ctx ctx;
	struct zproj_snapshot snap;
	struct zproj_result result;

	zproj_init(&ctx, &test_model);
	zproj_set_bool(&ctx, 1, true); /* trigger.active = true */
	zproj_snapshot_begin(&ctx, &snap);

	int ret = zproj_eval(&test_model, &snap, &result, NULL);

	zassert_equal(ret, ZPROJ_OK);

	uint16_t mode;

	zproj_get_mode(&result, &mode);
	zassert_equal(mode, 1, "Should transition to mode.active");
	zassert_equal(result.requested_action_count, 1,
		      "Should request 1 action");
}

ZTEST(zproj_eval, test_eval_rule_skips)
{
	struct zproj_ctx ctx;
	struct zproj_snapshot snap;
	struct zproj_result result;

	zproj_init(&ctx, &test_model);
	zproj_set_bool(&ctx, 1, false); /* trigger.active = false */
	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&test_model, &snap, &result, NULL);

	uint16_t mode;

	zproj_get_mode(&result, &mode);
	zassert_equal(mode, 0, "Should stay in mode.idle");
	zassert_equal(result.requested_action_count, 0);
}

ZTEST(zproj_eval, test_determinism)
{
	struct zproj_ctx ctx;
	struct zproj_snapshot snap;
	struct zproj_result r1, r2;

	zproj_init(&ctx, &test_model);
	zproj_set_bool(&ctx, 1, true);

	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&test_model, &snap, &r1, NULL);
	zproj_eval(&test_model, &snap, &r2, NULL);

	zassert_equal(r1.current_mode, r2.current_mode);
	zassert_equal(r1.raised_faults, r2.raised_faults);
	zassert_equal(r1.requested_action_count, r2.requested_action_count);
	zassert_equal(r1.eval_op_count, r2.eval_op_count);
}

ZTEST(zproj_eval, test_trace)
{
	struct zproj_ctx ctx;
	struct zproj_snapshot snap;
	struct zproj_result result;
	struct zproj_trace_entry trace_buf[8];
	struct zproj_trace trace;

	zproj_trace_init(&trace, trace_buf, 8);
	zproj_init(&ctx, &test_model);
	zproj_set_bool(&ctx, 1, true);
	zproj_snapshot_begin(&ctx, &snap);
	zproj_eval(&test_model, &snap, &result, &trace);

	zassert_equal(trace.count, 1, "Should have 1 trace entry");
	zassert_false(trace.overflow);

	const struct zproj_trace_entry *e = zproj_trace_get(&trace, 0);

	zassert_not_null(e);
	zassert_true(e->condition_result, "Rule should have fired");
}
