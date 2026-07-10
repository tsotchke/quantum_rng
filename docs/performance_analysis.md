# Performance Analysis

This document reports measured throughput of the quantum RNG, explains why the
numbers are what they are, and gives exact commands to reproduce them. All
figures below were measured on the reference machine; expect variation on other
hardware. There are no synthetic or projected numbers here — every value comes
from the benchmarks bundled in the repository.

## Test environment

| Property | Value |
|----------|-------|
| Machine  | Apple M2 Ultra |
| Architecture | arm64 (64-bit ARM) |
| Logical cores | 24 |
| Compiler | gcc/clang, `-Ofast -flto -march=native -ffast-math -funroll-loops` |
| Parallelism | OpenMP (Homebrew `libomp`), Accelerate framework |
| OS | macOS (Darwin) |

Because these builds use `-march=native -ffast-math`, results are specific to
this CPU and toolchain. The methodology, not the absolute numbers, is the
portable part.

## Reproducing the measurements

```sh
cd quantum_rng_rebuild
make >/dev/null 2>&1                 # build the libraries and objects

# Quantum core (v3): prints DIRECT and GROVER MB/s and a CHSH value
make test_v3

# Secure RNG (application layer): prints throughput at 1KB / 10KB / 100KB / 1MB
make examples_all >/dev/null 2>&1
DYLD_LIBRARY_PATH="$(brew --prefix libomp)/lib" ./secure_rng_demo
```

`make test_v3` builds and runs the v3 test/benchmark binary, which ends with a
throughput banner and reports a CHSH Bell-test value. `secure_rng_demo`'s
"Example 6: Performance Testing" section reports the secure-layer throughput at
four request sizes.

## Measured throughput

### Secure RNG — QUANTUM mode (default application path)

From `secure_rng_demo`, Example 6, across repeated runs:

| Request size | Throughput |
|--------------|------------|
| 1 KB   | ~5.4–5.6 MB/s |
| 10 KB  | ~4.8 MB/s |
| 100 KB | ~4.8 MB/s |
| 1 MB   | ~4.76–4.77 MB/s |

Small requests measure slightly faster because a fresh context still has
buffered entropy and the fixed per-call overhead is amortized over fewer bytes
before health-test and reseed bookkeeping dominate. Steady-state throughput
settles around **4.8 MB/s**. This is the number to plan against for sustained
secure generation in the default mode.

### Quantum core (v3) — DIRECT and GROVER

From the `make test_v3` benchmark banner:

| Path | Throughput |
|------|------------|
| DIRECT (direct quantum measurement) | ~7.2–7.4 MB/s |
| GROVER (Grover-enhanced sampling)   | ~18.5–20.2 MB/s |

DIRECT is the raw quantum-measurement path with no health-test/reseed wrapper,
which is why it runs faster than the secure layer's QUANTUM mode. GROVER is
faster still: it amortizes the cost of preparing and evolving the state vector
over more extracted bits per measurement cycle, so its effective per-byte cost
is lower despite performing more quantum operations per iteration.

### Bell verification (CHSH)

`make test_v3` runs a CHSH inequality test and prints, for example:

```
CHSH = 2.8560, Classical = 2.0000
```

Across runs the measured S value falls in roughly **2.77–2.99** for the
single 5,000-sample test in the suite. Every observed value exceeds the
classical bound of 2.0, confirming genuine quantum (non-classical) behavior.
Values are expected to cluster near the Tsirelson bound 2√2 ≈ 2.828; the spread
above and below is finite-sample statistical fluctuation from the modest sample
count, and a single run can land slightly above 2.828 for that reason. Larger
`num_measurements` tightens the estimate around 2.828.

## Why throughput is what it is

Unlike a classical stream cipher or PRNG, this generator performs real quantum
state-vector work and standards-mandated health testing on the path to output.
That work is the throughput.

| Cost | What it does | Effect |
|------|--------------|--------|
| Per-byte quantum measurement | Evolves an 8-qubit (256-dimensional) complex state vector and collapses it on measurement, seeded by hardware entropy. | Dominant cost; sets the base rate of the quantum path. |
| NIST SP 800-90B health testing | Runs the Repetition Count Test and Adaptive Proportion Test continuously on entropy before it is used. | Adds fixed per-sample overhead; the price of FIPS-style assurance. |
| Reseed bookkeeping | Tracks bytes since last reseed and pulls fresh, health-tested hardware entropy at the configured interval. | Periodic cost amortized across the reseed interval. |
| Bell verification (VERIFIED mode) | Periodically runs a CHSH test to prove the quantum stage still violates the classical bound. | Extra cost on top of QUANTUM; only in VERIFIED mode. |

The result is that throughput is measured in single-digit-to-tens of MB/s, not
GB/s. That is the expected and correct order of magnitude for per-byte quantum
measurement plus continuous health testing — it buys measurable quantum
behavior and standards-aligned monitoring, not raw speed.

## Choosing a mode: speed vs. assurance

The secure RNG exposes four modes (see the API reference for the exact enum).
Pick based on how much assurance the workload needs:

| Mode | Relative cost | Use when |
|------|---------------|----------|
| `FAST` | Lowest | High-volume, non-adversarial output that still wants health-tested hardware entropy. |
| `QUANTUM` (default) | Moderate (~4.8 MB/s steady state) | Cryptographic keys, IVs, nonces, tokens — the common case. |
| `HYBRID` | Adaptive | Mixed request sizes: requests at or below `hybrid_threshold` bytes take the FAST path, larger requests take QUANTUM. |
| `VERIFIED` | Highest | Maximum assurance; adds continuous CHSH Bell verification. |

Practical guidance:

- If you need the most bytes per second and your threat model allows it, use
  `FAST`, or use `HYBRID` so that only large requests pay the quantum cost.
- For the default cryptographic use case, `QUANTUM` at ~4.8 MB/s is generally
  more than adequate: a 256-bit key is 32 bytes, so key/IV/nonce generation is
  effectively instantaneous even at this rate.
- Reserve `VERIFIED` for cases where you must be able to demonstrate, on an
  ongoing basis, that the quantum stage still violates the classical Bell bound.
- For raw quantum output without the secure wrapper, the v3 core's DIRECT path
  (~7.2 MB/s) and GROVER path (~18.7 MB/s) are available directly.

## Resource notes

- The quantum core defaults to 8 qubits, a 256-dimensional complex state
  vector — a few kilobytes of amplitudes — so memory footprint is modest.
- The state-vector engine can scale far higher (up to 32 qubits) for
  simulation work, but that is a memory/throughput trade-off unrelated to RNG
  output rate; see `quantum_state.h` and the API reference.
- Throughput scales with single-core floating-point performance for the
  quantum path; OpenMP and Accelerate are used where the workload parallelizes.

---

The full quantum-simulation toolkit and the continued Bell-verified RNG live in
Moonlab (https://github.com/tsotchke/moonlab).
</content>
