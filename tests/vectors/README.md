# Golden Test Vectors

Each subdirectory is a test vector containing:

- `model.zrm.yaml` — the ZRM model
- `input_snapshot.json` — fact values to feed
- `expected_result.json` — expected evaluation result
- `expected_trace.json` — expected trace output

Vectors are tested by Python, generated C, and blob runtimes.
