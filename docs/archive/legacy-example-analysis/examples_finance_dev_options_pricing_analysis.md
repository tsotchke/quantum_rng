# Options Pricing Analysis

## Overview

The options pricing implementation provides a comprehensive tool for pricing various types of financial options using quantum random number generation for Monte Carlo simulation. It supports multiple option types, volatility models, and calculates important risk metrics (Greeks) for portfolio management.

## Volatility Models

1. **Constant Volatility (Black-Scholes)**
   - Traditional model with fixed volatility
   - Suitable for short-term options
   - Simple but may not capture volatility smile/skew

2. **Heston Stochastic Volatility**
   - Mean-reverting stochastic volatility
   - Captures volatility clustering and smile/skew
   - Parameters:
     * κ (kappa): Mean reversion rate
     * θ (theta): Long-term variance
     * σ (sigma): Volatility of variance
     * ρ (rho): Price-volatility correlation
     * v₀: Initial variance

## Option Types Supported

1. **European Options**
   - Call: Right to buy at strike price
   - Put: Right to sell at strike price
   - Standard Black-Scholes model assumptions

2. **Binary Options**
   - Call: Fixed payout if price above strike
   - Put: Fixed payout if price below strike
   - Digital/cash-or-nothing payoff structure

3. **Asian Options**
   - Call and Put variants
   - Based on average price over time
   - Reduces volatility impact
   - Path-dependent pricing

4. **Lookback Options**
   - Call: Based on maximum price during period
   - Put: Based on minimum price during period
   - Path-dependent pricing
   - Extreme value consideration

## Implementation Details

### Core Components

1. **Random Number Generation**
   - Quantum random number generation via quantum_rng library
   - Box-Muller transform for normal distribution
   - Correlated random numbers for Heston model
   - High-quality randomness for accurate simulations

2. **Price Path Simulation**
   - Geometric Brownian Motion for constant volatility
   - Heston SDE for stochastic volatility
   - Configurable time steps
   - Accounts for:
     * Risk-free rate
     * Dividend yield
     * Volatility dynamics
     * Time to maturity

3. **Greeks Calculation**
   - Delta: Price sensitivity to underlying
   - Gamma: Delta sensitivity to underlying
   - Theta: Time decay
   - Vega: Volatility sensitivity
   - Rho: Interest rate sensitivity
   - Calculated using finite differences

### Statistical Analysis

1. **Price Estimation**
   - Mean option price
   - Standard error
   - Confidence intervals

2. **Path Analysis**
   - Optional price path storage
   - Path statistics for exotic options
   - Extreme value tracking

### Configuration Options

1. **Simulation Parameters**
   - Number of paths (1,000 to 10,000,000)
   - Time steps
   - Random seed

2. **Option Parameters**
   - Spot price
   - Strike price
   - Time to maturity
   - Volatility (constant model)
   - Risk-free rate
   - Dividend yield

3. **Heston Parameters**
   - Mean reversion rate (κ)
   - Long-term variance (θ)
   - Volatility of variance (σ)
   - Correlation (ρ)
   - Initial variance (v₀)

4. **Output Controls**
   - Progress display
   - Greeks calculation
   - Path storage
   - Output format

## Mathematical Models

### Constant Volatility Model
```
dS = (r - q)Sdt + σSdW
```
Where:
- S: Asset price
- r: Risk-free rate
- q: Dividend yield
- σ: Volatility
- W: Wiener process

### Heston Model
```
dS = (r - q)Sdt + √vSdW₁
dv = κ(θ - v)dt + σ√vdW₂
⟨dW₁,dW₂⟩ = ρdt
```
Where:
- v: Instantaneous variance
- κ: Mean reversion rate
- θ: Long-term variance
- σ: Volatility of variance
- ρ: Correlation between price and variance

### Greeks Calculation
- Delta (Δ): ∂V/∂S
- Gamma (Γ): ∂²V/∂S²
- Theta (Θ): -∂V/∂t
- Vega (ν): ∂V/∂σ
- Rho (ρ): ∂V/∂r

Implemented using finite differences with small increments.

## Usage Examples

### Basic European Call Option (Constant Volatility)
```bash
./options_pricing -S 100 -K 100 -v 0.2 -r 0.05 -T 1.0
```

### European Call with Heston Model
```bash
./options_pricing -S 100 -K 100 -M heston --heston-kappa 2.0 --heston-theta 0.04 \
                 --heston-sigma 0.3 --heston-rho -0.7 --heston-v0 0.04
```

### Asian Option with Path Storage
```bash
./options_pricing -y asian_call -n 100000 -p -o asian_paths.csv -c
```

### Detailed Greeks Analysis
```bash
./options_pricing -S 100 -K 95 -V -G dgtvr -n 100000
```

### Binary Option JSON Output
```bash
./options_pricing -y binary_call -j -n 100000 -o binary_results.json
```

## Validation Approaches

1. **European Options**
   - Compare against Black-Scholes analytical solution
   - Verify put-call parity
   - Check convergence with increased paths

2. **Exotic Options**
   - Compare with known bounds
   - Verify monotonicity properties
   - Test limiting cases

3. **Greeks**
   - Verify symmetry relationships
   - Check boundary conditions
   - Compare with analytical solutions where available

4. **Heston Model**
   - Compare with semi-analytical solutions
   - Verify volatility surface properties
   - Test mean-reversion behavior

## Performance Characteristics

### Memory Usage
- Linear with number of paths when storing paths
- Constant without path storage
- Efficient Greeks calculation
- Additional state for Heston model

### Processing Speed
- Linear scaling with paths and time steps
- Parallel computation potential
- Progress monitoring overhead minimal
- Heston model ~2x slower than constant volatility

## Conclusions

The options pricing implementation provides a robust and flexible tool for financial derivatives analysis. Key strengths include:

1. **Comprehensiveness**: Supports multiple option types and Greeks
2. **Accuracy**: Uses quantum random numbers and validated models
3. **Flexibility**: Configurable parameters and outputs
4. **Sophistication**: Advanced volatility modeling with Heston
5. **Usability**: Clear interface and progress monitoring
6. **Extensibility**: Modular design for adding new models

Particularly suitable for:
- Option pricing and valuation
- Risk management
- Trading strategy analysis
- Academic research
- Volatility surface modeling

Future enhancements could include:
- Additional stochastic volatility models (SABR, etc.)
- Jump diffusion processes
- Multi-asset options
- GPU acceleration
- Real-time market data integration
- American option early exercise
