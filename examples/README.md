# Examples

Forty-four example programs across eight domains, each demonstrating a different use
of the quantum random number generator. Every program compiles cleanly under
`-Wall -Wextra` and runs to a successful exit.

Each domain directory has its own README with a per-program guide: what it does
(in plain language and in technical depth), what it teaches, and exactly how to
build and run it.

## Domains

| Domain | Programs | What it shows |
|--------|----------|---------------|
| [crypto/](crypto/README.md) | key derivation, key exchange, hash chain, quantum money, tokens, passwords | Cryptographic constructions and their pitfalls — educational, not production security |
| [quantum/](quantum/README.md) | Grover search, quantum-advantage and attack demos, post-quantum crypto, Metal GPU benchmarks | Quantum algorithms and where they beat classical methods |
| [finance/](finance/README.md) | Monte Carlo, Black-Scholes, Heston, portfolio optimization | Quantitative finance driven by high-quality randomness |
| [ml/](ml/README.md) | weight initialization, GAN, transformer | Randomness in machine learning |
| [science/](science/README.md) | molecular dynamics, quantum walks, procedural noise, weather | Physical and statistical simulation |
| [network/](network/README.md) | routing, traffic simulation | Randomized network modeling |
| [games/](games/README.md) | dice, Bell-certified lottery, slots, loot, particles, worlds, terrain | Provably fair game mechanics |
| [testing/](testing/README.md) | fuzzing, quantum-vs-classical comparison | Validating the generator itself |

## Building and running

From the repository root:

```bash
make                 # core library and self-test
make examples_all    # build all 44 example binaries
make metal           # Apple Metal GPU benchmarks (macOS)
```

Binaries are written to the repository root. On macOS, programs that use OpenMP
need `DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib` at runtime, and the two
crypto tests that link the shared library need `DYLD_LIBRARY_PATH=.`; each domain
README notes which programs require this and gives the exact command.

## A note on honesty

These examples are written to teach, and they say plainly what they are. The
cryptographic examples illustrate real concepts — and, in some cases,
deliberately illustrate how naive constructions fail — but they are not a
substitute for a vetted cryptographic library in production. Where an example
demonstrates a quantum algorithm, the README distinguishes the parts that are a
genuine simulation of quantum behavior from the parts that illustrate a
principle classically. No example prints a benchmark number it did not measure.

---

Part of the Quantum RNG examples. The full quantum-simulation toolkit and the
continued Bell-verified RNG live in Moonlab:
https://github.com/tsotchke/moonlab
