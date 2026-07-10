# Quantum algorithm examples

These programs run the repo's state-vector quantum simulator to demonstrate
quantum algorithms and their consequences for cryptography: Grover's search (the
quadratic speedup for unstructured search), performance benchmarks that scale
Grover across CPU cores / SIMD / Apple GPU, worked "quantum advantage" and
"quantum attack" walkthroughs, and a tour of post-quantum defenses.

## How the quantum RNG fits in, and what "quantum" means here

The core is a genuine **state-vector simulation**: a register of *n* qubits is
2ⁿ complex amplitudes, and gates (Hadamard, CNOT, phase, oracle, diffusion)
transform that whole vector. Its randomness is validated by a CHSH **Bell test**
(classical bound 2, Tsirelson bound 2√2 ≈ 2.828; a value above 2 is the
fingerprint of true quantum correlations). Several programs here use that Bell
test directly as their verification tool.

Because this is a *simulator*, keep one honest distinction in mind throughout:

> Grover's genuine, provable advantage is in the **number of oracle
> iterations** — O(√N) versus O(N) classically — and every Grover demo here
> shows that reduction faithfully. **Wall-clock** "speedup" is a different
> matter: in simulation the oracle touches all 2ⁿ amplitudes each iteration, so
> for these small sizes the simulated quantum path is not automatically faster
> in real time. The programs hedge this in their own output ("simulation
> overhead", "overhead for small N"); read the wall-clock lines with that in
> mind. The iteration-count advantage is the real result.

Entropy sources vary by program: the Grover demos use the low-level `qrng_v3_*`
generator; `quantum_advantage_demo`, `quantum_attack_classical`, and
`post_quantum_crypto` use the recommended **`secure_rng`** API (hardware entropy
+ NIST SP 800-90B health tests + quantum mixing; modes `FAST` / `QUANTUM` /
`HYBRID` / `VERIFIED`, where `VERIFIED` runs a real Bell test before serving
bytes); `grover_parallel_benchmark` uses the background entropy pool; the Metal
benchmarks use hardware entropy directly; and `phase3_phase4_benchmark` reads
`/dev/urandom`.

## Building and running

The library builds from the repo root, and the example binaries are emitted
**into the repo root** (not this directory):

```sh
cd ../..          # repo root: quantum_rng_rebuild/
make              # builds the core library

# Single-file Grover demos (pattern rule QUANTUM_SINGLE):
make grover_hash_collision
make grover_large_scale_demo
make grover_large_scale_optimized
make grover_password_crack
make phase3_phase4_benchmark

# Advantage / attack / post-quantum trio:
make quantum_examples            # post_quantum_crypto quantum_advantage_demo quantum_attack_classical

# Everything (except Metal, which is opt-in):
make examples_all
```

Run from the repo root, e.g. `./grover_hash_collision`. These link the library
objects statically, so no shared-library path is needed — **except** for OpenMP
and Metal, below.

### OpenMP program

`grover_parallel_benchmark` is multi-threaded with OpenMP. Build with
`make grover_parallel_benchmark` (or `make parallel_bench` to build and run it).
On macOS the library links Homebrew's libomp, so set the loader path at runtime:

```sh
make grover_parallel_benchmark
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib ./grover_parallel_benchmark --full
```

`metal_batch_benchmark` is also compiled with `-fopenmp` (its CPU-parallel
baseline uses `#pragma omp parallel for`) and therefore needs the same libomp
path at runtime.

### Metal GPU programs (macOS + Apple Silicon only)

`metal_gpu_benchmark` and `metal_batch_benchmark` require a Mac with an Apple
GPU and the Metal + Foundation frameworks. They are **not** part of
`examples_all`; build them with the dedicated target, which uses Apple's system
clang (Homebrew LLVM cannot target the Metal runtime):

```sh
make metal        # builds metal_gpu_benchmark and metal_batch_benchmark
./metal_gpu_benchmark
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib ./metal_batch_benchmark
```

Each program's command-line options are noted in its entry below (most take
none).

---

## Program reference

### grover_hash_collision (`main()`, no args)

**Plain:** Given a hash output, find an input that produces it — first by classic
trial and error, then with Grover's quantum search — and compare.

**Technical:** Works over an 8-qubit register (256-value space). Builds a uniform
superposition with Hadamards, then repeats `[oracle → diffusion]` for the optimal
≈ (π/4)√(N/M) iterations, where the oracle phase-flips every input whose 8-bit
`simple_hash` equals the target. Reports the amplified probability mass on
solution states and samples a measurement from the final distribution. Runs three
targets (two random, one fixed `0xFF`). Entropy via `qrng_v3_*`.

**Teaches:** the Grover loop (oracle + diffusion), optimal iteration count, and
amplitude amplification.

**Caveat:** `simple_hash` is an 8-bit toy chosen to fit 8 qubits, not a
cryptographic hash. See the note above about iteration count vs. wall-clock.

### grover_large_scale_demo (`main()`, no args)

**Plain:** Shows the quantum search advantage growing as the problem gets bigger.

**Technical:** Same Grover structure, run progressively at 10, 12, 14, and 16
qubits (1K → 64K values), reporting the iteration reduction versus brute force
and the Grover efficiency (fraction of the theoretical √N). This is the
straightforward baseline that `grover_large_scale_optimized` is measured against.

**Teaches:** how the O(√N) vs O(N) gap widens with scale.

### grover_large_scale_optimized (`main()`, no args)

**Plain:** The same large-scale search, re-engineered to run much faster in the
simulator, while keeping the algorithm correct.

**Technical:** Implements four "Tier 1" optimizations over the baseline: (1) a
**hash cache** precomputing every hash once; (2) a **sparse oracle** that flips
only the (few) solution amplitudes instead of scanning all 2ⁿ each iteration;
(3) a **SIMD Hadamard** (ARM NEON when available, otherwise a stride-based scalar
fallback); and (4) **adaptive stopping** that watches solution probability and
halts on convergence, over-rotation, or a plateau. Runs at 10/12/14/16 qubits and
prints which optimizations are active.

**Teaches:** that algorithmic identity is preserved while the constant factors
(oracle cost, gate vectorization, early stopping) are optimized.

**Caveat:** The closing summary references baseline timings for you to compare
against; run this and `grover_large_scale_demo` back to back to see the actual
delta on your machine rather than relying on the printed target numbers.

### grover_password_crack (`main()`, no args)

**Plain:** Cracks a "password" whose hash is deliberately expensive to compute,
the regime where a search-count advantage matters most.

**Technical:** Uses a 1000-round iterated hash (`HASH_ROUNDS`, a stand-in for a
bcrypt-style cost factor) so each guess is costly. Runs Grover at 10, 12, and 14
qubits, reporting attempts, total hash operations, and the iteration reduction
versus classical brute force. Entropy via `qrng_v3_*`.

**Teaches:** why Grover threatens brute-force protection (AES-128 → 2⁶⁴ effective,
hence "double the key size"), and how expensive per-guess work interacts with a
√N advantage.

**Caveat:** The headline "wins on wall-clock" framing is a simulation artifact —
the simulated oracle still evaluates the expensive hash across the whole search
space per iteration. The dependable claim is the **iteration** reduction.

### grover_parallel_benchmark (`main()`, OpenMP)

**Plain:** Measures how much faster many Grover searches run when spread across
CPU cores.

**Technical:** Benchmarks the library's parallel Grover (`grover_parallel_*`) by
running batches of independent 16-qubit searches across cores, reporting
throughput, speedup, and parallel efficiency. Reports whether OpenMP was compiled
in and the optimal batch size for the machine. Entropy from a background entropy
pool.

**Options:**

```
./grover_parallel_benchmark            # quick batch benchmarks (default)
./grover_parallel_benchmark --full     # full sequential-vs-parallel benchmark
./grover_parallel_benchmark --scaling  # speedup vs core-count table
./grover_parallel_benchmark --help
```

**Run:** needs libomp at runtime on macOS —
`DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib ./grover_parallel_benchmark`.

**Caveat:** Speedups depend on your core count and system load; the "M2 Ultra
24-core" framing is the machine it was tuned on, not a promise.

### phase3_phase4_benchmark (`main()`, no args)

**Plain:** A broad performance sweep — memory, gates, normalization, and Grover
scaling — up to large qubit counts, using Apple's Accelerate math library.

**Technical:** Prints CPU/RAM/cache info via `sysctl`, then benchmarks:
Accelerate/AMX capabilities, state normalization, Hadamard/CNOT gate throughput,
memory allocation/init from 16 up to ~30 qubits (stopping when allocation fails),
and Grover search scaling from 8 up to 22 qubits (stopping past ~10 s). Entropy
comes from `/dev/urandom`, not the QRNG.

**Teaches:** where a state-vector simulator's cost actually goes (memory
bandwidth dominates) and how it scales with qubit count.

**Notes:** Requires the Accelerate framework. Every figure in the final
performance summary is captured live during the run (measured Grover time and
largest completed search, gate times at the top qubit count, peak normalization
throughput, largest initialized state, and the detected Accelerate capability);
earlier versions printed fixed narrative speedup constants there, which have been
removed. The tables above the summary are measured live as well.

### metal_gpu_benchmark (`main()`, no args — macOS/Metal)

**Plain:** Times the same Grover search on the CPU versus the Apple GPU.

**Technical:** For 8–16 qubits, runs an identical Grover search on the CPU and via
Metal compute kernels (`metal_grover_search`), reporting per-size and overall
speedup, then benchmarks individual GPU kernels (Hadamard, oracle, diffusion) in
MOps/s. Uses hardware entropy. Exits with an error if Metal is unavailable.

**Teaches:** GPU vs CPU trade-offs for state-vector operations (a single small
search is latency-bound; the GPU shines on throughput — see the batch benchmark).

**Build/run:** `make metal` then `./metal_gpu_benchmark`. Numbers are measured
live on your GPU.

### metal_batch_benchmark (`main()`, no args — macOS/Metal + OpenMP)

**Plain:** Shows the GPU's real strength: running *many* Grover searches at once.

**Technical:** Compares three strategies on batches of 16-qubit searches —
CPU sequential, CPU parallel (OpenMP), and a single batched Metal kernel launch
(`metal_grover_batch_search`) — at batch sizes 24/48/76, with a detailed 76-search
analysis and throughput comparison. Uses hardware entropy.

**Teaches:** GPUs optimize **throughput**, not single-operation **latency** — the
program's own closing note: single search → CPU wins, batch of 76+ → GPU wins.

**Build/run:** `make metal` then
`DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib ./metal_batch_benchmark` (it needs
libomp for the CPU-parallel baseline). Numbers are measured live.

### quantum_advantage_demo (`main()`, no args)

**Plain:** Four short demos of things quantum randomness/superposition can do that
classical PRNGs can't.

**Technical:** Built on `secure_rng` + `bell_test` + quantum gates. (1) **Bell
test detection** — prepares a |Φ⁺⟩ Bell state and runs `bell_test_chsh`, showing a
CHSH value above the classical bound 2. (2) **Deutsch–Jozsa** oracle problem. (3)
**Haar/sampling** advantage. (4) **Source certification** — min-entropy,
autocorrelation, and a Bell test combined into a pass/fail verdict.

**Teaches:** Bell-inequality violation as a genuine quantum witness, plus the
*shape* of oracle and sampling speedups.

**Caveat:** Only demos 1 and 4 genuinely exercise the quantum simulator and the
CHSH test. Demos 2 and 3 are **illustrative** — the decision/operation counts are
computed classically to convey the principle, not by running a full quantum
algorithm. The headline "for 256 bits: 1 query" figures are the theoretical
promise, not something computed here.

### quantum_attack_classical (`main()`, no args)

**Plain:** Walks through how a future quantum computer would break today's crypto
— Grover on passwords, and Shor's algorithm against discrete-log, elliptic curves,
and RSA.

**Technical:** Built on `secure_rng` + `grover` + quantum state/gates. Demo 1
runs a **real Grover search** (`grover_search`) on an 8-qubit space to find a
password index. Demos 2–4 (discrete log / ECDSA / RSA factoring) are **classical
simulations of Shor's principle**: "quantum period finding" is a classical loop,
and the crypto is toy-sized (p = 23, N = 61×53) so you can watch the math. Closes
with a threat/timeline summary and post-quantum recommendations.

**Teaches:** which primitives Shor breaks (RSA, DH, ECC — completely) versus what
Grover merely weakens (symmetric crypto — double the key size), and the
"store-now-decrypt-later" risk.

**Caveat:** The file's own banner says it plainly — these are **educational
simulations**, not attacks that run on a quantum computer. Only the Grover demo
uses the quantum simulator; the Shor demos illustrate the principle classically.

### post_quantum_crypto (`main()`, no args)

**Plain:** The defensive counterpart — four cryptographic approaches believed to
survive quantum computers, all seeded from quantum randomness.

**Technical:** Built on `secure_rng`. (1) **BB84** quantum key distribution —
Alice/Bob basis choice, sifting, and eavesdrop-detection framing. (2) **LWE
lattice crypto** — a small but real Learning-With-Errors keygen/encrypt/decrypt of
one bit (dimension 256, q = 3329, Gaussian errors via Box–Muller over
`secure_rng`), the basis of NIST's Kyber/Dilithium. (3) **Lamport one-time
signatures** — hash-based signing with tamper detection. (4) **One-time pad** —
XOR with a quantum-random pad, demonstrating information-theoretic perfect
secrecy (any plaintext is equally consistent with the ciphertext).

**Teaches:** the families of post-quantum defense — QKD (physics-based), lattice
(LWE hardness), hash-based signatures, and the OTP's perfect secrecy — and why
each needs high-quality randomness.

**Caveats:** Educational. BB84 is a **classical simulation** of the protocol (no
real photon states are disturbed; the eavesdrop-detection loop is a placeholder).
The Lamport demo uses a **toy XOR "hash"** (its own comment says "in production use
SHA-3"), so it teaches the signature structure, not real collision resistance. The
LWE and one-time-pad demos are genuine small implementations over `secure_rng`.

---

Part of the Quantum RNG examples. The full quantum-simulation toolkit and the
continued Bell-verified RNG live in Moonlab:
https://github.com/tsotchke/moonlab
