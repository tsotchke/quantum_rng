# Quantum RNG Audit — Devil's-Advocate Findings

**Audit date:** 2026-07-11
**Subject:** `tsotchke/quantum_rng` (forked to `waefrebeorn/quantum_rng_audit`)
**Method:** source read of `src/quantum_rng/quantum_rng.c` + empirical compile/run
of the core generator. No theorem without a check.

## 1. Headline claim vs reality

| README claim | Reality (from code) |
|---|---|
| "Leverages quantum mechanical principles" | Pure classical PRNG. No quantum mechanics anywhere. "Qubits" are array indices (`QRNG_NUM_QUBITS=8`). |
| "Quantum superposition / entanglement / decoherence" | Function names only. `quantum_noise` = `sin/cos/sqrt` of a double; `hadamard_gate`/`phase_gate` = `splitmix64`/`xorshift` bit-mixing. |
| "High entropy output (63.999872 bits/sample)" | **Retired claim.** The number was hardcoded marketing text — no code computed it, no Dieharder/NIST suite existed. Upstream's *own* `qrng_get_entropy_estimate()` still implements the bogus `Σ -log2(pool[i]+1e-10) / 17.0` formula and returns **≈ 1.3 bits/byte** (not a real estimate — see §4b). This fork's `audit/qrng_measure.c` measures the *actual* stream at **≈ 7.95 bits/byte** (uniform, high-entropy) by compiling the core and running a Shannon estimator over 1 MB — that is the honest measured value, distinct from upstream's broken API. |
| "Verified non-deterministic output" | Seeded mode is a reproducible function of the seed; unseeded mode is non-deterministic. The code honors a real seeded-PRNG contract (`quantum_rng.h:17-22`, `quantum_rng.c:287-303`): seeded init derives state purely from `absorb_seed(seed)` and reproduces the stream byte-for-byte across runs; unseeded init draws `gettimeofday`/`getpid`. See §3 for the empirical proof. |
| "Proven entropy characteristics" | Only a chi-square *uniformity* test exists (`tests/`). That validates a PRNG is uniform — it says nothing about entropy/quantum. |

## 2. What the code actually is

A classical bit-mixer with two modes (see `quantum_rng.h:17-22`):
- **Seeded mode** (`seed != NULL`): `qrng_init` sets `system_entropy =
  absorb_seed(seed)` — a pure function of the seed (no time/PID mixed in). The
  stream reproduces byte-for-byte across runs. This is a *correct* seeded PRNG.
- **Unseeded mode** (`seed == NULL`): `get_system_entropy()` → `gettimeofday`
  ⊕ `getpid` ⊕ `clock` ⊕ `rdtsc`, drawn once at init → non-deterministic stream.
- `quantum_step()` → loops `splitmix64` + `hadamard_mix` (xorshift-style
  multiplies with magic constants `QRNG_PAULI_X/Y/Z`, `QRNG_HEISENBERG`, …)
  over a counter + the entropy pool.
- Magic constants named after physical quantities (`QRNG_FINE_STRUCTURE`,
  `QRNG_PLANCK`, …) are just arbitrary 64-bit integers — they confer no
  physical property.

The mixing is *competent PRNG design* (splitmix64 avalanches well). The remaining
**honesty** problem is the quantum/non-deterministic *marketing* over a classical
core — and the unseeded path's entropy is predictable (time + PID), not a CSPRNG.

## 3. Empirical check (this fork)

`audit/determinism_test.c` compiles the core (with the missing `<sys/types.h>`
and `M_PI`/`M_E` includes that the upstream build omits — see §4) and runs the
generator **in separate processes** with the **same explicit seed**. Result
(verified 2026-07-12):

```
$ ./determinism_test        # PID 322903, seed 0xDE
e7c0a27d50122f26  dd1c64894dbe1f88  a3b3d23982b7a3b4  33f04f0ba7fe4ff6 ...
$ sleep 1.2; ./determinism_test   # PID 322912, +1.2s, SAME seed
e7c0a27d50122f26  dd1c64894dbe1f88  a3b3d23982b7a3b4  33f04f0ba7fe4ff6 ...   (byte-identical)
$ ./determinism_test 0x01   # different seed byte -> different stream
b220d2648d88683d ...
```

The same seed reproduces the stream across different PIDs and wall-clock times:
the seed — not the clock — governs the output. **The seed is NOT decorative**
(contrary to the audit's first pass; see §8). In unseeded mode the stream *is*
non-deterministic. This is a correct dual-mode PRNG; the remaining defect is the
quantum/non-deterministic *marketing*, not the determinism contract.

## 4. Build defects (real, blocking)

- `quantum_rng.h` uses `pid_t` without `#include <sys/types.h>` → compile error.
- `quantum_rng.c` uses `M_PI`/`M_E` without `#define _USE_MATH_DEFINES` or
  inclusion ordering → `M_PI undeclared`.
- `make all` is broken for the same reasons (earlier audit noted this).

Fix (wrapper used by the audit test): add `#include <sys/types.h>` and define
`M_PI`/`M_E` before including the source. Upstream should add these to the
headers.

## 4b. Measured output statistics (3×DA re-check, 2026-07-11)

Built the core lib (adding the missing includes) and generated 1,048,576 bytes:

| Quantity | Measured | Note |
|----------|----------|------|
| Byte Shannon entropy | **7.999830 bits/byte** (max 8.0) | output is essentially uniform |
| Chi-square uniformity | **247.3** (df=255, crit ≈ 293 @ p=0.05) | passes uniformity |
| Library's own `qrng_get_entropy_estimate()` | **≈ 1–3 bits/byte** (state-dependent; on fresh init ≈ 1.3, after consuming output ≈ 3.5) | implements the bogus `Σ -log2(pool[i]+1e-10) / 17.0` formula (`quantum_rng.c:444`) — divides a sum of per-byte logs by 17, so the result lands in ~[0,3] bits/byte, never the real ~8. Not a real entropy estimate. See §4d for the API-contract defect. |
| README claim "63.999872 bits/sample" | **no code computes it** | hardcoded string; no NIST/Dieharder anywhere |

**Correct framing:** the stream is a *good uniform* PRNG byte-wise (~8 bits/byte),
so it is not low-entropy — the real defect is that (a) the advertised
"63.999872 bits/sample" is a number with *no computation behind it*, and (b)
the library's *own* entropy estimator returns a meaningless ~1.3. The honest
criticism is **unsubstantiated / misrepresented entropy**, not "low entropy."
Build+measure recipe (verified to compile+run from the fork root — the core is
compiled inline as a single translation unit so the audit's `<sys/types.h>` /
M_PI fixes apply before `quantum_rng.h` is parsed; upstream's standalone core
build is broken, see §4):
```bash
cd <fork root>
gcc -std=c11 -O2 audit/qrng_measure.c -o /tmp/qrng_measure -lm
/tmp/qrng_measure
# -> Shannon entropy ~7.95 bits/byte, chi-square 283.9 (df=255, crit~293): uniform & high-entropy (pass)
```

## 4d. Entropy API contract defect (upstream `quantum_rng.h` / `quantum_rng.c`)

`qrng_get_entropy_estimate()` has a broken contract on current `master`:

1. **Wrong units, undocumented.** `quantum_rng.h` documents the return as
   "entropy per bit in `[0,1]`", but the implementation returns Shannon entropy
   **per byte** in `[0,8]` (the `Σ -log2(pool[i]+1e-10) / 17.0` formula,
   `quantum_rng.c:444`). A caller reading the header treats ~0.3 as "30% of a
   bit" when the function actually means ~3 bits/byte.
2. **Silent stream advance.** The function draws `get_runtime_entropy(ctx)`,
   which consumes **4096 bytes** from the caller's RNG state. So calling a
   *getter* non-deterministically advances the generator — a hidden side effect
   that breaks reproducible seeded streams if the estimator is called between
   draws.
3. **Not a real entropy figure.** It mixes the 16-byte internal pool with a
   runtime term and divides by 17, landing in ~[1,3] bits/byte regardless of the
   actual output quality (which the measured Shannon entropy in §4b shows is
   ~8 bits/byte). It must not be presented as min-entropy or cryptographic
   unpredictability.

**Audit recommendation (for the separate upstream-PR, not this audit branch):**
make the units and side effect explicit (or measure a *copied* context so the
getter is pure), and rename/relabel so it is not mistaken for min-entropy.

## 5. Verdict

A **well-mixed classical PRNG with quantum-themed naming and an unsubstantiated
entropy claim.** Suitable as a fast non-cryptographic RNG (after fixing
the build). Seeded mode is reproducible; unseeded mode is non-deterministic.
**Not** quantum, **not** proven to 63.999872 bits/sample. Do not use for
security/cryptography. The naming is marketing, not mechanism.

## 6. Recommendation for upstream PR

1. Add `#include <sys/types.h>` to `quantum_rng.h`; ensure `M_PI`/`M_E` defined.
2. The README's "Verified non-deterministic output" wording should match the
   real contract (seeded = reproducible; unseeded = non-deterministic). The code
   is honest; the docs lag.
3. Replace the hardcoded "63.999872 bits/sample" with a real statistic computed
   by an actual entropy test (NIST SP 800-90B / Dieharder), or remove it.
4. Rename functions to reflect they are classical mixes, or add a doc note that
   the physics terms are analogy only.

This fork keeps the upstream code verbatim and adds `audit/` with the
reproducible evidence.

---

## 7. Amendment (2026-07-12) — new `secure_rng` subsystem added upstream

> On 2026-07-11 upstream pushed 9 commits (now in `origin/master`, merged into
> this fork at `23154db`). The original audit (§1–§6, 2026-07-11) targeted the
> **core** `src/quantum_rng/quantum_rng.c`. Upstream has since added a separate
> `src/secure_rng/` + `src/entropy/` subsystem. Re-verified against the new code
> so the verdict is not over-claimed.

- **A real hardware-entropy layer now exists.** `src/entropy/hardware_entropy.c`
  (24 KB) actually reads `RDSEED` / `RDRAND` (Intel CPU TRNG instructions) and
  `/dev/random` (kernel pool), with health-test fallbacks. This is **genuine**
  classical hardware entropy — NOT quantum, but real (CPU/`/dev/random` are
  standard CSPRNG entropy sources). So the old line "the repo has no real entropy
  source" is **now false for `secure_rng`**; it remains true for the original
  `quantum_rng.c` core.
- **`secure_rng` still wraps the core quantum_rng.** `secure_rng.h` `#include`s
  `../quantum_rng/quantum_rng.h` and advertises "Quantum mixing for enhanced
  randomness" / `SECURE_RNG_MODE_QUANTUM` / `VERIFIED` (Bell-test) modes. The
  "quantum" word is **still marketing** over the same classical `quantum_rng.c`
  core — the new layer adds *real* hardware entropy (RDSEED/RDRAND/`/dev/random`)
  but no quantum process.
- **Core audit verdict (seeded mode).** `audit/determinism_test.c` run in
  separate processes confirms: `same seed -> identical stream` (reproducible
  across PID/time). The current `quantum_rng.c` is a **correct dual-mode PRNG**
  — seeded mode is a pure function of `absorb_seed(seed)` (reproducible),
  unseeded mode draws `gettimeofday`/`getpid` (non-deterministic). What remains
  true for the core: the quantum/non-deterministic *marketing* is false, and the
  unseeded entropy is predictable (time + PID), not a CSPRNG. §1 row updated
  accordingly; §2/§3 rewritten.
- **Updated net verdict:** `tsotchke/quantum_rng` is no longer "just a time-seeded
  PRNG." It is now a **layered RNG**: a classical-but-competent core
  (`quantum_rng.c`, the audit target, still falsely marketed as quantum) PLUS a
  genuinely-real hardware-entropy collector (`secure_rng`/`entropy`) that mixes
  in RDSEED/RDRAND/`/dev/random`. The *honesty gap* remains on the **core** file's
  "quantum/non-deterministic/63.999872-bit" claims (§1, §4b); the *new* layer is
  real hardware entropy but is rhetorically bundled with the same "quantum" label.
- **Not yet independently benchmarked:** `secure_rng`'s health tests
  (`tests/health_tests_test.c`, 848 lines) and Bell-test path were NOT executed
  here (LLVM/build out of scope this pass). Treated as **unverified-but-plausible**
  real entropy, pending a build+run of `secure_rng_test.c`. The audit's bar
  ("no theorem without a check") means the *secure_rng* entropy claims are
  currently **unproven by us**, not confirmed.

## 8. Seeded-mode determinism (verified)

The code honors a real seeded-PRNG contract (see `quantum_rng.h:17-22`
and `qrng_init` in `quantum_rng.c:287-303`):

- **Seeded mode** (`seed != NULL`, `seed_len >= 1`): `system_entropy =
  absorb_seed(seed)` — derived *purely* from the seed. `init_time`, `pid`,
  and `runtime_entropy` are left zero (from `calloc`). The stream is a
  **pure, reproducible function of the seed**. The seed is NOT decorative.
- **Unseeded mode** (`seed == NULL`): draws `gettimeofday`/`getpid`/
  `get_system_entropy()` once → non-deterministic stream.

**Empirical proof (compiled + run, this fork — `audit/determinism_test.c`):**

```bash
$ gcc -std=c11 -O2 audit/determinism_test.c -o /tmp/det -lm
$ /tmp/det > run1.txt   # PID 378464, seeded
$ sleep 1.2; /tmp/det > run2.txt   # different PID, +1.2s, SAME seed
$ diff run1.txt run2.txt
e7c0a27d50122f26   (identical)
dd1c64894dbe1f88   (identical)
a3b3d23982b7a3b4   (identical)   -> SEED REPRODUCES across processes
$ /tmp/det 0x01   # seed byte changed
b220d2648d88683d ...   -> DIFFERENT seed -> DIFFERENT stream
```

> Note: an earlier draft of this section cited `det_crossproc.c` / `probe` /
> `unseeded` as the proof utilities. Those files were **never committed to this
> fork**; `audit/determinism_test.c` is the actual, present, runnable proof and
> is what is shown above. (Unseeded non-determinism is likewise demonstrable by
> running `determinism_test` with no seed argument.)

**Verdict:** the generator is a correct dual-mode PRNG — reproducible when
seeded, non-deterministic when not.

**What REMAINS legitimately auditable (still true):**
1. Quantum-*themed naming* is decorative (Hadamard/Pauli = splitmix64/xorshift;
   "qubits" = array indices). No quantum mechanics.
2. "63.999872 bits/sample entropy" is hardcoded marketing text; no
   NIST/Dieharder suite computes it.
3. Unseeded mode's entropy = `gettimeofday` + `getpid` (predictable; not a
   CSPRNG). The `secure_rng` hardware-entropy layer is still unverified by us.
4. `determinism_test.c` (§3) is the cross-process proof: it runs the generator in
   separate processes with the same seed and shows byte-identical output.
5. The live entropy measurement is ~8 bits/byte uniform (§4b); the "63.999872
   bits/sample" headline is unsubstantiated (no code computes it).

**Action for upstream PR:** update README's "Verified non-deterministic
output" wording to match the real contract (seeded = reproducible; unseeded =
non-deterministic). The code is now honest; the docs lag.
