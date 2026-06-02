# SPDX-License-Identifier: MIT
"""Golden vector tests — framework for verifying deterministic evaluation.

Each subdirectory under tests/vectors/ contains a vector.json with an inline
ARB model, input facts/timestamps, and expected results.

The same vectors are tested by:
  - Python reference evaluator (this file + test_cross_validation.py)
  - Generated C runtime under Zephyr
  - Blob runtime under Zephyr

NOTE: The comprehensive cross-validation tests are in test_cross_validation.py.
This file is kept for backwards compatibility with the original vector
discovery mechanism.
"""

import json
from pathlib import Path

import pytest

from arbiter.evaluator import ArbiterEvaluator

VECTORS_DIR = Path(__file__).resolve().parent.parent / "vectors"


def _load_vectors():
    """Discover golden vector test cases using vector.json format."""
    if not VECTORS_DIR.exists():
        return []
    vectors = []
    for d in sorted(VECTORS_DIR.iterdir()):
        if d.is_dir() and (d / "vector.json").exists():
            vectors.append(d.name)
    return vectors


@pytest.mark.parametrize("vector_name", _load_vectors() or ["placeholder"])
def test_golden_vector(vector_name):
    """Verify golden vector produces expected result via Python evaluator."""
    if vector_name == "placeholder":
        pytest.skip("No golden vectors yet — add to tests/vectors/")
    vector_dir = VECTORS_DIR / vector_name
    vec = json.loads((vector_dir / "vector.json").read_text(encoding="utf-8"))
    model_data = vec["model"]
    expected = vec["expected"]

    ev = ArbiterEvaluator(model_data)
    for fact_name, value in vec.get("facts", {}).items():
        ev.set_fact(fact_name, value)
    for fact_name, ms in vec.get("timestamps", {}).items():
        ev.set_timestamp(fact_name, ms)
    snap_ts = vec.get("snapshot_timestamp_ms", 0)
    if snap_ts:
        ev.set_snapshot_timestamp(snap_ts)

    result = ev.eval()
    assert result.fired_rules == expected["fired_rules"]
