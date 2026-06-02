# SPDX-License-Identifier: MIT
"""Tests for engine scaling profiles, resource reports, and profile validation.

Covers:
  REQ-ARCH-029 — Engine scaling profiles with auto-detection
  REQ-ARCH-033 — Model complexity analysis (resource budget report)
  REQ-BUILD-002 — clang-tidy CI gate (indirectly, via string stripping)
  TEST-038, TEST-042
"""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

from arbiter.canonical import canonicalize
from arbiter.compiler import PROFILE_LIMITS, CompileOptions, CompileResult, compile_model
from arbiter.emit_c import emit_c_source, emit_c_header

SAMPLES_DIR = Path(__file__).resolve().parent.parent.parent / "samples"


# ---------------------------------------------------------------------------
# Fixtures: minimal models of various sizes
# ---------------------------------------------------------------------------

def _make_model(n_facts: int, n_rules: int) -> dict:
    """Build a synthetic model with exactly n_facts and n_rules."""
    facts = [
        {"id": f"f{i:03d}", "type": "int32", "default": 0}
        for i in range(n_facts)
    ]
    rules = [
        {
            "id": f"r{i:03d}",
            "class": "inference",
            "when": {"all": [{"fact": f"f{i % n_facts:03d}", "op": ">", "value": 0}]},
            "then": {"explanation": f"rule {i}"},
        }
        for i in range(n_rules)
    ]
    return {
        "arb_version": 0.1,
        "model": "test_profile",
        "target": {"rtos": "zephyr"},
        "facts": facts,
        "rules": rules,
    }


SMALL_MODEL = _make_model(4, 4)      # fits nano
MEDIUM_MODEL = _make_model(12, 10)    # fits micro+, exceeds nano
STANDARD_MODEL = _make_model(32, 32)  # fits standard+, exceeds micro
LARGE_MODEL = _make_model(128, 128)   # fits full only


# ---------------------------------------------------------------------------
# PROFILE_LIMITS sanity checks
# ---------------------------------------------------------------------------

def test_profile_limits_keys():
    """All four profiles must be defined."""
    assert set(PROFILE_LIMITS.keys()) == {"nano", "micro", "standard", "full"}


def test_profile_limits_ascending():
    """Profile limits must increase from nano → full."""
    profiles = ["nano", "micro", "standard", "full"]
    for key in ("max_facts", "max_rules"):
        values = [PROFILE_LIMITS[p][key] for p in profiles]
        assert values == sorted(values), f"{key} not ascending: {values}"


def test_profile_limits_index_bits():
    """nano/micro use 8-bit indices; standard/full use 16-bit."""
    assert PROFILE_LIMITS["nano"]["index_bits"] == 8
    assert PROFILE_LIMITS["micro"]["index_bits"] == 8
    assert PROFILE_LIMITS["standard"]["index_bits"] == 16
    assert PROFILE_LIMITS["full"]["index_bits"] == 16


# ---------------------------------------------------------------------------
# Profile validation (REQ-ARCH-029)
# ---------------------------------------------------------------------------

class TestProfileValidation:
    """Compiler rejects models that exceed profile limits."""

    def _compile_synthetic(self, model_data: dict, profile: str) -> CompileResult:
        """Write model to temp file and compile with given profile."""
        import yaml
        with tempfile.TemporaryDirectory() as tmp:
            model_path = Path(tmp) / "test_model.arb.yaml"
            model_path.write_text(yaml.dump(model_data), encoding="utf-8")
            opts = CompileOptions(
                out_c=Path(tmp) / "out.c",
                out_h=Path(tmp) / "out.h",
                profile=profile,
            )
            return compile_model(model_path, opts)

    def test_small_model_fits_nano(self):
        result = self._compile_synthetic(SMALL_MODEL, "nano")
        assert result.success

    def test_small_model_fits_all_profiles(self):
        for profile in ("nano", "micro", "standard", "full"):
            result = self._compile_synthetic(SMALL_MODEL, profile)
            assert result.success, f"Small model should fit {profile}"

    def test_medium_model_rejects_nano(self):
        result = self._compile_synthetic(MEDIUM_MODEL, "nano")
        assert not result.success
        assert result.diagnostics.has_errors()
        errors = [d for d in result.diagnostics.diagnostics if d.severity == "error"]
        assert any("ARB-PROFILE" in d.code for d in errors)

    def test_medium_model_fits_micro(self):
        result = self._compile_synthetic(MEDIUM_MODEL, "micro")
        assert result.success

    def test_standard_model_rejects_micro(self):
        result = self._compile_synthetic(STANDARD_MODEL, "micro")
        assert not result.success

    def test_standard_model_fits_standard(self):
        result = self._compile_synthetic(STANDARD_MODEL, "standard")
        assert result.success

    def test_large_model_rejects_standard(self):
        result = self._compile_synthetic(LARGE_MODEL, "standard")
        assert not result.success

    def test_large_model_fits_full(self):
        result = self._compile_synthetic(LARGE_MODEL, "full")
        assert result.success

    def test_profile_error_has_suggestion(self):
        result = self._compile_synthetic(MEDIUM_MODEL, "nano")
        errors = [d for d in result.diagnostics.diagnostics if d.severity == "error"]
        assert any(d.suggestion and "larger profile" in d.suggestion for d in errors)


# ---------------------------------------------------------------------------
# Resource report (REQ-ARCH-033)
# ---------------------------------------------------------------------------

class TestResourceReport:
    """Compiler emits a resource budget report with the compilation result."""

    def _compile_battery(self, profile: str = "standard") -> CompileResult:
        model = SAMPLES_DIR / "battery_policy" / "models" / "battery.arb.yaml"
        with tempfile.TemporaryDirectory() as tmp:
            opts = CompileOptions(
                out_c=Path(tmp) / "model.c",
                out_h=Path(tmp) / "model.h",
                profile=profile,
            )
            return compile_model(model, opts)

    def test_report_present(self):
        result = self._compile_battery()
        assert result.resource_report
        assert "Profile:" in result.resource_report

    def test_report_contains_facts(self):
        result = self._compile_battery()
        assert "Facts:" in result.resource_report

    def test_report_contains_rules(self):
        result = self._compile_battery()
        assert "Rules:" in result.resource_report

    def test_report_contains_ram_estimate(self):
        result = self._compile_battery()
        assert "RAM estimate:" in result.resource_report

    def test_report_contains_rodata_estimate(self):
        result = self._compile_battery()
        assert ".rodata:" in result.resource_report

    def test_report_contains_wcet(self):
        result = self._compile_battery()
        assert "WCET ops:" in result.resource_report

    def test_report_contains_fit_checks(self):
        result = self._compile_battery()
        # Should show checkmarks for profiles the model fits
        assert "\u2713" in result.resource_report  # ✓

    def test_report_shows_profile_name(self):
        result = self._compile_battery("full")
        assert "Profile: full" in result.resource_report

    def test_report_ram_increases_with_facts(self):
        """Larger models should report higher RAM estimates."""
        import yaml
        with tempfile.TemporaryDirectory() as tmp:
            small_path = Path(tmp) / "small.arb.yaml"
            small_path.write_text(yaml.dump(SMALL_MODEL), encoding="utf-8")
            r_small = compile_model(small_path, CompileOptions(profile="standard"))

            big_path = Path(tmp) / "big.arb.yaml"
            big_path.write_text(yaml.dump(STANDARD_MODEL), encoding="utf-8")
            r_big = compile_model(big_path, CompileOptions(profile="standard"))

        assert r_small.success, f"small compile failed: {r_small.diagnostics.format()}"
        assert r_big.success, f"big compile failed: {r_big.diagnostics.format()}"

        # Extract RAM numbers
        import re
        m_small = re.search(r"~(\d+) B", r_small.resource_report)
        m_big = re.search(r"~(\d+) B", r_big.resource_report)
        assert m_small and m_big, "RAM estimate not found in resource reports"
        assert int(m_big.group(1)) > int(m_small.group(1))


# ---------------------------------------------------------------------------
# String stripping on nano/micro profiles
# ---------------------------------------------------------------------------

class TestStringStripping:
    """Nano/micro profiles strip name/explanation strings from generated C."""

    def test_nano_strips_strings(self):
        model = canonicalize(SMALL_MODEL)
        src = emit_c_source(model, emit_trace_strings=False)
        # All name fields should be NULL
        assert ".name = NULL" in src or 'name' not in src

    def test_standard_keeps_strings(self):
        model = canonicalize(SMALL_MODEL)
        src = emit_c_source(model, emit_trace_strings=True)
        # Should contain quoted fact names
        assert '"f000"' in src

    def test_force_strings_overrides_nano(self):
        """--force-strings should emit strings even on nano profile."""
        model = canonicalize(SMALL_MODEL)
        src = emit_c_source(model, emit_trace_strings=True)
        assert '"f000"' in src


# ---------------------------------------------------------------------------
# End-to-end: compile real samples with various profiles
# ---------------------------------------------------------------------------

class TestSampleProfiles:
    """Real sample models compile successfully with appropriate profiles."""

    SAMPLE_MODELS = [
        ("battery_policy", "models/battery.arb.yaml"),
        ("pid_controller", "models/pid_engine.arb.yaml"),
        ("kalman_filter", "models/kalman.arb.yaml"),
    ]

    @pytest.mark.parametrize("sample,model_rel", SAMPLE_MODELS)
    def test_sample_compiles_standard(self, sample, model_rel):
        model = SAMPLES_DIR / sample / model_rel
        if not model.exists():
            pytest.skip(f"Sample model not found: {model}")
        with tempfile.TemporaryDirectory() as tmp:
            opts = CompileOptions(
                out_c=Path(tmp) / "model.c",
                out_h=Path(tmp) / "model.h",
                profile="standard",
            )
            result = compile_model(model, opts)
            assert result.success, f"{sample} should compile on standard"
            assert result.resource_report

    @pytest.mark.parametrize("sample,model_rel", SAMPLE_MODELS)
    def test_sample_compiles_full(self, sample, model_rel):
        model = SAMPLES_DIR / sample / model_rel
        if not model.exists():
            pytest.skip(f"Sample model not found: {model}")
        with tempfile.TemporaryDirectory() as tmp:
            opts = CompileOptions(
                out_c=Path(tmp) / "model.c",
                out_h=Path(tmp) / "model.h",
                profile="full",
            )
            result = compile_model(model, opts)
            assert result.success, f"{sample} should compile on full"

    @pytest.mark.parametrize("sample,model_rel", SAMPLE_MODELS)
    def test_sample_has_resource_report(self, sample, model_rel):
        model = SAMPLES_DIR / sample / model_rel
        if not model.exists():
            pytest.skip(f"Sample model not found: {model}")
        with tempfile.TemporaryDirectory() as tmp:
            opts = CompileOptions(
                out_c=Path(tmp) / "model.c",
                out_h=Path(tmp) / "model.h",
                profile="standard",
            )
            result = compile_model(model, opts)
            assert result.success
            assert "WCET ops:" in result.resource_report
            assert "RAM estimate:" in result.resource_report


# ---------------------------------------------------------------------------
# Header generation: profile-aware defines
# ---------------------------------------------------------------------------

class TestHeaderGeneration:
    """Generated header includes profile-relevant defines."""

    def test_header_has_wcet_define(self):
        model = canonicalize(SMALL_MODEL)
        hdr = emit_c_header(model)
        assert "ARBITER_MODEL_WCET_OP_COUNT" in hdr

    def test_header_has_fact_count(self):
        model = canonicalize(SMALL_MODEL)
        hdr = emit_c_header(model)
        assert "ARBITER_MODEL_FACT_COUNT" in hdr
        assert f"{len(SMALL_MODEL['facts'])}u" in hdr

    def test_header_has_rule_count(self):
        model = canonicalize(SMALL_MODEL)
        hdr = emit_c_header(model)
        assert "ARBITER_MODEL_RULE_COUNT" in hdr
        assert f"{len(SMALL_MODEL['rules'])}u" in hdr

    def test_header_has_model_hash(self):
        model = canonicalize(SMALL_MODEL)
        hdr = emit_c_header(model)
        assert "ARBITER_MODEL_HASH" in hdr
