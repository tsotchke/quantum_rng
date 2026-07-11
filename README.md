# Quantum RNG

A random number generator built on a real simulation of quantum measurement, wrapped in a production-grade secure generator with hardware entropy and continuous NIST SP 800-90B health testing.

Most software that calls itself a "quantum" random number generator is a conventional algorithm dressed in physics vocabulary. This one is different in a way you can check for yourself. At its core is an actual quantum system — a state vector evolved by genuine unitary gates and collapsed by Born-rule measurement, exactly as quantum mechanics prescribes. The claim that it behaves quantum-mechanically is not asserted in prose; it is demonstrated by a built-in Bell test. That test measures a quantity called the CHSH value, which no classical system can push above 2. This generator measures **S in the range 2.83 to 2.90**, essentially at the theoretical quantum limit of 2√2 ≈ 2.828. A classical process cannot produce that number, and every build runs the test.

If you are new to this: randomness matters because the security of nearly everything digital — keys, passwords, secure connections — depends on numbers an adversary cannot predict. Quantum mechanics is the only part of nature we know to be fundamentally, provably unpredictable. This project shows how a real quantum-measurement process turns that unpredictability into usable random bytes, and it does so on an ordinary computer by simulating the physics faithfully and drawing its underlying entropy from the machine's hardware sources.

> 🌙 **Lineage.** The quantum-simulation and Bell-verified-RNG work begun here is carried forward in [Moonlab](https://github.com/tsotchke/moonlab), a full quantum computing simulator. See [Where this work continues](#-where-this-work-continues-moonlab).

## What makes it real

- **A genuine quantum state-vector engine.** Superposition via the Hadamard gate, entanglement via controlled-NOT, and interference via the phase and T gates, all applied to a true vector of complex amplitudes, with measurement modeled by the Born rule and wavefunction collapse. See `src/quantum_rng/quantum_state.c` and `quantum_gates.c`.
- **Verified quantum behavior.** A real Clauser-Horne-Shimony-Holt (CHSH) Bell-inequality test in `src/quantum_rng/bell_test.c` — the same experiment used to rule out classical explanations of entanglement in the laboratory. `make test_v3` runs it on every build.
- **Grover's search algorithm.** A working amplitude-amplification implementation (`src/quantum_rng/grover.c`, with SIMD and multi-core variants), used both as a demonstration and as a randomness-amplification path.
- **Hardware entropy.** The unpredictability that drives measurement is drawn from the operating system and CPU: RDSEED and RDRAND on x86, RNDR on ARM, `getrandom()`, `/dev/random`, `/dev/urandom`, and CPU timing jitter, arranged in an automatic fallback chain with per-source quality estimation (`src/entropy/`).
- **NIST SP 800-90B health tests.** The Repetition Count Test and Adaptive Proportion Test run continuously on the entropy stream to detect a failing or degraded source (`src/health/`). All 26 health tests pass.
- **A production secure generator.** A unified, thread-safe interface integrates every layer above, offering four operating modes (fast, quantum, hybrid, verified), automatic reseeding, and error callbacks (`src/secure_rng/`). All 23 integration tests pass.
- **GPU acceleration.** Batched state-vector kernels on both **Apple Metal** (`src/quantum_rng/metal/`) and **NVIDIA CUDA** (`src/quantum_rng/cuda/`) — Hadamard, oracle, Grover diffusion, Pauli gates, and batched Grover search. Verified on Apple Silicon, RTX 3090, and RTX 3050.

## Performance

Measured on an Apple M2 Ultra; reproduce with the `make` targets shown.

| Path | Throughput |
|------|-----------|
| Secure generator, quantum mode (hardware entropy + health tests + quantum mixing) | about 4.6–5.3 MB/s |
| Quantum core, direct measurement | about 6.9 MB/s |
| Grover-amplified generation | about 18.7 MB/s |
| Metal GPU batch (M2 Ultra) | run `make metal` |

These are honest single-run figures on one machine, and every one is reproducible with the commands above. The throughput reflects what the generator is actually doing: simulating a quantum measurement and health-testing the result for every byte it produces. Speed is deliberately not the headline. If you need large volumes of non-security-critical randomness, a classical generator will be far faster; if you need maximum assurance, the verified mode certifies its output with a Bell test. This generator occupies the ground between those extremes — randomness whose quality and provenance you can inspect.

## Building

```bash
git clone https://github.com/tsotchke/quantum_rng.git
cd quantum_rng
make                 # core library and the v3 self-test
make examples_all    # all 44 example programs, across 8 domains
make metal           # Apple Metal GPU benchmarks (macOS)
make cuda            # NVIDIA CUDA GPU benchmark (needs the CUDA toolkit)
make verify_all      # everything: core test suites plus every example
```

## Quick start

The recommended entry point is the secure generator, which combines every layer into one interface.

```c
#include "secure_rng/secure_rng.h"

int main(void) {
    secure_rng_ctx_t *ctx;
    if (secure_rng_init(&ctx) != SECURE_RNG_SUCCESS)
        return 1;

    uint8_t key[32];
    secure_rng_bytes(ctx, key, sizeof key);   /* hardware-seeded, health-tested, quantum-mixed */

    uint64_t v;    secure_rng_uint64(ctx, &v);
    int32_t  roll; secure_rng_range32(ctx, 1, 6, &roll);

    secure_rng_free(ctx);
    return 0;
}
```

## Verification

The project's central promise — that this is genuinely quantum, secure, and correct — is enforced by tests you can run yourself.

```bash
make test_v3            # quantum core: 18/18, including the CHSH Bell test
make test_health        # NIST SP 800-90B health tests: 26/26
make test_secure_rng    # secure generator integration: 23/23
make test_thread_safety # concurrent access and mode switching
```

A single command, `make verify_all`, builds and runs all of the above and then builds all 44 examples.

## Examples

The library ships 44 example programs across eight domains. Every one compiles cleanly under `-Wall -Wextra` and runs to a successful exit. Each has its own documentation under `examples/<domain>/`, and the catalogue lives in [docs/example_applications.md](docs/example_applications.md).

- **Cryptography** — key derivation, a deliberately transparent toy key agreement that walks through the attack breaking it, a hash chain secured with real SHA-256, Wiesner quantum money with verified counterfeit detection, and unbiased token and password generation.
- **Quantum algorithms** — Grover search for hash collisions and password recovery at several scales, a quantum-advantage demonstration, post-quantum cryptography sizing, a quantum-versus-classical Bell comparison, and Metal GPU benchmarks.
- **Finance** — Monte Carlo pricing with a correct Box-Muller Gaussian transform, Black-Scholes with exotic options and Greeks, the Heston stochastic-volatility model, and portfolio optimization.
- **Machine learning** — quantum-seeded neural-network initialization, a generative adversarial network, and a small transformer text generator.
- **Science** — molecular dynamics with a Langevin thermostat, quantum walks, correctly-spectred procedural noise, and a weather simulation.
- **Networking** — quantum-weighted routing and packet-traffic simulation.
- **Games** — provably fair dice, a Bell-certified lottery, slot machines, loot tables, entangled particle systems, and procedural world and terrain generation.
- **Testing** — a fuzz harness and a quantum-versus-classical statistical comparison.

The cryptographic examples are educational: they illustrate real concepts and their pitfalls, and they say plainly where a production system must use a vetted library instead.

## Security posture

The secure generator is engineered as a real cryptographically secure RNG: hardware-seeded, health-tested to the NIST SP 800-90B methodology, resistant to state-recovery backtracking, and continuously monitored. It is suitable for security-sensitive randomness in the documented modes. Formal FIPS 140-3 or SP 800-90B *certification* — as distinct from following the test methodology — requires evaluation by an accredited laboratory, which is outside the scope of this repository. The documentation states this plainly rather than implying a certificate that does not exist.

## Documentation

- [docs/quantum_principles.md](docs/quantum_principles.md) — the physics being simulated, from qubits to the Bell test
- [docs/quantum_rng.md](docs/quantum_rng.md) — architecture, end to end
- [docs/api_reference.md](docs/api_reference.md) — the complete API
- [docs/HEALTH_TESTS.md](docs/HEALTH_TESTS.md) — NIST SP 800-90B health testing
- [docs/PRODUCTION_READY.md](docs/PRODUCTION_READY.md) — deployment guide
- [docs/performance_analysis.md](docs/performance_analysis.md) — measured benchmarks and methodology
- [docs/example_applications.md](docs/example_applications.md) — guide to all 44 examples

## 🌙 Where this work continues: Moonlab

The quantum-simulation and Bell-verified-RNG work begun in this repository is carried forward in [Moonlab](https://github.com/tsotchke/moonlab).

Quantum RNG started from a single question: can you build a random number generator on a genuinely simulated quantum measurement, and then prove the quantum behavior rather than merely claim it? Moonlab is where that question grew into a complete platform.

> **Moonlab** is a high-performance quantum computing simulator written in C, with bindings for Python, Rust, and TypeScript. It provides dense state-vector simulation of up to 32 qubits with SIMD acceleration, tensor-network methods (MPS, DMRG, TDVP), quantum algorithms (Grover, VQE, QAOA, QPE, and Bell tests), topological codes, quantum-chemistry simulation, post-quantum cryptography (ML-KEM and SHA-3), and — descended directly from this project — a Bell-verified quantum RNG.

For the complete quantum-simulation toolkit, use Moonlab: **https://github.com/tsotchke/moonlab**. Quantum RNG remains here as the compact, self-contained origin of that generator line: small enough to read from end to end, with every claim backed by a test you can run.

## License

MIT. See [LICENSE](LICENSE).

## Citation

```bibtex
@software{quantum_rng,
  title  = {Quantum RNG: A Verified Quantum-Measurement Random Number Generator
            with Hardware Entropy and NIST SP 800-90B Health Testing},
  author = {tsotchke},
  year   = {2026},
  url    = {https://github.com/tsotchke/quantum_rng}
}
```
