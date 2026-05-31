/* SPDX-License-Identifier: MIT */

#include <zephyr/ztest.h>
#include <arbiter/arbiter.h>

/* Inline test model: 2 facts, 1 rule, 1 condition, 1 action */

static const struct ARBITER_fact_def test_facts[] = {
	{ .id = 0, .type = ARBITER_FACT_UINT32, .range_min = 0, .range_max = 100,
	  .default_value = 50, .stale_after_ms = 0, .safety_relevant = false,
	  .name = "sensor.value" },
	{ .id = 1, .type = ARBITER_FACT_BOOL, .range_min = 0, .range_max = 1,
	  .default_value = 0, .stale_after_ms = 0, .safety_relevant = false,
	  .name = "trigger.active" },
};

static const struct ARBITER_condition_def test_conditions[] = {
	{ .fact_id = 1, .op = ARBITER_OP_EQ, .value = 1,
	  .group = ARBITER_COND_ALL, .group_index = 0, .next = UINT16_MAX },
};

static const struct ARBITER_action_def test_actions[] = {
	{ .id = 0, .type = ARBITER_ACTION_CALLBACK, .target_fact_id = 0,
	  .target_value = 0, .callback = NULL, .must_complete_within_ms = 0,
	  .safe_state_action = false, .name = "test_action" },
};

static const struct ARBITER_rule_def test_rules[] = {
	{ .id = 0, .rule_class = ARBITER_RULE_INFERENCE,
	  .condition_start = 0, .condition_count = 1,
	  .action_start = 0, .action_count = 1,
	  .safety_goal_id = UINT16_MAX, .set_mode = 1,
	  .safety_critical = false,
	  .name = "rule.trigger", .explanation = "Trigger is active." },
};

static const char *test_modes[] = { "mode.idle", "mode.active" };

static const struct ARBITER_model test_model = {
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

ZTEST_SUITE(ARBITER_eval, NULL, NULL, NULL, NULL, NULL);

ZTEST(ARBITER_eval, test_init)
{
	struct ARBITER_ctx ctx;
	int ret = ARBITER_init(&ctx, &test_model);

	zassert_equal(ret, ARBITER_OK, "init should succeed");
	zassert_true(ctx.initialized, "ctx should be initialized");
}

ZTEST(ARBITER_eval, test_init_null)
{
	zassert_equal(ARBITER_init(NULL, &test_model), ARBITER_EINVAL);

	struct ARBITER_ctx ctx;

	zassert_equal(ARBITER_init(&ctx, NULL), ARBITER_EINVAL);
}

ZTEST(ARBITER_eval, test_set_facts)
{
	struct ARBITER_ctx ctx;

	ARBITER_init(&ctx, &test_model);

	zassert_equal(ARBITER_set_u32(&ctx, 0, 75), ARBITER_OK);
	zassert_equal(ARBITER_set_bool(&ctx, 1, true), ARBITER_OK);

	/* Out of range fact ID */
	zassert_equal(ARBITER_set_u32(&ctx, 99, 0), ARBITER_ERANGE);

	/* Type mismatch */
	zassert_equal(ARBITER_set_bool(&ctx, 0, true), ARBITER_EINVAL);
}

ZTEST(ARBITER_eval, test_eval_rule_fires)
{
	struct ARBITER_ctx ctx;
	struct ARBITER_snapshot snap;
	struct ARBITER_result result;

	ARBITER_init(&ctx, &test_model);
	ARBITER_set_bool(&ctx, 1, true); /* trigger.active = true */
	ARBITER_snapshot_begin(&ctx, &snap);

	int ret = ARBITER_eval(&test_model, &snap, &result, NULL);

	zassert_equal(ret, ARBITER_OK);

	uint16_t mode;

	ARBITER_get_mode(&result, &mode);
	zassert_equal(mode, 1, "Should transition to mode.active");
	zassert_equal(result.requested_action_count, 1,
		      "Should request 1 action");
}

ZTEST(ARBITER_eval, test_eval_rule_skips)
{
	struct ARBITER_ctx ctx;
	struct ARBITER_snapshot snap;
	struct ARBITER_result result;

	ARBITER_init(&ctx, &test_model);
	ARBITER_set_bool(&ctx, 1, false); /* trigger.active = false */
	ARBITER_snapshot_begin(&ctx, &snap);
	ARBITER_eval(&test_model, &snap, &result, NULL);

	uint16_t mode;

	ARBITER_get_mode(&result, &mode);
	zassert_equal(mode, 0, "Should stay in mode.idle");
	zassert_equal(result.requested_action_count, 0);
}

ZTEST(ARBITER_eval, test_determinism)
{
	struct ARBITER_ctx ctx;
	struct ARBITER_snapshot snap;
	struct ARBITER_result r1, r2;

	ARBITER_init(&ctx, &test_model);
	ARBITER_set_bool(&ctx, 1, true);

	ARBITER_snapshot_begin(&ctx, &snap);
	ARBITER_eval(&test_model, &snap, &r1, NULL);
	ARBITER_eval(&test_model, &snap, &r2, NULL);

	zassert_equal(r1.current_mode, r2.current_mode);
	zassert_equal(r1.raised_faults, r2.raised_faults);
	zassert_equal(r1.requested_action_count, r2.requested_action_count);
	zassert_equal(r1.eval_op_count, r2.eval_op_count);
}

ZTEST(ARBITER_eval, test_trace)
{
	struct ARBITER_ctx ctx;
	struct ARBITER_snapshot snap;
	struct ARBITER_result result;
	struct ARBITER_trace_entry trace_buf[8];
	struct ARBITER_trace trace;

	ARBITER_trace_init(&trace, trace_buf, 8);
	ARBITER_init(&ctx, &test_model);
	ARBITER_set_bool(&ctx, 1, true);
	ARBITER_snapshot_begin(&ctx, &snap);
	ARBITER_eval(&test_model, &snap, &result, &trace);

	zassert_equal(trace.count, 1, "Should have 1 trace entry");
	zassert_false(trace.overflow);

	const struct ARBITER_trace_entry *e = ARBITER_trace_get(&trace, 0);

	zassert_not_null(e);
	zassert_true(e->condition_result, "Rule should have fired");
}
