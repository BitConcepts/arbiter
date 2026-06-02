# Hardware PID Benchmark

Cycle-accurate PID benchmark targeting real Cortex-M hardware.
Uses the ARM DWT CYCCNT register for cycle-exact measurement on
Cortex-M targets, and falls back to `clock_gettime` on `native_sim`.

## Supported Boards

- **nucleo_f446re** — STM32F446RE (Cortex-M4, 180 MHz)
- **nucleo_h743zi** — STM32H743ZI (Cortex-M7, 480 MHz)
- **native_sim** — Host execution (nanosecond timing)

## Building

```sh
# For real hardware
west build -b nucleo_f446re tests/benchmarks/hw_benchmark
west build -b nucleo_h743zi tests/benchmarks/hw_benchmark

# For simulation
west build -b native_sim tests/benchmarks/hw_benchmark
```

## Flashing

Connect the Nucleo board via USB (ST-Link) and run:

```sh
west flash
```

Ensure your udev rules are configured for ST-Link. On Linux:

```sh
# /etc/udev/rules.d/99-stlink.rules
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="374b", MODE="0666"
```

## Capturing Results

Open a serial terminal at 115200 baud on the board's virtual COM port:

```sh
# Linux — typical device path for Nucleo boards
picocom -b 115200 /dev/ttyACM0

# Or use west
west espressif monitor  # (for ESP boards)
# For STM32 Nucleo, use any serial terminal on the ST-Link VCP
```

Press the board's reset button to re-run the benchmark.

### Expected Output

```
[00:00:00.000,000] <inf> hw_bench: === HW PID Benchmark ===
[00:00:00.000,000] <inf> hw_bench: Timer unit: cycles
[00:00:00.000,000] <inf> hw_bench: Iterations: 10000  (warmup: 500)
[00:00:00.xxx,xxx] <inf> hw_bench: --- Hand-coded PID ---
[00:00:00.xxx,xxx] <inf> hw_bench:   Total: NNNN cycles  (NN cycles/tick)
[00:00:00.xxx,xxx] <inf> hw_bench:   RAM (struct): 24 bytes
[00:00:00.xxx,xxx] <inf> hw_bench: --- arbiter Engine PID ---
[00:00:00.xxx,xxx] <inf> hw_bench:   Total: NNNN cycles  (NN cycles/tick)
[00:00:00.xxx,xxx] <inf> hw_bench:   RAM (ctx): NNN bytes
[00:00:00.xxx,xxx] <inf> hw_bench: === Comparison ===
[00:00:00.xxx,xxx] <inf> hw_bench:   Engine overhead: NN% (NN vs NN cycles/tick)
[00:00:00.xxx,xxx] <inf> hw_bench: HW Benchmark complete
```

## Running via Twister

```sh
# Build-only test (no hardware required)
west twister -T tests/benchmarks/hw_benchmark -p native_sim

# With real hardware (requires connected board and runner configured)
west twister -T tests/benchmarks/hw_benchmark -p nucleo_f446re --device-testing
```

## How It Works

### DWT CYCCNT (Cortex-M)

The Data Watchpoint and Trace (DWT) unit provides a 32-bit free-running
cycle counter (`CYCCNT`). At typical Cortex-M clock speeds:

- **180 MHz (F446RE)**: wraps every ~23.8 seconds
- **480 MHz (H743ZI)**: wraps every ~8.9 seconds

The benchmark completes well within these limits.

### Measurement Methodology

1. **Warmup**: 500 iterations to stabilize caches and branch predictors.
2. **Measured window**: 10,000 iterations of the PID loop.
3. **Both implementations** (hand-coded and arbiter engine) use identical
   inputs and produce identical control outputs.
4. The cycle count delta is divided by the iteration count for per-tick cost.

### ROM Comparison

ROM cannot be measured at runtime. After building, compare `.elf` sizes:

```sh
arm-none-eabi-size build/zephyr/zephyr.elf
```
