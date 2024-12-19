# Example Applications

This document provides detailed documentation for the tested and verified example applications in the Quantum RNG library.

## Cryptography

### Key Derivation (key_derivation.c)
A memory-hard key derivation function that uses quantum entropy for enhanced security.

```bash
Usage: key_derivation [OPTIONS] password

Options:
  -i, --iterations N    Number of iterations (default: 100000)
  -m, --memory N        Memory size in KB (default: 1024KB)
  -k, --keysize N       Output key size in bytes (default: 32)
  -x, --quantum N       Quantum mixing percentage (1-100, default: 50)
  -s, --salt STRING     Salt value (random if not provided)
  -q, --quiet           Quiet mode (only output final key)
  -v, --verbose         Verbose mode (show additional statistics)
  -j, --json            Output results in JSON format
  -H, --hex             Output results in hexadecimal
  -P, --no-progress     Hide progress bar
  -E, --no-entropy      Skip entropy verification
  -o, --output FILE     Write output to file

Example Usage:
# Basic key derivation
key_derivation -i 10000 -m 2048 mypassword

# High security with maximum quantum mixing
key_derivation -i 100000 -m 4096 -x 100 -v mypassword
```

### Key Exchange (key_exchange.c)
Quantum-enhanced key exchange protocol for secure session key establishment.

```bash
Usage: key_exchange [OPTIONS]

Options:
  -r, --role ROLE       Role: initiator or responder
  -s, --seed STRING     Random seed
  -n, --rounds N        Number of exchange rounds (1-10, default: 3)
  -q, --quiet           Quiet mode (only output final key)
  -v, --verbose         Verbose mode (show protocol details)
  -j, --json            Output results in JSON format
  -x, --hex             Output results in hexadecimal
  -P, --no-progress     Hide progress bar
  -E, --no-entropy      Skip entropy verification
  -i, --interactive     Run in interactive mode
  -o, --output FILE     Write output to file

Example Usage:
# Start as initiator
key_exchange -r initiator -n 5 -v

# Interactive mode with entropy verification
key_exchange -i -v
```

### Quantum Chain (quantum_chain.c)
A bitcoin blockchain-inspired data structure using quantum entropy for block generation.

```bash
Usage: quantum_chain [OPTIONS]

Options:
  -b, --blocks N       Number of blocks
  -d, --difficulty N   Mining difficulty
  -s, --size N        Block size in bytes
  -q, --quantum N     Quantum entropy per block
  -v, --verify        Verify chain integrity
  -m, --mine          Enable mining simulation
  -o, --output FILE   Save chain to file

Example Usage:
# Create 10-block chain
quantum_chain -b 10 -d 4 -q 128

# Mine blocks with verification
quantum_chain -b 5 -m -v
```

## Finance

### Monte Carlo Simulation (monte_carlo.c)
Advanced financial modeling using quantum-enhanced random sampling.

```bash
Usage: monte_carlo [OPTIONS]

Options:
  -n, --simulations N    Number of simulations (1000-1000000)
  -d, --days N           Trading days (default: 252)
  -p, --price N          Initial price (default: 100.00)
  -v, --volatility N     Annual volatility (default: 0.20)
  -r, --rate N           Risk-free rate (default: 0.05)
  -y, --yield N          Dividend yield (default: 0.00)
  -s, --seed STRING      Random seed
  -q, --quiet            Quiet mode (only output final values)
  -V, --verbose          Verbose mode (show additional statistics)
  -j, --json             Output results in JSON format
  -c, --csv              Output results in CSV format
  -P, --no-progress      Hide progress bar
  -C, --confidence N     Confidence level (95 or 99)
  -o, --output FILE      Write output to file

Example Usage:
# Basic price simulation
monte_carlo -n 10000 -d 252 -p 100

# Detailed analysis with custom parameters
monte_carlo -n 50000 -v 0.30 -r 0.03 -V -j
```

## Games

### Quantum Dice (quantum_dice.c)
True random dice rolling implementation using quantum entropy.

```bash
Usage: quantum_dice [OPTIONS]

Options:
  -s, --sides N       Number of sides per die (2-100, default: 6)
  -n, --number N      Number of dice to roll (1-10, default: 1)
  -S, --seed STRING   Random seed
  -q, --quiet         Quiet mode (only output numbers)
  -v, --verbose       Verbose mode (show additional statistics)
  -j, --json          Output results in JSON format
  -a, --no-ascii      Disable ASCII art dice faces
  -f, --fairness      Verify fairness with statistical tests
  -i, --interactive   Run in interactive mode
  -o, --output FILE   Write output to file

Example Usage:
# Roll a single d6
quantum_dice

# Roll multiple dice with fairness verification
quantum_dice -n 3 -s 20 -f

# Interactive mode with ASCII art
quantum_dice -i
```

## Testing

The library includes comprehensive test suites in the tests/ directory:

### Statistical Testing (tests/statistical/)
1. Distribution Tests:
   - Uniformity analysis
   - Chi-square tests
   - Kolmogorov-Smirnov test
   - Entropy assessment

2. Sequence Tests:
   - Run length distribution
   - Autocorrelation analysis
   - Pattern detection
   - Frequency analysis

### Benchmark Suite (tests/benchmark_suite.c)
1. Speed Tests:
   - Generation throughput
   - Latency measurements
   - Scaling analysis
   - Memory usage

2. Quality Tests:
   - Bias detection
   - Pattern analysis
   - Statistical strength
   - Entropy measurement

3. Comparison Tools:
   - Classical vs quantum
   - Performance profiles
   - Resource utilization
   - Quality metrics

### Visualization Tools (tests/visualization/)
Interactive visualization tools for analyzing test results and random number distributions:
- Distribution plots
- Q-Q plots
- Entropy visualization
- Time series analysis
