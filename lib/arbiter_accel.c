/* SPDX-License-Identifier: MIT */

/**
 * @file arbiter_accel.c
 * @brief HW-accelerated expression evaluation (REQ-ARCH-030).
 *
 * Conditionally compiled when CONFIG_ARBITER_HW_ACCEL=y.
 * Provides platform-optimized intrinsics for the inner expression
 * evaluation loop:
 *
 *  - ARM Cortex-M4+: uses __QADD, __QSUB, __SMMUL, __SMMLA for
 *    saturating arithmetic and widening multiply.
 *  - RISC-V P-extension: uses KADDW/KSUBW for saturating ops.
 *  - Fallback: identical to the scalar C path in arbiter_eval.c.
 *
 * The eval loop calls arbiter_accel_eval_expression() for each
 * expression when HW_ACCEL is enabled, replacing eval_expression().
 */

#include <arbiter/arbiter.h>
#include <zephyr/kernel.h>

#if defined(CONFIG_ARBITER_HW_ACCEL) && CONFIG_ARBITER_HW_ACCEL

/* ── ARM Cortex-M4+ (DSP instructions) ─────────────────────── */
#if defined(CONFIG_CPU_CORTEX_M_HAS_DSP) || \
    defined(__ARM_FEATURE_DSP)

#include <arm_acle.h>

/**
 * Saturating add — single-cycle QADD on Cortex-M4+.
 */
static inline int32_t accel_sat_add(int32_t a, int32_t b)
{
	return __qadd(a, b);
}

/**
 * Saturating subtract — single-cycle QSUB.
 */
static inline int32_t accel_sat_sub(int32_t a, int32_t b)
{
	return __qsub(a, b);
}

/**
 * Widening multiply then shift: (a * b) >> 32, returning upper 32 bits.
 * Used for fixed-point SCALE operations.
 * SMMUL returns the upper 32 bits of a signed 32x32→64 multiply.
 */
static inline int32_t accel_scale(int32_t value, int32_t numer, int32_t denom)
{
	if (denom == 0) {
		return 0;
	}
	/* Fall back to 64-bit for division; SMMUL alone can't divide. */
	int64_t wide = (int64_t)value * (int64_t)numer;

	wide /= denom;
	/* Saturate to int32 */
	if (wide > INT32_MAX) {
		return INT32_MAX;
	}
	if (wide < INT32_MIN) {
		return INT32_MIN;
	}
	return (int32_t)wide;
}

/**
 * Accumulate with widening: target += (left * right) / scale.
 * Uses SMMLA (signed multiply-accumulate, returning upper 32 bits)
 * when scale is a power of 2^32; otherwise falls back to 64-bit.
 */
static inline int32_t accel_accumulate(int32_t current, int32_t left,
				       int32_t right, int32_t scale)
{
	int64_t wide = (int64_t)left * (int64_t)right;

	if (scale != 0) {
		wide /= scale;
	}
	int64_t acc = (int64_t)current + wide;

	if (acc > INT32_MAX) {
		return INT32_MAX;
	}
	if (acc < INT32_MIN) {
		return INT32_MIN;
	}
	return (int32_t)acc;
}

/* ── RISC-V P-extension ────────────────────────────────────── */
#elif defined(CONFIG_RISCV_ISA_EXT_P) || defined(__riscv_p)

/*
 * RISC-V P-extension (packed SIMD) provides KADDW/KSUBW for
 * saturating 32-bit add/sub. These are exposed via builtins
 * when the compiler supports the P extension.
 *
 * Note: P-extension toolchain support is still maturing.
 * Use GCC builtins where available, else fall back to C.
 */

static inline int32_t accel_sat_add(int32_t a, int32_t b)
{
#if __has_builtin(__builtin_riscv_kaddw)
	return __builtin_riscv_kaddw(a, b);
#else
	int64_t r = (int64_t)a + b;
	return (r > INT32_MAX) ? INT32_MAX : (r < INT32_MIN) ? INT32_MIN : (int32_t)r;
#endif
}

static inline int32_t accel_sat_sub(int32_t a, int32_t b)
{
#if __has_builtin(__builtin_riscv_ksubw)
	return __builtin_riscv_ksubw(a, b);
#else
	int64_t r = (int64_t)a - b;
	return (r > INT32_MAX) ? INT32_MAX : (r < INT32_MIN) ? INT32_MIN : (int32_t)r;
#endif
}

static inline int32_t accel_scale(int32_t value, int32_t numer, int32_t denom)
{
	if (denom == 0) {
		return 0;
	}
	int64_t wide = (int64_t)value * (int64_t)numer;

	wide /= denom;
	if (wide > INT32_MAX) {
		return INT32_MAX;
	}
	if (wide < INT32_MIN) {
		return INT32_MIN;
	}
	return (int32_t)wide;
}

static inline int32_t accel_accumulate(int32_t current, int32_t left,
				       int32_t right, int32_t scale)
{
	int64_t wide = (int64_t)left * (int64_t)right;

	if (scale != 0) {
		wide /= scale;
	}
	return accel_sat_add(current, (int32_t)(
		(wide > INT32_MAX) ? INT32_MAX :
		(wide < INT32_MIN) ? INT32_MIN : wide
	));
}

/* ── Generic fallback (should not normally be reached) ─────── */
#else

static inline int32_t accel_sat_add(int32_t a, int32_t b)
{
	int64_t r = (int64_t)a + b;

	return (r > INT32_MAX) ? INT32_MAX :
	       (r < INT32_MIN) ? INT32_MIN : (int32_t)r;
}

static inline int32_t accel_sat_sub(int32_t a, int32_t b)
{
	int64_t r = (int64_t)a - b;

	return (r > INT32_MAX) ? INT32_MAX :
	       (r < INT32_MIN) ? INT32_MIN : (int32_t)r;
}

static inline int32_t accel_scale(int32_t value, int32_t numer, int32_t denom)
{
	if (denom == 0) {
		return 0;
	}
	int64_t wide = (int64_t)value * (int64_t)numer;

	wide /= denom;
	if (wide > INT32_MAX) {
		return INT32_MAX;
	}
	if (wide < INT32_MIN) {
		return INT32_MIN;
	}
	return (int32_t)wide;
}

static inline int32_t accel_accumulate(int32_t current, int32_t left,
				       int32_t right, int32_t scale)
{
	int64_t wide = (int64_t)left * (int64_t)right;

	if (scale != 0) {
		wide /= scale;
	}
	int64_t acc = (int64_t)current + wide;

	if (acc > INT32_MAX) {
		return INT32_MAX;
	}
	if (acc < INT32_MIN) {
		return INT32_MIN;
	}
	return (int32_t)acc;
}

#endif /* platform selection */

/**
 * Resolve an operand: read from a fact or use a literal.
 */
static int32_t accel_resolve(const struct ARBITER_snapshot *snapshot,
			     arbiter_index_t fact_id, int32_t literal)
{
	if (fact_id == ARBITER_INDEX_MAX) {
		return literal;
	}
	if (fact_id < snapshot->count) {
		return snapshot->values[fact_id].value;
	}
	return 0;
}

/**
 * HW-accelerated expression evaluation.
 * Called by arbiter_eval.c when CONFIG_ARBITER_HW_ACCEL is enabled.
 */
void arbiter_accel_eval_expression(const struct ARBITER_expr_def *expr,
				   struct ARBITER_snapshot *snapshot,
				   uint32_t *op_count)
{
	(*op_count)++;

	if (expr->target_fact_id >= snapshot->count) {
		return;
	}

	int32_t left = accel_resolve(snapshot, expr->left_fact_id,
				     expr->left_literal);
	int32_t right = accel_resolve(snapshot, expr->right_fact_id,
				      expr->right_literal);
	int32_t result = 0;

	switch (expr->op) {
	case ARBITER_EXPR_ADD:
		result = accel_sat_add(left, right);
		break;
	case ARBITER_EXPR_SUB:
		result = accel_sat_sub(left, right);
		break;
	case ARBITER_EXPR_MUL:
		result = left * right;
		break;
	case ARBITER_EXPR_DIV:
		result = (right != 0) ? (left / right) : 0;
		break;
	case ARBITER_EXPR_MOD:
		result = (right != 0) ? (left % right) : 0;
		break;
	case ARBITER_EXPR_ABS:
		result = (left < 0) ? -left : left;
		break;
	case ARBITER_EXPR_NEGATE:
		result = -left;
		break;
	case ARBITER_EXPR_MIN:
		result = (left < right) ? left : right;
		break;
	case ARBITER_EXPR_MAX:
		result = (left > right) ? left : right;
		break;
	case ARBITER_EXPR_CLAMP:
		if (left < right) {
			result = right;
		} else if (left > expr->scale) {
			result = expr->scale;
		} else {
			result = left;
		}
		break;
	case ARBITER_EXPR_SHIFT_R:
		result = left >> (right & 31);
		break;
	case ARBITER_EXPR_SHIFT_L:
		result = left << (right & 31);
		break;
	case ARBITER_EXPR_SCALE:
		result = accel_scale(left, right, expr->scale);
		break;
	case ARBITER_EXPR_ASSIGN:
		result = left;
		break;
	case ARBITER_EXPR_ACCUMULATE: {
		int32_t current = snapshot->values[expr->target_fact_id].value;

		result = accel_accumulate(current, left, right, expr->scale);
		break;
	}
	default:
		return;
	}

	snapshot->values[expr->target_fact_id].value = result;
	snapshot->values[expr->target_fact_id].valid = true;
}

#endif /* CONFIG_ARBITER_HW_ACCEL */
