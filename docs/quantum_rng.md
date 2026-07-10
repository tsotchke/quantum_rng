# Quantum RNG v3 — Architecture Deep Dive

This document describes the v3 random-number pipeline as it is actually built.
The system is a layered CSPRNG: physical hardware entropy is health-tested to
NIST SP 800-90B, used to drive an exact quantum state-vector simulation whose
Born-rule measurements are Bell-test-verified, and conditioned into output
through a unified secure-RNG front end with selectable operating modes.

For the physics — the state vector, the gate matrices, the Born rule, and the
CHSH Bell test — see `quantum_principles.md`. This document is about the
plumbing: where entropy comes from, how it is tested, how it becomes output,
and what the security posture honestly is.

## The pipeline at a glance

```
  ┌─────────────────────────────────────────────────────────────────┐
  │ Layer 1  Hardware entropy          src/entropy/hardware_entropy.c │
  │          RDSEED · RDRAND · RNDR/RNDRRS · getrandom() ·            │
  │          /dev/random · /dev/urandom · CPU jitter (fallback chain) │
  │          Background pre-generation  src/entropy/entropy_pool.c    │
  └───────────────────────────────┬─────────────────────────────────┘
                                   │ raw bytes
  ┌───────────────────────────────▼─────────────────────────────────┐
  │ Health tests (NIST SP 800-90B)          src/health/health_tests.c │
  │          Repetition Count Test · Adaptive Proportion Test         │
  │          Startup tests + continuous per-sample testing            │
  └───────────────────────────────┬─────────────────────────────────┘
                                   │ tested entropy
              ┌────────────────────┴────────────────────┐
              │                                          │
  ┌───────────▼─────────────────────┐   ┌───────────────▼────────────────────┐
  │ Layer 2  Quantum simulation      │   │ Conditioning core                   │
  │ src/quantum_rng/quantum_rng_v3.c │   │ src/quantum_rng/quantum_rng.c       │
  │ state vector + gates + Born-rule │   │ quantum-inspired mixing CSPRNG      │
  │ measurement + Grover + Bell test │   │ (used by secure_rng QUANTUM mode)   │
  │ modes: DIRECT / GROVER /         │   │                                     │
  │        BELL_VERIFIED             │   │                                     │
  └───────────┬─────────────────────┘   └───────────────┬────────────────────┘
              │                                          │
  ┌───────────▼──────────────────────────────────────────▼────────────────────┐
  │ Layer 3  Unified secure RNG              src/secure_rng/secure_rng.c        │
  │          modes: FAST · QUANTUM · HYBRID · VERIFIED                          │
  │          reseeding · thread safety · health monitoring · statistics        │
  └────────────────────────────────────────────────────────────────────────────┘
```

There are two distinct engines under the "quantum" name, and it is worth being
precise about which is which:

- **The state-vector simulation** (`quantum_rng_v3.c` with `quantum_state.c`,
  `quantum_gates.c`, `bell_test.c`, `grover.c`) is the genuine quantum
  simulation: a 2ⁿ-dimensional complex state vector, real unitary gates, and
  Born-rule measurement. Its randomness is physical, sourced from Layer 1.
- **The conditioning core** (`quantum_rng.c`, the `qrng_*` API) is a fast
  quantum-*inspired* mixing CSPRNG. It borrows quantum vocabulary
  (`hadamard_mix`, Pauli-derived constants) but it is a 64-bit integer mixing
  function, not a state-vector simulation. It is what the `secure_rng` QUANTUM
  mode uses as its output conditioner over health-tested hardware entropy.

Both are real, honest components; they are simply different things. The rest of
this document treats each on its own terms.

## Layer 1 — Hardware entropy

`src/entropy/hardware_entropy.c` collects raw entropy from every physical source
the platform offers and falls back gracefully. `entropy_get_bytes()` tries the
sources in quality order and returns the first that succeeds:

| Priority | Source | Detection | Notes |
|----------|--------|-----------|-------|
| 1 | `RDSEED` (x86) / `RNDRRS` (ARMv8.5) | CPUID leaf 7 / `FEAT_RNG` | reseeded TRNG output; retried up to 100× |
| 2 | `RDRAND` (x86) / `RNDR` (ARMv8.5) | CPUID leaf 1 / `FEAT_RNG` | conditioned TRNG; retried up to 10× |
| 3 | `getrandom()` | syscall probe | Linux kernel CSPRNG |
| 4 | `/dev/random` | `open()` | kernel entropy pool (hardware-backed) |
| 5 | `/dev/urandom` | `open()` | kernel CSPRNG |
| 6 | CPU jitter | always available | last-resort software source |

`entropy_init()` probes capabilities into an `entropy_capabilities_t` and picks
a preferred source; `entropy_get_bytes_from_source()` implements each one.

**ARM note, stated honestly.** On Apple Silicon the direct `RNDR`/`RNDRRS`
system-register reads are **disabled by default** (guarded out in
`entropy_init`). The register encoding is not stable across all macOS versions
and chip revisions and can raise an illegal-instruction fault; the code documents
this and deliberately relies on `/dev/random` instead, which on Apple Silicon is
fed by the hardware TRNG and delivers full-entropy bytes. The x86 `RDSEED`/
`RDRAND` paths are compiled and used where the CPUID bits are present.

**CPU jitter** (`entropy_jitter()`) is a careful software source, not a naive
timer read: it times a compute-bound loop, extracts entropy from the timing
delta and its second difference, mixes with a SipHash-derived function
(`crypto_mix`/`sipround`), and applies **von Neumann debiasing** to the bit
stream to remove first-order bias. It exists as a floor so the generator never
has *zero* sources; the quality estimator rates it conservatively (5 bits/byte
vs. 8 for the hardware TRNGs).

**Background pooling.** `src/entropy/entropy_pool.c` wraps the raw sources in a
pre-generated, continuously health-tested pool with an optional background
thread (`enable_background_thread`) that refills when the pool drops below
`refill_threshold`. The v3 engine uses a 64 KB pool by default. This decouples
consumers from the latency and blocking behavior of the underlying sources.

## Health testing — NIST SP 800-90B

Before any entropy is used it passes the two SP 800-90B health tests in
`src/health/health_tests.c`. These run at startup on a batch and then
continuously, one sample at a time, for the life of the generator.

- **Repetition Count Test (RCT)** — `health_test_rct()`. Detects a stuck source
  by failing if a single value repeats too many times in a row. The cutoff is
  `C = ⌈1 + 30/H⌉` (`health_calculate_rct_cutoff`), derived from a 2⁻³⁰ false-
  positive target at the configured min-entropy `H`.
- **Adaptive Proportion Test (APT)** — `health_test_apt()`. Detects gradual
  entropy loss by counting, within a window of 512 samples
  (`apt_window_size`), how often the window's first value recurs, and failing
  if that count exceeds a binomial cutoff (`health_calculate_apt_cutoff`, a
  normal approximation at ~6.9σ for the 2⁻³⁰ bound).
- **Startup tests** — `health_tests_startup()` runs the tests over 1024 samples
  and refuses to mark the generator operational unless they pass.

The default configuration is conservative: `min_entropy = 4.0` bits/byte, which
sizes the cutoffs defensively. A failure sets a callback and is surfaced to the
caller as a hard error, not swallowed. `health_tests_run()` is the per-sample
entry point; `secure_rng` invokes it on every byte it draws
(`collect_tested_entropy`).

## Layer 2 — Quantum state-vector simulation and measurement

`quantum_rng_v3.c` is the genuine quantum RNG. Its header comment lays out the
three-layer design whose purpose is to break the circular dependency "a quantum
measurement needs randomness, but the RNG *is* the randomness": the quantum
layer (Layer 2) draws its measurement randomness from the hardware entropy pool
(Layer 1), which knows nothing about quantum simulation.

`qrng_v3_init_with_config()` wires this together:

1. Initialize the hardware entropy pool (Layer 1).
2. Initialize an 8-qubit `quantum_state_t` (256-dimensional; `num_qubits`
   configurable up to `MAX_QUBITS`).
3. Bridge them with `quantum_entropy_init()`, installing
   `entropy_pool_callback` as the state's measurement-randomness source. This is
   the join point: every Born-rule draw in the simulation pulls from the tested
   hardware pool.

Output generation refills a 64 KB buffer using one of three modes:

- **`QRNG_V3_MODE_DIRECT`** (`extract_quantum_entropy`) — evolve the state
  through a random circuit (`evolve_quantum_state` selects gates —
  Hadamard, CNOT, R_y, X, Z, S, CZ, R_z — using hardware entropy), then repeatedly
  apply a few mixing gates and take a full-register Born-rule measurement
  (`quantum_measure_all_fast`), emitting the measured basis index as bytes. The
  state is re-evolved periodically to guarantee diversity.
- **`QRNG_V3_MODE_GROVER`** (`extract_grover_entropy`) — prepare the register in
  uniform superposition, apply a couple of Grover iterations
  (`grover_oracle` + `grover_diffusion`) toward a random target to shape a
  non-uniform amplitude distribution, then batch-measure from it. This is the
  Grover-enhanced sampling path.
- **`QRNG_V3_MODE_BELL_VERIFIED`** — generates as DIRECT but, when
  `enable_bell_monitoring` is set, periodically runs `qrng_v3_verify_quantum()`
  (a full `bell_test_chsh` on qubits 0 and 1) and refuses to proceed if the
  measured CHSH `S` falls below `min_acceptable_chsh` (default 2.4). The running
  CHSH mean/min/max are tracked in the stats and via `bell_test_monitor_t`.

`qrng_v3` also exposes higher-level quantum sampling:
`qrng_v3_grover_sample`, `qrng_v3_grover_sample_distribution` (amplitude
amplification toward an arbitrary target distribution, per Brassard et al.), and
`qrng_v3_grover_multi_target`. Diagnostics include
`qrng_v3_get_entanglement_entropy` (von Neumann entropy of half the register)
and `qrng_v3_is_quantum_verified`.

## The conditioning core

`quantum_rng.c` (`qrng_*`) is the mixing CSPRNG that `secure_rng` uses to
condition tested hardware entropy in QUANTUM mode. Internally it keeps an
8-word working state plus a 16-element entropy pool, and on each `quantum_step`
runs several mixing rounds built from `splitmix64` and `hadamard_mix` (a
strong integer avalanche function keyed by constants derived from physical
constants in `quantum_constants.h`), continuously folding in fresh runtime
entropy. `qrng_init` seeds it from the health-tested startup entropy;
`qrng_reseed` re-keys it; `qrng_bytes` fills output. It is fast and has good
statistical properties, but — to be clear — it is a conditioning/mixing
function, not the state-vector simulator. Naming aside, no unitary matrices or
Born-rule collapse happen here; those live in Layer 2.

## Layer 3 — The unified secure RNG

`src/secure_rng/secure_rng.c` is the production front end that most callers use.
`secure_rng_init_with_config()` performs the full secure bring-up:

1. Initialize hardware entropy (`entropy_init`) and, if
   `require_hardware_entropy` is set, verify at least one real source exists.
2. Initialize health tests with the configured SP 800-90B parameters.
3. Collect 2048 bytes of startup entropy and run the SP 800-90B startup tests;
   abort init if they fail.
4. Seed the `qrng` conditioning core with that tested entropy.
5. Optionally allocate an entropy cache and initialize a thread-safety rwlock.
6. Mark the context `OPERATIONAL`.

### The four modes

`secure_rng_bytes()` dispatches on the configured mode:

- **`FAST`** — return health-tested hardware entropy directly
  (`collect_tested_entropy`). Lowest latency; still fully health-tested.
- **`QUANTUM`** (default) — condition through the `qrng` core (`qrng_bytes`)
  over a reseeded, health-tested entropy base. The balanced default.
- **`HYBRID`** — choose per request by size: requests below
  `hybrid_threshold` (default 1 KB) use FAST, larger ones use QUANTUM, so small
  frequent draws stay cheap while bulk draws get full conditioning.
- **`VERIFIED`** — the maximum-assurance mode. Before serving output, it runs a
  CHSH Bell test (`run_bell_certification` in `secure_rng.c`, 2000 measurement
  pairs via `bell_test_chsh`, seeded from the context's hardware entropy) and
  confirms the quantum engine violates the classical bound, S > 2. If the source
  fails to violate the bound, the context enters the error state and returns
  `SECURE_RNG_ERROR_HEALTH_TEST_FAILED` rather than emitting bytes. The
  certificate is established at first use and renewed after every reseed; the
  most recent measured value is retained in `ctx->last_chsh_value`. This makes
  the VERIFIED label literal: no VERIFIED byte is served under an uncertified
  quantum source. (Independently, `qrng_v3`'s `BELL_VERIFIED` mode exposes the
  same CHSH machinery at the lower level via `qrng_v3_verify_quantum`.)

`secure_rng_set_mode()` switches modes at runtime; `secure_rng_mode_string()`
gives human-readable names.

### Reseeding

Reseeding is automatic and forward-secure in intent. With
`auto_reseed_enabled` (default on) and `reseed_interval` (default 1 MB), once
`bytes_since_reseed` crosses the interval the next `secure_rng_bytes()` call
triggers `secure_rng_reseed()`, which collects a fresh 1 KB of health-tested
entropy and re-keys the `qrng` core. `secure_rng_reseed_with_entropy()` lets a
caller inject external entropy, which is itself health-tested and XOR-mixed with
freshly collected hardware entropy before reseeding — external input can only
add uncertainty, never replace the hardware base.

### Thread safety

Thread safety is **opt-in** (`enable_thread_safety`, or
`secure_rng_init_threadsafe`). When enabled the context guards all operations
with a `pthread_rwlock` — write locks for generation and reseeding, read locks
for stats. It is off by default because the recommended high-throughput pattern
is one context per thread, which needs no locking at all. Either pattern is
supported; pick per your workload.

### Output API and statistics

`secure_rng_bytes` / `_uint64` / `_uint32` / `_double` produce raw bytes and
typed values; `_double` returns `[0,1)` with 53 bits of precision. Range
functions `secure_rng_range32` / `_range64` use **rejection sampling** against a
modulo threshold to produce unbiased values in `[min,max]`. `secure_rng_get_stats`
and `secure_rng_print_stats` expose bytes generated, reseed count, per-mode byte
counts, and RCT/APT failure counters; `secure_rng_get_health_stats` and
`secure_rng_get_entropy_caps` expose the underlying health and source state.
`secure_rng_self_test()` exercises the whole path end to end.

Throughout, secrets are zeroed on free and on error via `secure_memzero`
(`src/common/secure_memory.h`); on a health-test failure with `zeroize_on_error`
set, the output buffer is wiped before returning.

## Performance (Apple M2 Ultra)

Representative measured throughput on an Apple M2 Ultra:

| Path | Throughput |
|------|-----------|
| `secure_rng` QUANTUM mode | ~4.6–5.3 MB/s |
| `qrng_v3` DIRECT (quantum core) | ~6.9 MB/s |
| `qrng_v3` GROVER (amplified) | ~18.7 MB/s |
| CHSH `S` (measured, default config) | ≈ 2.83–2.90 (classical bound 2, Tsirelson 2√2 ≈ 2.828) |

These are indicative, not a contract — throughput depends on CPU, entropy-source
latency, build flags, and configuration. Measure on your own machine:

- `make test_v3` builds and runs the v3 test, which reports quantum-core
  throughput and prints the CHSH value and correlations from a live Bell test.
- `make parallel_bench` runs the multithreaded Grover benchmark.

(There is no `make benchmark` target in this tree; use the two above.)

## Security posture — stated honestly

- **This is real CSPRNG engineering.** Multi-source hardware entropy with a
  quality-ordered fallback chain; NIST SP 800-90B RCT and APT health tests run
  at startup and continuously; startup self-tests that gate the operational
  state; automatic reseeding with a bounded interval; external-entropy injection
  that can only add uncertainty; unbiased range sampling; and secure zeroization
  of key material. The design follows established practice for entropy sourcing,
  conditioning, and health monitoring.
- **It is not formally certified.** Nothing here has been through a FIPS 140-3 /
  SP 800-90B validation at an accredited testing laboratory (CAVP/CMVP). That is
  a formal, documented, lab-driven process and is explicitly out of scope for
  this project. Implementing the SP 800-90B *tests* is necessary for that path
  but is not the same as certification. Treat this as a well-engineered,
  standards-informed generator, not a certified module.
- **Documented design choices.** Direct ARM `RNDR`/`RNDRRS` is disabled by
  default on Apple Silicon in favor of the hardware-backed `/dev/random`; the
  choice is documented in the code and reflected above rather than papered over.

## Continuation

This quantum-simulation, Bell-verified RNG work continues in **Moonlab**
(<https://github.com/tsotchke/moonlab>), a full quantum-computing simulator.
Moonlab includes a Bell-verified quantum RNG descended from this engine, within
a larger framework of gates, circuits, and quantum algorithms.

## References

1. Turan, M. S., Barker, E., Kelsey, J., McKay, K. A., Baish, M. L., & Boyle, M.
   (2018). *NIST SP 800-90B: Recommendation for the Entropy Sources Used for
   Random Bit Generation.* — RCT/APT health tests, min-entropy, conditioning.
2. Barker, E., & Kelsey, J. (2015). *NIST SP 800-90A Rev. 1: Recommendation for
   Random Number Generation Using Deterministic Random Bit Generators.* —
   reseeding and DRBG structure.
3. Clauser, J. F., Horne, M. A., Shimony, A., & Holt, R. A. (1969). Proposed
   Experiment to Test Local Hidden-Variable Theories. *Physical Review Letters*,
   23(15), 880–884. — the CHSH Bell test used for verification.
4. Nielsen, M. A., & Chuang, I. L. (2010). *Quantum Computation and Quantum
   Information* (10th Anniversary ed.). Cambridge University Press. — gate and
   measurement semantics of the simulation layer.
5. Brassard, G., Høyer, P., Mosca, M., & Tapp, A. (2000). Quantum Amplitude
   Amplification and Estimation. — the Grover-mode amplitude amplification.
