/* SPDX-License-Identifier: MIT */
/* Minimal Zephyr kernel stub for clang-tidy standalone analysis. */

#ifndef ZEPHYR_KERNEL_H_
#define ZEPHYR_KERNEL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

/* Kconfig stubs — clang-tidy needs these defined. */
#ifndef CONFIG_ARBITER_LOG_LEVEL
#define CONFIG_ARBITER_LOG_LEVEL 3
#endif

#ifndef CONFIG_ARBITER_MAX_FACTS
#define CONFIG_ARBITER_MAX_FACTS 64
#endif

#ifndef CONFIG_ARBITER_MAX_ACTIONS_PER_EVAL
#define CONFIG_ARBITER_MAX_ACTIONS_PER_EVAL 16
#endif

#ifndef CONFIG_ARBITER_MAX_TRACE_ENTRIES
#define CONFIG_ARBITER_MAX_TRACE_ENTRIES 64
#endif

#ifndef CONFIG_ARBITER_MAX_TRACE_INPUTS
#define CONFIG_ARBITER_MAX_TRACE_INPUTS 8
#endif

/* Minimal kernel API stubs */
static inline uint32_t k_uptime_get_32(void) { return 0; }

/* BIT macro */
#ifndef BIT
#define BIT(n) (1UL << (n))
#endif

/* IS_ENABLED stub */
#ifndef IS_ENABLED
#define IS_ENABLED(config) 0
#endif

/* Branch prediction hints (Zephyr provides these via toolchain.h) */
#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#endif /* ZEPHYR_KERNEL_H_ */
