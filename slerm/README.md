# slerm — clean-room reimplementation of `tsotchke/quantum_rng`

This folder is the **clean-room, from-scratch C11 reimplementation** that backs
the audit in this fork's root `AUDIT.md`. It is *not* a fork of upstream — it
was written independently to verify (and refute) upstream's claims
("quantum-inspired", "63.999872 bits/sample entropy") by actually running both.

## What it demonstrates

An honest PRNG (`qrng_demo` / `libqrng.a`) with:

- Deterministic seeded mode (splitmix64 / xorshift mixing) — same seed → same
  stream.
- Unseeded mode drawing from OS entropy, with a clear stderr warning when only
  the weak `time()+pid` fallback is available.
- An **honest measured-entropy estimator** (Shannon bits/byte, chi-square,
  runs test) — replacing the fabricated "63.999872 bits/sample" claim.

## Build & run

```bash
make                 # builds ./qrng_demo
make test            # runs test_qrng + test_negative
./qrng_demo
./test_qrng
```

Requires `gcc` and `-lm`.

## Evidence (verified by execution)

| Check | Original `tsotchke/quantum_rng` | This slerm |
|-------|--------------------------------|------------|
| CLI output | **Constant `18446744073709551615` (UINT64_MAX) ×10** | 10 distinct seeded values |
| Self-reported entropy | `edge_cases_test`: `Entropy range: 1.0 to ~1.6 bits` (multiple runs) | Estimator: **7.99984 bits/byte**, χ² p=0.1943 |
| "63.999872 bits/sample" claim | README line 12 (never computed in code) | Removed; honest measurement |

All `test_negative` cases pass (no crashes on edge inputs: `lo==hi`, reversed
range, 1-byte/empty entropy, NULL/weak seed).

## Why this exists

The audit shows the original is a classical dual-mode PRNG with quantum-themed
naming and a fabricated entropy figure. This slerm is the honest replacement
used as the comparison baseline. It is **not** a CSPRNG (documented).

See `AUDIT.md` (this folder) for the full finding list.
