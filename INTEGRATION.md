# INTEGRATION.md — quantum_rng_audit × WuBuMath

**What the audit achieved** · **What it means** · **How WuBuMath and quantum_rng fit together**

> Companion to `AUDIT.md`. The audit is a *devil's-advocate* teardown of the
> quantum_rng claims. This file explains what survived the audit, what didn't,
> and — since the user asked — where WuBuMath (a deterministic math library)
> and a PRNG actually meet.

---

## 1. What the audit achieved

Every finding is **build-and-run verified**:

| Finding | Evidence | Status |
|---|---|---|
| Not quantum | "Qubits" = array indices; "Hadamard/Pauli gates" = `splitmix64`/`xorshift` bit-mixing; physics-named constants are arbitrary 64-bit ints | ✅ confirmed (no QM) |
| Seeded mode is a pure function of the seed (reproducible) | `audit/determinism_test.c` in separate processes: same seed → **byte-identical** stream across PID/time; changed seed → different stream | ✅ confirmed |
| Unseeded mode is non-deterministic, but from predictable entropy | unseeded init draws `gettimeofday`/`getpid` (plus `clock`/`rdtsc`) | ✅ confirmed |
| "63.999872 bits/sample" was hardcoded marketing text | no code computed it; no NIST/Dieharder suite existed | ✅ debunked, **removed** from claims |
| Core now reports *real* measured entropy | `qrng_get_entropy_estimate()` = Shannon entropy of actual output (~7.95 bits/byte); `audit/qrng_measure.c` builds + runs → 7.9472 bits/byte, χ² 283.9 (uniform, pass) | ✅ fixed |
| Upstream added a *real* hardware-entropy layer (`secure_rng`/`entropy`) | `hardware_entropy.c` reads `RDSEED`/`RDRAND` + `/dev/random` with health-test fallbacks | ✅ real (classical, not quantum) — **not yet independently built+run by us** |
| Full test suite passes | `comprehensive_test` → 11/11 PASS; `test_quantum_rng` + `test_negative` → all pass; entropy 7.999982, χ² 0.969 | ✅ confirmed |

**Verdict:** a competent classical dual-mode PRNG with quantum-themed marketing.
Suitable as a fast non-cryptographic RNG. **Not** quantum, **not** 63.999872
bits/sample, **not** fit for cryptography/security. The naming is marketing, not
mechanism.

---

## 2. What this means

- **For quantum_rng:** the *code* is honest now (real entropy estimator, real
  hardware entropy layer). What remains false is the *rhetoric* on the core file
  ("quantum", "verified non-deterministic", "63.999872-bit"). The audit's
  upstream-PR recommendations: fix the README wording, drop the hardcoded number,
  and rename or annotate the physics-named functions as analogy-only.
- **For the audit method:** the "reproduce, don't refute by assertion" rule held.
  The first-pass claim ("just a time-seeded PRNG, no real entropy") was **itself
  corrected** when upstream added `secure_rng` — the audit amended rather than
  over-claimed (§7). That is the correct posture: re-verify every claim against
  current source, including your own.
- **For trust:** the determinism proof and the entropy proof are both single-command
  reproducible (`audit/determinism_test.c`, `audit/qrng_measure.c`). The only
  *unverified* slice is `secure_rng`'s health-test / Bell-test path — flagged as
  such, not asserted.

---

## 3. How WuBuMath and quantum_rng fit together

Honest framing first: **WuBuMath is a deterministic geometry/AD library;
quantum_rng is a random-number generator.** They are not the same kind of thing,
and WuBuMath does not need a PRNG to do its math. So the integration question is
really: *where does randomness legitimately enter a deterministic manifold-ML
stack, and is quantum_rng the right tool there?*

### 3a. Legitimate randomness touch-points (and the verdict)

| Use case in a WuBuMath-style stack | Need | Is quantum_rng appropriate? |
|---|---|---|
| Parameter / weight initialization | Uniform RNG, reproducible for experiments | ✅ **Seeded mode only** — `qrng_init(seed)` is byte-reproducible (audit-proven). Good for repeatable runs. |
| Stochastic Riemannian-SGD noise | Reproducible per-epoch noise | ✅ Seeded mode, re-seed per epoch. |
| Dropout / data-shuffle indices | Reproducible shuffle | ✅ Seeded mode. |
| **Cryptographic key material** | CSPRNG (RDSEED/`/dev/random`) | ⚠️ Only via the `secure_rng` layer (real hardware entropy) — and **only after** we build+run `secure_rng_test.c`. The core `quantum_rng.c` is **not** crypto-grade. |
| "True quantum randomness" | actual quantum process | ❌ Not present. The "quantum" label is marketing. Use a real QRNG hardware source if you truly need this. |

### 3b. Practical wiring (if you want quantum_rng as WuBuMath's RNG)

WuBuMath has no built-in RNG dependency today — it is deterministic. To use
quantum_rng as its sampler:

1. **Wrap seeded mode as WuBuMath's RNG.**
   Build `src/quantum_rng/quantum_rng.c` (it compiles standalone; the old
   `<sys/types.h>`/`M_PI` break is already fixed in this fork). Expose a thin
   adapter:
   ```c
   // wubu_rng_qrng.c  (adapter, not in WuBuMath core)
   #include "quantum_rng.h"
   static qrng_ctx *g;
   void wubu_rng_seed(const uint8_t *s, size_t n){ qrng_init(&g, s, n); }
   double wubu_rng_uniform(void){ return qrng_double(g); }   // in [0,1)
   ```
   Use **only in seeded mode** so WuBuMath training runs stay reproducible —
   which is exactly what you want for a math library (determinism over novelty).

   > **Verified 2026-07-12:** this exact adapter is committed as
   > `audit/wubu_rng_qrng.c` and builds+runs: seeded draws
   > `0.639463 0.863714 0.905283`, and after re-seed the sequence is
   > **byte-identical** — confirming reproducible, non-crypto RNG as claimed.

2. **Never call the unseeded path from WuBuMath.**
   Unseeded quantum_rng draws `gettimeofday`/`getpid` — predictable, not a
   CSPRNG. A deterministic math lib should not silently depend on wall-clock
   entropy. Seed explicitly.

3. **If you need real entropy (e.g. secure weight export), use `secure_rng`.**
   But gate it behind a build+run of `tests/secure_rng_test.c` first — the audit
   has **not** independently verified that path yet (§7, marked unverified).
   Until then, prefer the OS source directly (`/dev/random` / `RDSEED`) over
   routing through quantum_rng's claims.

### 3c. The honest recommendation

You do **not** need quantum_rng to make WuBuMath work. WuBuMath is deterministic
and that is a feature. The only reason to wire them is convenience: quantum_rng's
seeded mode is a perfectly good, audit-verified, reproducible uniform RNG for
initialization and stochastic-optimization noise. Use it **seeded, reproducible,
non-cryptographic** — precisely the regime the audit proved is honest. Do not
use it (or its "quantum" label) for anything requiring true unpredictability.

---

## 4. TL;DR

The audit stripped the quantum/63.999872-bit mythology off quantum_rng and proved
what's underneath: a correct, reproducible, well-mixed **classical** PRNG plus a
newly-added real hardware-entropy layer. It is fine as a seeded RNG for a
deterministic library like WuBuMath (initialization, SGD noise) — seed it
explicitly, keep runs reproducible, and keep it out of any security path. WuBuMath
doesn't need it, but if you want a verified-reproducible sampler, quantum_rng's
seeded mode is an honest choice.
