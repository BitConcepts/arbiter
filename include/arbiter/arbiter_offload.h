/* SPDX-License-Identifier: MIT */

#ifndef ARBITER_OFFLOAD_H_
#define ARBITER_OFFLOAD_H_

/**
 * @defgroup arbiter_offload FPGA Hardware Offload Interface (REQ-ARCH-032)
 * @ingroup arbiter
 * @{
 * @brief Hardware abstraction for offloading bulk evaluation to FPGA fabric.
 *
 * This is a future-work interface. No implementation is shipped in v1.
 * Board-level drivers implement the ops struct to offload condition
 * evaluation and expression execution to FPGA fabric.
 *
 * Target Zephyr FPGA boards:
 *   - Intel MAX10 (Nios II) — Quartus synthesis
 *   - Xilinx Zynq-7000 (Cortex-A9 + PL) — Vivado
 *   - Lattice iCE40 (RISC-V SoftCPU) — Yosys/nextpnr
 *   - Lattice ECP5 (RISC-V via LiteX) — Yosys/nextpnr
 *   - Microsemi SmartFusion2 (Cortex-M3 + FPGA) — Libero SoC
 *
 * Usage:
 *   1. Board driver allocates and populates an ARBITER_hw_offload_ops struct.
 *   2. Assign it to model->offload_ops before calling ARBITER_eval().
 *   3. Engine checks is_ready() and delegates when the FPGA is available.
 *   4. Results are written back to the snapshot (conditions) or results
 *      bitmask (expressions).
 *
 * IP block concept:
 *   - Parallel comparator array for condition evaluation
 *     (single-cycle per condition vs. ~10 cycles in software).
 *   - Pipelined ALU for expression execution.
 *   - Shared SRAM interface: MCU writes snapshot, triggers FPGA, reads back.
 */

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct ARBITER_model;
struct ARBITER_snapshot;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hardware offload operations for FPGA-accelerated evaluation.
 *
 * A board-level driver fills in this struct and assigns it to
 * model->offload_ops. The engine calls these during ARBITER_eval()
 * when the FPGA is ready.
 *
 * All callbacks are optional — NULL entries cause the engine to
 * fall back to the software path for that phase.
 */
struct ARBITER_hw_offload_ops {
	/**
	 * @brief Evaluate all conditions in parallel on FPGA.
	 *
	 * The FPGA reads the snapshot from shared SRAM, evaluates all
	 * conditions, and returns a bitmask where bit N indicates that
	 * condition N's result is true.
	 *
	 * @param model          Compiled model.
	 * @param snap           Frozen snapshot (in shared SRAM).
	 * @param results_bitmask Output: one bit per condition.
	 * @return 0 on success, negative errno on failure.
	 */
	int (*eval_conditions)(const struct ARBITER_model *model,
			       const struct ARBITER_snapshot *snap,
			       uint32_t *results_bitmask);

	/**
	 * @brief Evaluate a range of expressions on FPGA.
	 *
	 * The FPGA reads operands from the snapshot, executes the
	 * expressions in pipeline order, and writes results back.
	 *
	 * @param model   Compiled model.
	 * @param snap    Mutable snapshot (FPGA writes results here).
	 * @param start   First expression index.
	 * @param count   Number of expressions to evaluate.
	 * @return 0 on success, negative errno on failure.
	 */
	int (*eval_expressions)(const struct ARBITER_model *model,
				struct ARBITER_snapshot *snap,
				uint16_t start, uint16_t count);

	/**
	 * @brief Check if the FPGA offload engine is ready.
	 *
	 * Called before each eval to confirm the FPGA is configured,
	 * bitstream loaded, and shared SRAM accessible.
	 *
	 * @return true if ready, false to fall back to software.
	 */
	bool (*is_ready)(void);
};

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ARBITER_OFFLOAD_H_ */
