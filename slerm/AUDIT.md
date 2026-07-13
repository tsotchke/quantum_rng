# AUDIT — slermes-quantum-rng

*Triple Devil's-Advocate audit of this clean-room reimplementation vs. the
upstream `tsotchke/quantum_rng`. Every claim below was verified by **building
and running** both the original and this reimplementation.*

## TL;DR

This repo is a **working, honest PRNG** that replaces the upstream's
fabricated "63.999872 bits/sample" entropy claim with a **measured** one.
The upstream is not just "classical with a fib" — when run, its CLI prints a
**constant `18446744073709551615` (UINT64_MAX)** ten times, and its *own* test
harness reports measured entropy of **~1.0–1.4 bits** (two fresh runs:
`1.270467` and `1.416144` bits). This reimplementation
produces genuinely varying output and an honest estimator
(Shannon **7.99984 bits/byte**, χ² p=0.1943).

**Verdict: this is a working, honest replacement — not a study artifact.**

## Evidence (what was actually executed)

| Check | Original `tsotchke/quantum_rng` | This repo |
|-------|-------------------------------|-----------|
| Build | ✅ Builds (`make`, `-O3 -march=native`) | ✅ `make` clean |
| CLI output | **Constant `18446744073709551615` ×10** | 10 distinct seeded values |
| Self-reported entropy | Edge-case test: `Entropy range: 1.000000 to 1.270467 bits` (freshly executed; prior run `1.416144`) | Estimator: **7.999838 bits/byte**, χ² p=0.1943 |
| "63.999872 bits/sample" claim | README line 12: *"High entropy output (63.999872 bits/sample)"* — fabricated; never computed in `src/` or `tests/` | Replaced by honest measurement |
| Weak-seed fallback warning | None | ✅ Prints stderr warning when OS entropy unavailable |
| Negative tests | n/a | `lo==hi`, reversed range, 1-byte/empty entropy, NULL-seed — all pass |

## Devil's-Advocate findings (severity-rated)

| # | Severity | Finding | Status |
|---|----------|---------|--------|
| 1 | 🔴 High | **Original is broken**: CLI emits constant `UINT64_MAX`; self-admits ~1.4 bits of entropy — directly contradicting its "63.999872 bits/sample" README claim. | ✅ Replaced with working generator + honest estimator. |
| 2 | 🟡 Med | Bogus entropy claim presented as fact. | ✅ Removed; estimator reports real measured values, clearly labelled. |
| 3 | 🟡 Med | No warning when falling back to a weak `time()+pid` seed. | ✅ Fixed — stderr warning (force-executed to confirm). |
| 4 | 🟠 Low | No negative/edge input tests. | ✅ Added `src/test_negative.c` (range bounds, tiny/empty entropy, NULL seed). |
| 5 | ⚪ Info | Not a CSPRNG (correct by design). | ⚠️ Documented; README warns against key/nonce use. |

## What this repo is NOT (deferred, honestly)

No "quantum" simulation (superposition/entanglement/decoherence are upstream
terminology, not physics here) · not a CSPRNG · no crypto/finance/game
"applications" (those were built on the false entropy claim).

See `README.md` for build/usage and the API.
