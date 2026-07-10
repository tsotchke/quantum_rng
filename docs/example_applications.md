# Example Applications

Quantum RNG ships **44 example programs across 8 domains**. Every one builds
`-Wall -Wextra` clean and runs to success:

```bash
make examples_all      # build all 44
make metal             # + Apple Metal GPU benchmarks (macOS)
```

Run any binary from the repo root; those using OpenMP need
`DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib` on macOS.

Each example is a teaching artifact: it demonstrates one idea clearly, is
algorithmically correct, and never overstates security. The cryptographic
examples are educational — for production secrets use a vetted library.

## Cryptography (`examples/crypto/`)

| Program | What it demonstrates |
|---------|----------------------|
| `key_derivation` | Deterministic KDF over an RNG stream; entropy measurement and its limits; why real systems use HKDF/Argon2. |
| `key_verification` | Reproducibility and validation of derived key material. |
| `key_exchange` | A deliberately transparent toy key agreement — including a worked eavesdropper attack showing why naive XOR agreement fails, and what ECDH/KEMs do instead. |
| `quantum_chain` | A hash chain with QRNG nonces and a real SHA-256 integrity check; tamper detection; safe handling of untrusted import data. |
| `quantum_money` | **Wiesner's quantum money**: unforgeable banknotes verified by state fidelity + CHSH certification. Demonstrated counterfeit detection. |
| `secure_token` / `password_gen` | Unbiased token/password generation via rejection sampling. |

## Quantum algorithms (`examples/quantum/`)

| Program | What it demonstrates |
|---------|----------------------|
| `grover_hash_collision`, `grover_password_crack`, `grover_large_scale_demo`, `grover_large_scale_optimized` | Grover amplitude amplification: √N search speedup, SIMD/large-register variants. |
| `grover_parallel_benchmark` | Multi-core (OpenMP) Grover throughput. |
| `quantum_advantage_demo` | Where quantum measurement beats classical sampling. |
| `quantum_attack_classical` | Grover applied against a classical construction. |
| `post_quantum_crypto` | Post-quantum parameter sizing. |
| `metal_gpu_benchmark`, `metal_batch_benchmark` | Apple Metal GPU state-vector kernels (macOS). |
| `phase3_phase4_benchmark` | End-to-end optimized-pipeline benchmark. |

## Finance (`examples/finance/`)

| Program | What it demonstrates |
|---------|----------------------|
| `monte_carlo` | Geometric Brownian motion with a correct Box–Muller Gaussian transform; convergence to theory. |
| `options_pricing` | Black–Scholes closed form + Monte Carlo; exotic (Asian/lookback) payoffs; Greeks. |
| `heston_model` | Stochastic-volatility pricing (CIR variance process, Itô-correct). |
| `quantum_portfolio` | Portfolio optimization (GA + Cholesky-correlated Monte Carlo), VaR/CVaR, drawdown. |

## Machine learning (`examples/ml/`)

| Program | What it demonstrates |
|---------|----------------------|
| `neural_init` | Quantum-seeded Xavier/He weight initialization; measured variances match theory. |
| `quantum_gan` | A small GAN with correct non-saturating backprop, trained on a ring distribution. |
| `quantum_transformer` | A minimal transformer text generator driven by the RNG. |

## Science (`examples/science/`)

| Program | What it demonstrates |
|---------|----------------------|
| `molecular_dynamics` | Velocity-Verlet integration with a Langevin thermostat; stable temperature and a sane radial distribution function. |
| `quantum_walk` | Discrete-time quantum walks (1–3D); ballistic spread vs classical diffusion. |
| `quantum_noise` | White / pink / brown / Perlin / fractal noise with correct spectra. |
| `weather_sim` | A minimal hydrological-cycle atmosphere model. |

## Networking (`examples/network/`)

| Program | What it demonstrates |
|---------|----------------------|
| `quantum_routing` | Reliability/latency/load-weighted quantum route selection over a connected topology. |
| `traffic_sim` | Store-and-forward packet simulation with congestion and burst modeling. |

## Games (`examples/games/`)

| Program | What it demonstrates |
|---------|----------------------|
| `quantum_dice` | Provably fair dice via unbiased rejection sampling. |
| `bell_certified_lottery` | Draws certified by a live CHSH Bell test (classical CHSH ≈ 1.4 vs quantum ≈ 2.83). |
| `quantum_slots`, `loot_system` | Fair weighted outcomes. |
| `particle_system` | Entanglement-linked particles with correct bookkeeping. |
| `procedural_worlds`, `terrain_generation` | Octave-noise world/terrain generation. |
| `quantum_evolution` | A small genetic-algorithm demo. |

## Testing (`examples/testing/`)

| Program | What it demonstrates |
|---------|----------------------|
| `fuzz_test` | Randomized API fuzzing (no crashes). |
| `quantum_vs_classical` | Side-by-side CHSH: a genuine local-hidden-variable model (≤2) vs the quantum simulation (≈2.83). |

## Flagship demo

`secure_rng_demo` walks the full production API end to end — initialization,
health testing, all generation calls, mode switching, reseeding, statistics.
Start here if you're integrating the library.

---

The full quantum-simulation toolkit and the continued Bell-verified quantum RNG
live in **[Moonlab](https://github.com/tsotchke/moonlab)**.
