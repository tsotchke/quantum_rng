# Documentation

Curated, maintained documentation for Quantum RNG.

## Start here

| Document | What it covers |
|----------|----------------|
| [quantum_principles.md](quantum_principles.md) | The physics being simulated — qubits, unitary gates, Born-rule measurement, and the CHSH Bell test that verifies genuine quantum behavior. Read this first to understand *why* it's a real quantum RNG. |
| [quantum_rng.md](quantum_rng.md) | Architecture deep-dive: hardware entropy → NIST SP 800-90B health tests → quantum state-vector simulation → the secure RNG modes → output. |
| [api_reference.md](api_reference.md) | Complete API, organized by layer. Start with the `secure_rng` interface (recommended for all users). |

## Operating & integrating

| Document | What it covers |
|----------|----------------|
| [PRODUCTION_READY.md](PRODUCTION_READY.md) | Deployment guide for the secure RNG. |
| [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) | Embedding the library in your application. |
| [HEALTH_TESTS.md](HEALTH_TESTS.md) | NIST SP 800-90B health testing (Repetition Count + Adaptive Proportion). |
| [HEALTH_TESTS_QUICK_START.md](HEALTH_TESTS_QUICK_START.md) | Fast reference for the health-test API. |

## Reference

| Document | What it covers |
|----------|----------------|
| [performance_analysis.md](performance_analysis.md) | Measured benchmarks and exact reproduction commands. |
| [example_applications.md](example_applications.md) | Guide to the 44 example programs across 8 domains. |

## Historical notes

Development status reports, optimization sprints, and roadmaps are archived
under [`archive/`](archive/). They are not maintained.

---

The full quantum-simulation toolkit — and the continued Bell-verified quantum
RNG — live in **[Moonlab](https://github.com/tsotchke/moonlab)**.
