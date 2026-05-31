# SPDX-License-Identifier: MIT
"""Golden vector tests — framework for verifying deterministic evaluation.

Each model in tests/vectors/ should include:
  - input_snapshot.json
  - expected_result.json
  - expected_trace.json

The same vectors are tested by:
  - Python reference evaluator (this file)
  - Generated C runtime under Zephyr
  - Blob runtime under Zephyr
"""

import json
from pathlib import Path

import pytest

VECTORS_DIR = Path(__file__).resolve().parent.parent / "vectors"


def _load_vectors():
    """Discover and load golden vector test cases."""
    if not VECTORS_DIR.exists():
        return []
    vectors = []
    for d in sorted(VECTORS_DIR.iterdir()):
        if d.is_dir() and (d / "input_snapshot.json").exists():
            vectors.append(d.name)
    return vectors


@pytest.mark.parametrize("vector_name", _load_vectors() or ["placeholder"])
def test_golden_vector(vector_name):
    """Verify golden vector produces expected result."""
    if vector_name == "placeholder":
        pytest.skip("No golden vectors yet — add to tests/vectors/")
    vector_dir = VECTORS_DIR / vector_name
    input_snap = json.loads((vector_dir / "input_snapshot.json").read_text())
    expected = json.loads((vector_dir / "expected_result.json").read_text())
    # TODO: implement Python reference evaluator and compare
    pytest.skip("Python reference evaluator not yet implemented")
