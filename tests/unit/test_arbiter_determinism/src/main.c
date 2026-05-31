/* SPDX-License-Identifier: MIT */

/**
 * Determinism Proof Test
 *
 * This test provides empirical proof that arbiter_eval() is deterministic:
 * given the same compiled model and the same fact snapshot, the engine
 * always produces the same result.
 *
 * Three proof strategies:
 *   1. Repeat-eval: run eval N times with identical inputs, assert
 *      byte-identical results every time.
 *   2. Order-independence: set facts in different orders, verify same result.
 *   3. Cross-seed: use multiple different input scenarios, verify each
 *      is internally consistent across repetitions.
 *
 * This is Level 1 (empirical) evidence per the certification roadmap.
 * Level 2 (CBMC bounded model checking) and Level 3 (Lean 4 formal proof)
 * are documented in safety/certification_roadmap.md.
 */

#include <zephyr/ztest.h>
#include <arbiter/arbiter.h>
#include <string.h>

/* Use a minimal inline model for self-contained testing */
static const struct arbiter_fact_def test_facts[] = {
	{ .id = 0, .type = ARBITER_FACT_INT32, .range_min = -1000,
	  .range_max = 1000, .default_value = 0, .stale_after_ms = 0,
	  .safety_relevant = true, .name = "input" },
	{ .id = 1, .type = ARBITER_FACT_INT32, .range_min = -10000,
	  .range_max = 10000, .default_value = 0, .stale_after_ms = 0,
	  .safety_relevant = true, .name = "output" },
	{ .id = 2, .type = ARBITER_FACT_BOOL, .range_min = 0,
	  .range_max = 1, .default_value = 1, .stale_after_ms = 0,
	  .safety_relevant = false, .name = "enable" },
};

static const struct arbiter_condition_def test_conditions[] = {
	{ .fact_id = 2, .op = ARBITER_OP_EQ, .value = 1,
	  .group = ARBITER_COND_ALL, .group_index = 0, .next = UINT16_MAX },
};

static const struct arbiter_expr_def test_expressions[] = {
	/* output = input * 3 */
	{ .target_fact_id = 1, .op = ARBITER_EXPR_SCALE,
	  .left_fact_id = 0, .left_literal = 0,
	  .right_fact_id = UINT16_MAX, .right_literal = 3,
	  .scale = 1 },
};

static const struct arbiter_action_def test_actions[] = {
	{ .id = 0, .type = ARBITER_ACTION_CALLBACK, .target_fact_id = 0,
	  .target_value = 0, .callback = NULL, .must_complete_within_ms = 0,
	  .safe_state_action = false, .name = "noop" },
};

static const struct arbiter_rule_def test_rules[] = {
	{ .id = 0, .rule_class = ARBITER_RULE_INFERENCE,
	  .condition_start = 0, .condition_count = 1,
	  .action_start = 0, .action_count = 0,
	  .expr_start = 0, .expr_count = 1,
	  .safety_goal_id = UINT16_MAX, .set_mode = UINT16_MAX,
	  .safety_critical = false,
	  .name = "compute_output", .explanation = "output = input * 3" },
};

static const char *test_mode_names[] = { "normal" };

static const struct arbiter_model test_model = {
	.name = "determinism_test",
	.model_hash = {0},
	.schema_hash = {0},
	.fact_count = 3,
	.rule_count = 1,
	.condition_count = 1,
	.action_count = 1,
	.expr_count = 1,
	.mode_count = 1,
	.facts = test_facts,
	.rules = test_rules,
	.conditions = test_conditions,
	.actions = test_actions,
	.expressions = test_expressions,
	.mode_names = test_mode_names,
};

/* ------------------------------------------------------------------ */
/* Proof 1: Repeat-eval determinism                                   */
/* ------------------------------------------------------------------ */

#define REPEAT_COUNT 10000

ZTEST(determinism, test_repeat_eval_identical)
{
	struct arbiter_ctx ctx;
	struct arbiter_snapshot snap_ref, snap_cur;
	struct arbiter_result result_ref, result_cur;

	arbiter_init(&ctx, &test_model);
	arbiter_set_bool(&ctx, 2, true);
	arbiter_set_i32(&ctx, 0, 42);

	/* Reference run */
	arbiter_snapshot_begin(&ctx, &snap_ref);
	arbiter_eval(&test_model, &snap_ref, &result_ref, NULL);

	/* Repeat N times and compare */
	for (int i = 0; i < REPEAT_COUNT; i++) {
		arbiter_snapshot_begin(&ctx, &snap_cur);
		arbiter_eval(&test_model, &snap_cur, &result_cur, NULL);

		/* Byte-identical snapshot values */
		zassert_mem_equal(
			snap_ref.values, snap_cur.values,
			sizeof(snap_ref.values[0]) * test_model.fact_count,
			"Snapshot diverged on iteration %d", i);

		/* Identical result */
		zassert_equal(result_ref.mode, result_cur.mode,
			      "Mode diverged on iteration %d", i);
		zassert_equal(result_ref.fired_count, result_cur.fired_count,
			      "Fired count diverged on iteration %d", i);
	}
}

/* ------------------------------------------------------------------ */
/* Proof 2: Fact write order independence                             */
/* ------------------------------------------------------------------ */

ZTEST(determinism, test_fact_order_independence)
{
	struct arbiter_ctx ctx_a, ctx_b;
	struct arbiter_snapshot snap_a, snap_b;
	struct arbiter_result res_a, res_b;

	/* Context A: set input first, then enable */
	arbiter_init(&ctx_a, &test_model);
	arbiter_set_i32(&ctx_a, 0, 100);
	arbiter_set_bool(&ctx_a, 2, true);
	arbiter_snapshot_begin(&ctx_a, &snap_a);
	arbiter_eval(&test_model, &snap_a, &res_a, NULL);

	/* Context B: set enable first, then input */
	arbiter_init(&ctx_b, &test_model);
	arbiter_set_bool(&ctx_b, 2, true);
	arbiter_set_i32(&ctx_b, 0, 100);
	arbiter_snapshot_begin(&ctx_b, &snap_b);
	arbiter_eval(&test_model, &snap_b, &res_b, NULL);

	/* Must be identical */
	zassert_equal(ctx_a.fact_values[1].value, ctx_b.fact_values[1].value,
		      "Output differs: order A=%d, order B=%d",
		      ctx_a.fact_values[1].value, ctx_b.fact_values[1].value);
	zassert_equal(res_a.mode, res_b.mode, "Mode differs by write order");
}

/* ------------------------------------------------------------------ */
/* Proof 3: Cross-seed consistency                                    */
/* ------------------------------------------------------------------ */

ZTEST(determinism, test_cross_seed_consistency)
{
	struct arbiter_ctx ctx;
	struct arbiter_snapshot snap;
	struct arbiter_result result;

	/* Test 100 different input values, each repeated 100 times */
	for (int32_t input = -50; input < 50; input++) {
		int32_t reference_output = 0;

		for (int rep = 0; rep < 100; rep++) {
			arbiter_init(&ctx, &test_model);
			arbiter_set_bool(&ctx, 2, true);
			arbiter_set_i32(&ctx, 0, input);

			arbiter_snapshot_begin(&ctx, &snap);
			arbiter_eval(&test_model, &snap, &result, NULL);

			int32_t output = ctx.fact_values[1].value;

			if (rep == 0) {
				reference_output = output;
				/* Verify the math is correct */
				zassert_equal(output, input * 3,
					      "Wrong result: %d * 3 != %d",
					      input, output);
			} else {
				zassert_equal(output, reference_output,
					      "Cross-seed: input=%d rep=%d "
					      "got %d expected %d",
					      input, rep, output,
					      reference_output);
			}
		}
	}
}

/* ------------------------------------------------------------------ */
/* Proof 4: Re-init determinism (fresh context every time)            */
/* ------------------------------------------------------------------ */

ZTEST(determinism, test_reinit_determinism)
{
	int32_t results[100];

	for (int i = 0; i < 100; i++) {
		struct arbiter_ctx ctx;
		struct arbiter_snapshot snap;
		struct arbiter_result result;

		arbiter_init(&ctx, &test_model);
		arbiter_set_bool(&ctx, 2, true);
		arbiter_set_i32(&ctx, 0, 777);

		arbiter_snapshot_begin(&ctx, &snap);
		arbiter_eval(&test_model, &snap, &result, NULL);

		results[i] = ctx.fact_values[1].value;
	}

	/* All 100 must be identical */
	for (int i = 1; i < 100; i++) {
		zassert_equal(results[0], results[i],
			      "Re-init diverged: run 0=%d, run %d=%d",
			      results[0], i, results[i]);
	}
}

ZTEST_SUITE(determinism, NULL, NULL, NULL, NULL, NULL);
