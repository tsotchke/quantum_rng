# 🌌 Quantum RNG

A high-performance quantum-inspired random number generator that leverages quantum mechanical principles in classical computing environments. This library simulates quantum phenomena on classical hardware to generate high-quality random numbers with proven entropy characteristics.

## ✨ Features

- Quantum-inspired random number generation using simulated quantum effects:
  - Quantum superposition
  - State vector evolution
  - Quantum entanglement
  - Decoherence simulation
- High entropy output (63.999872 bits/sample)
- Comprehensive test suite with statistical validation
- Extensive documentation and examples
- Cross-platform C implementation
- Hardware-optimized performance

## ⚡ Performance

- 4.82M operations per second
- 178.45 MB/sec throughput
- ~4KB context size
- Optimized for L1 cache usage
- Verified non-deterministic output
- Competitive with leading classical RNGs while providing quantum properties

## 🛠️ Installation

```bash
# Clone the repository
git clone https://github.com/tsotchke/quantum_rng.git
cd quantum_rng

# Build the library and examples
make
```

## 🚀 Quick Start

```c
#include <quantum_rng.h>

int main() {
    qrng_ctx *ctx;
    qrng_error err;
    
    // Initialize RNG
    err = qrng_init(&ctx, NULL, 0);
    if (err != QRNG_SUCCESS) {
        fprintf(stderr, "Failed to initialize: %s\n", qrng_error_string(err));
        return 1;
    }
    
    // Generate random numbers
    printf("Random uint64: %lu\n", qrng_uint64(ctx));
    printf("Random double: %f\n", qrng_double(ctx));
    printf("Random range [1,6]: %d\n", qrng_range(ctx, 1, 6));
    
    // Cleanup
    qrng_free(ctx);
    return 0;
}
```

## 📚 Documentation

- [Quantum RNG Deep Dive](docs/quantum_rng.md) - Comprehensive explanation of the quantum RNG's principles, advantages, and impact
- [API Reference](docs/api_reference.md) - Detailed API documentation with examples
- [Quantum Principles](docs/quantum_principles.md) - Technical details of quantum simulation
- [Performance Analysis](docs/performance_analysis.md) - Detailed performance metrics and comparisons

## 💡 Examples

### 🔒 Cryptography

The initial release includes two cryptographic examples that demonstrate the library's capabilities in security applications:

#### Key Derivation
- Implementation: [examples/crypto/key_derivation.c](examples/crypto/key_derivation.c)
- Tests: [examples/crypto/key_derivation_test.c](examples/crypto/key_derivation_test.c)
- Analysis: [examples/crypto/key_derivation_analysis.md](examples/crypto/key_derivation_analysis.md)

Features:
- Quantum-enhanced key derivation
- Multiple iterations showing optimization progress
- Comprehensive test suite
- Performance analysis

#### Key Exchange
- Implementation: [examples/crypto/key_exchange.c](examples/crypto/key_exchange.c)
- Tests: [examples/crypto/key_exchange_test.c](examples/crypto/key_exchange_test.c)
- Analysis: [examples/crypto/key_exchange_analysis.md](examples/crypto/key_exchange_analysis.md)

Features:
- Secure key exchange protocol
- Quantum entropy integration
- Test suite with security verification
- Performance benchmarks

### 💹 Finance

#### Monte Carlo Simulation
- Implementation: [examples/finance/monte_carlo.c](examples/finance/monte_carlo.c)
- Header: [examples/finance/monte_carlo.h](examples/finance/monte_carlo.h)
- Analysis: [examples/finance/monte_carlo_analysis.md](examples/finance/monte_carlo_analysis.md)

Features:
- Advanced financial modeling using quantum randomness
- Efficient path generation
- Statistical analysis tools
- Performance optimizations
- Comprehensive documentation

### 🎲 Games

#### Quantum Dice
- Implementation: [examples/games/quantum_dice.c](examples/games/quantum_dice.c)
- Header: [examples/games/quantum_dice.h](examples/games/quantum_dice.h)

A simple but effective demonstration of the RNG in action:
- Fair dice rolling implementation
- Configurable sides (d4, d6, d8, d10, d12, d20, etc.)
- Statistical distribution tests
- Example of basic RNG usage

## 🧪 Testing

The library includes a comprehensive test suite:

```bash
# Run all tests
make test

# Run specific test suites
./tests/comprehensive_test  # Full functionality verification
./tests/edge_cases_test    # Edge case handling
./tests/test_quantum_rng   # Core RNG validation
```

### Statistical Testing
```bash
# Run statistical tests
./tests/statistical/statistical_tests

# Run quantum property verification
./tests/quantum_stats
```

## 📊 Performance Testing

```bash
# Run full benchmark suite
make benchmark
./benchmark_suite

# Run specific benchmarks
./tests/benchmark_matrix   # Matrix operation performance
```

## 🔮 Future Improvements

We have several exciting examples and applications in development that will be released soon:

### Finance Applications
- Options Pricing - Black-Scholes model with quantum entropy
- Quantum Portfolio - Portfolio optimization using quantum principles
- Heston Model - Stochastic volatility modeling

### Game Development
- Quantum Evolution - Evolutionary algorithms with quantum randomness
- Particle System - Physics-based particle simulation
- Procedural Worlds - Terrain and world generation
- Quantum Slots - Fair slot machine implementation
- Terrain Generation - Advanced landscape generation
- Loot System - Fair item drop system

### Machine Learning
- Quantum Transformer - RNG-enhanced transformer architecture
- Neural Initialization - Quantum-inspired weight initialization
- Quantum GAN - Generative adversarial network with quantum noise

### Scientific Applications
- Molecular Dynamics - Particle simulation
- Quantum Walk - Random walk implementations
- Weather Simulation - Atmospheric modeling
- Quantum Noise - Advanced noise generation

### Networking
- Quantum Routing - Network routing algorithms
- Traffic Simulation - Network traffic modeling

Each of these examples will be thoroughly tested and documented before release, demonstrating the versatility of the Quantum RNG library across different domains.

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for details on:

- Code style and standards
- Testing requirements
- Documentation expectations
- Pull request process

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Nielsen & Chuang's "Quantum Computation and Quantum Information"
- The quantum computing research community
- All contributors and testers

## 📝 Citation

If you use this library in your research, please cite:

```bibtex
@software{quantum_rng,
  title = {Semi-Classical Quantum Random Number Generator With Examples},
  author = {tsotchke},
  year = {2024},
  url = {https://github.com/tsotchke/quantum_rng}
}
```