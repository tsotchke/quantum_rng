# Finance examples

These programs use the quantum RNG as the source of randomness for classic
computational-finance methods: Monte Carlo simulation of asset prices, option
pricing (closed-form and simulated), stochastic-volatility modelling, and
portfolio optimisation. In plain terms: they roll a very high-quality set of
dice many times to estimate what a stock, an option, or a whole portfolio might
be worth in the future, and how risky it is.

Every program here draws its randomness from the lower-level `qrng_*` API
(`qrng_init`, `qrng_double`, `qrng_uint64`, `qrng_free`) rather than the
higher-level `secure_rng` interface. `qrng_double` returns a uniform value in
`[0, 1)`; the finance code turns those uniforms into the Gaussian shocks that
the models need via the **Box-Muller transform** (see below). The RNG core is a
real state-vector quantum simulation whose statistical quality is verified
elsewhere in the toolkit with a CHSH Bell test (classical bound 2, Tsirelson
bound 2√2 ≈ 2.828); these examples consume its output, they do not re-run the
Bell test.

These are teaching tools that show the mathematics end to end. They are honest,
correct implementations of the models, but they are not a production trading or
risk system: path counts are modest, there is no market-data plumbing, and (as
noted per program) some results carry Monte Carlo sampling noise by design.

## The Box-Muller transform (used everywhere here)

Geometric Brownian motion and the other models need standard normal `N(0,1)`
shocks, but the RNG produces uniforms. Box-Muller converts two independent
uniforms `u1, u2 ∈ (0,1)` into two independent normals:

```
r  = sqrt(-2 ln u1)
z0 = r cos(2π u2)
z1 = r sin(2π u2)
```

The code draws `u1` in a reject-zero loop (or uses `1 - qrng_double`) so `ln`
never sees 0, returns `z0`, and caches `z1` for the next call, so on average one
uniform is consumed per normal. A raw uniform would bias the drift, which is why
this step matters.

## `finance/` vs `finance/dev/`

`finance/` is the canonical, built source tree. `finance/dev/` contains a
byte-identical snapshot of the options-pricing family only
(`options_pricing.[ch]`, `heston_model.[ch]`, `options_pricing_demo.c`,
`options_pricing_test.c`). Nothing in the Makefile references `dev/`; it is a
legacy duplicate. Read and build from `finance/`.

## Building and running

The quantum RNG library builds from the repository root:

```
make                    # builds libquantumrng.so, libsecure_qrng.so, the CLIs
```

Then build the finance programs by target name (each rule links the example
object files against the library):

```
make monte_carlo_test
make options_pricing_test
make options_pricing_demo
make quantum_portfolio
make examples_all        # builds every example across all domains at once
```

On macOS the library and examples are built with OpenMP via Homebrew `libomp`,
so binaries need that library (and, for the options-pricing family, the shared
`libquantumrng.so` in the repo root) on the loader path at runtime:

```
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib:. ./quantum_portfolio -n 4 -s 1000 -P
```

On Linux use `LD_LIBRARY_PATH=.` instead. The `quantum_portfolio` and the ML
binaries link the RNG objects statically, so they strictly only need `libomp`;
the options-pricing binaries additionally need `.` on the path because they link
the shared `libquantumrng.so`. Using the combined path above works for all of
them.

Note: there is a Makefile target named `options_pricing`, but it does **not**
build — `options_pricing.c` is a library with no `main()`, so linking that
target fails with an undefined `_main`. It is not part of `examples_all`. The
runnable option programs are `options_pricing_demo` and `options_pricing_test`.
There is likewise no `monte_carlo` binary; the runnable Monte Carlo program is
`monte_carlo_test`.

---

## `monte_carlo.c` / `monte_carlo.h` (library, no `main`)

**What it does.** Estimates where a stock might end up after a year by
simulating many random price paths and averaging the outcome. This is the
Monte Carlo engine for **Geometric Brownian Motion (GBM)**; it has no `main()`
and is linked into `monte_carlo_test`.

**The math.** Each path steps the price with the exact log-Euler GBM update

```
S_{t+1} = S_t · exp( (r - q - ½σ²)·dt + σ·√dt · z ),   z ~ N(0,1)
```

where `r` is the risk-free rate, `q` the dividend yield, `σ` the volatility, and
`dt = 1 / trading_days`. The `-½σ²` term is the Ito correction that keeps the
discounted price a martingale. The normal shock `z` comes from Box-Muller. After
running all paths it reports the mean terminal price, standard deviation, min,
max, and a confidence interval `mean ± z·σ/√N`.

**API details worth knowing.** `config->confidence_level` stores the *z-score*
(1.96 for 95%, 2.576 for 99%), not a percentage; the print routines convert it
to a nominal percentage with `erf(z/√2)`. `run_simulation` passes a `NULL` seed
pointer when no seed is set, because `qrng_init` indexes a non-NULL seed with
`i % seed_len` and would divide by zero at `seed_len == 0`.

**Configuration.** Defaults: 100,000 simulations, 252 trading days, S₀ = 100,
σ = 20%, r = 5%, q = 2%. Bounds: 1,000 to 10,000,000 simulations. The file
includes an argument parser `parse_simulation_args` accepting `-n` (sims),
`-d` (days), `-p` (price), `-v` (vol), `-r` (rate), `-y` (dividend),
`-o json|csv` (output mode), `-s` (seed), `-f` (output file) — but note **no
program wires this parser to a `main`**, so those flags are not exposed by any
built binary. They are exercised only by the unit test.

**Caveat.** Because the RNG mixes in hardware entropy on every draw, runs are
statistically equivalent but not bit-for-bit reproducible even with a fixed
seed.

## `monte_carlo_test.c` (has `main`)

**What it does.** The runnable test harness for the Monte Carlo engine — the
thing you actually execute to see the simulation work and self-check. Takes no
arguments.

```
make monte_carlo_test
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib:. ./monte_carlo_test
```

**What it demonstrates / checks.** Six tests: config initialisation, argument
parsing, a 10,000-path simulation whose mean terminal price is asserted within
10% of the theoretical `S₀·exp((r−q)·T)`, JSON and CSV output, error handling
(rejecting out-of-range simulation counts, zero trading days, negative prices),
and a rough throughput measurement. It prints the measured mean against the
theoretical value so you can see convergence. The test uses a reduced 10,000
simulations because the quantum RNG produces on the order of a few hundred
thousand doubles per second.

## `options_pricing.c` / `options_pricing.h` (library, no `main`)

**What it does.** Prices options — contracts whose payoff depends on where a
stock ends up. It provides both the exact textbook formula and a Monte Carlo
estimator, plus the risk sensitivities ("Greeks"). No `main()`; linked into
`options_pricing_demo` and `options_pricing_test`, and it dispatches to the
Heston engine when stochastic volatility is selected.

**The math it implements.**

- **Black-Scholes closed form** (`black_scholes_price`) for European calls and
  puts with continuous dividend yield:
  `d1 = [ln(S/K) + (r − q + ½σ²)T] / (σ√T)`, `d2 = d1 − σ√T`, and
  `Call = S e^{−qT} N(d1) − K e^{−rT} N(d2)` (put by symmetry), where `N` is the
  normal CDF built from `erf`.
- **Closed-form Greeks** (`black_scholes_greeks`): delta, gamma, vega, theta,
  rho for European calls/puts, derived analytically from the same `d1, d2`.
- **Monte Carlo path pricing** (`run_pricing_simulation`): simulates GBM paths
  (same log-Euler update as the Monte Carlo engine), discounts each payoff by
  `e^{−rT}`, and reports the mean price, standard error `σ/√N`, and a 95%
  interval.
- **Exotic payoffs** (`path_payoff`): **Asian** calls/puts use the arithmetic
  average of the path; **lookback** calls/puts use the path maximum/minimum
  against a fixed strike; **binary** (cash-or-nothing) pay 1 or 0 on the
  terminal price. Vanilla calls/puts use the terminal price only.

**How the Greeks are computed.** For a European call or put under **constant**
volatility the code returns the **closed-form Black-Scholes Greeks** — the exact
analytic values, with no simulation noise. Only for exotic payoffs or the Heston
model does it use Monte Carlo **central-difference** Greeks (bump-and-reprice
with `0.2×` the path count per estimate). Pass a seed with `-s` to make those
Monte Carlo estimates reproducible.

**Defaults.** 10,000 paths (bounds 1,000–1,000,000), 252 time steps, S = K = 100,
T = 1 year, σ = 20%, r = 5%, q = 2%, European call. Heston defaults: κ = 2.0,
θ = 0.04, σ_v = 0.3, ρ = −0.7, v₀ = 0.04.

## `options_pricing_demo.c` (has `main`)

**What it does.** A one-shot demonstration that prices a standard at-the-money
European call and prints all five Greeks. Configuration is hardcoded (no
arguments); it seeds the RNG from the current time.

```
make options_pricing_demo
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib:. ./options_pricing_demo
```

**What you see.** With S = K = 100, σ = 20%, r = 5%, q = 2%, T = 1, 10,000
paths, a representative run prints an option price near 9.5 with a standard error
around 0.14 and a 95% interval, followed by the closed-form Greeks (delta ≈ 0.59,
gamma ≈ 0.019, vega ≈ 38, theta ≈ −5.1, rho ≈ 49). Exact numbers vary run to run
because of the entropy-mixed RNG.

## `options_pricing_test.c` (has `main`)

**What it does.** The runnable validation suite for the pricing library. No
arguments.

```
make options_pricing_test
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib:. ./options_pricing_test
```

**What it checks.** Monte Carlo call/put prices against the Black-Scholes closed
form (asserted within five standard errors, a ~1e-6 false-failure rate);
**put-call parity** `C − P = S − Ke^{−rT}` both exactly for the closed form and
within combined MC error; a Heston simulation producing sane bounded prices; the
closed-form Greeks satisfying parity relations (`Δ_call − Δ_put = e^{−qT}`, equal
gamma and vega) and sign/range bounds; a constant-vol-vs-Heston comparison with
matched long-run variance agreeing within 10%; error handling for invalid inputs;
and a throughput measurement.

## `heston_model.c` / `heston_model.h` (library, no `main`)

**What it does.** Adds *random, mean-reverting volatility* to the price
simulation — real markets do not have constant volatility, and this captures the
"volatility clusters and smiles" that constant-vol Black-Scholes misses. No
`main()`; used by the options-pricing library and its test/demo.

**The math.** The **Heston** model evolves the stock and its instantaneous
variance together:

```
dS = (r − q) S dt + √v · S dW_s
dv = κ(θ − v) dt + σ_v √v dW_v,     corr(dW_s, dW_v) = ρ
```

The variance follows a **CIR (Cox-Ingersoll-Ross) square-root process**: κ is the
mean-reversion speed pulling variance toward the long-run level θ, σ_v is the
volatility of variance, and ρ correlates the price and variance shocks. Per step
the code draws two independent Box-Muller normals `z1, z2`, correlates them
(`z_s = z1`, `z_v = ρ z1 + √(1−ρ²) z2`), advances the variance with an Euler step
using a **full-truncation scheme** (negative variance excursions are clamped to
zero), and steps the stock in log space with the `−½v·dt` Ito correction so the
discounted price stays a martingale. The Heston path pricer prices vanilla and
binary payoffs on the terminal price and reports price, standard error, and a
95% interval.

**Caveat.** This is the standard Euler-with-truncation discretisation, chosen for
clarity. It is biased for very coarse time steps or when the Feller condition
(`2κθ ≥ σ_v²`) is violated; it is not the higher-accuracy QE or exact scheme a
production desk would use.

## `quantum_portfolio.c` / `quantum_portfolio.h` (has `main`, full CLI)

**What it does.** Chooses how to split money across several stocks to get the
best risk-adjusted return, then stress-tests that allocation against thousands of
correlated random market scenarios. This is the only finance program with a real
command-line interface.

```
make quantum_portfolio
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib:. ./quantum_portfolio --help
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib:. ./quantum_portfolio -n 4 -s 1000 -P
```

**The math, in two stages.**

1. **Quantum-enhanced genetic algorithm (GA)** over the weight simplex. A
   population of 100 candidate weight vectors evolves for up to 1,000 generations
   using tournament selection, blend (arithmetic) crossover with a
   quantum-random blend factor, mutation (rate 0.1), elitism (the best-so-far is
   carried forward), and renormalisation back onto the simplex. Every random
   choice — initial weights, tournament picks, crossover blend, mutation — is
   driven by the quantum RNG. In the default (Sharpe) mode the fitness is the
   **Sharpe ratio** `(E[return] − r_f)/σ_portfolio`, where portfolio variance is
   the full `wᵀ Σ w` quadratic form using each asset's volatility and the
   correlation matrix. In target mode (used for the efficient frontier) the
   fitness minimises volatility subject to a soft penalty for missing a target
   return.

2. **Cholesky-correlated Monte Carlo** of the optimised portfolio. The asset
   correlation matrix is **Cholesky-factorised** (`Σ = L Lᵀ`, in place, with a
   positive-definite check that falls back to independent shocks and a warning if
   the matrix is not PD). Each simulated day draws independent Box-Muller normals
   `z`, correlates them via `ε = L z`, and accumulates each asset's daily return
   `μ·dt + σ·√dt·ε`. From the distribution of simulated horizon returns it
   computes **95% Value at Risk (VaR)** (the 5th-percentile loss) and **95%
   Conditional VaR / Expected Shortfall (CVaR)** (the mean loss beyond VaR), plus
   the mean maximum drawdown across paths.

**Efficient frontier & rebalancing.** With `-E` it sweeps 100 target returns and
runs the GA in target mode at each to trace the risk/return frontier. With `-B`
it prints the suggested trades from the current equal-weight allocation to the
optimised weights.

**Options.** `-n/--assets`, `-s/--simulations` (1,000–10,000,000, default 2,000),
`-t/--horizon` (days, default 252), `-r/--rate`, `-R/--target`, `-T/--tolerance`,
`-S/--seed`, `-q/--quiet`, `-v/--verbose`, `-j/--json`, `-c/--csv`,
`-P/--no-progress`, `-E/--frontier`, `-B/--rebalance`, `-o/--output FILE`,
`-h/--help`. The built-in universe is ten sample tickers with fixed expected
returns/volatilities and a single-factor 0.3 correlation structure; asking for
more than ten assets cycles through the sample data.

**Caveats.** Expected returns and volatilities are illustrative constants, not
estimated from market data. The default is a small 2,000 simulations because each
simulation consumes `horizon × assets` normal draws from the (deliberately
un-optimised) quantum RNG; larger runs are correct but slow.

---

## `options_pricing_cli.c` -> `options_pricing` (command-line pricer)

**In one sentence:** price an option from the command line, choosing the
underlying, the option type, the volatility model, and which risk sensitivities
(Greeks) to report.

**What it does.** A full command-line front end over the pricing engine. For a
European call or put under constant volatility it prints both the analytic
Black-Scholes price and the Monte Carlo estimate (and their difference), so you
can see the simulation converge on the exact answer. Exotic payoffs (Asian,
lookback, binary) and the Heston stochastic-volatility model are priced by Monte
Carlo. Greeks for European options are the closed-form Black-Scholes values;
for exotics and Heston they are Monte Carlo finite differences.

**Build/run:** `make options_pricing`, then e.g.
`./options_pricing -y call -S 100 -K 100 -T 1 -v 0.2 -r 0.05 -G all`.

Options: `-S` spot, `-K` strike, `-T` maturity (years), `-v` volatility, `-r`
risk-free rate, `-q` dividend yield, `-y` type (`call`, `put`, `binary_call`,
`binary_put`, `asian_call`, `asian_put`, `lookback_call`, `lookback_put`), `-M`
model (`bs` or `heston`), `--heston-kappa/--heston-theta/--heston-sigma/--heston-rho/--heston-v0`,
`-n` Monte Carlo paths, `-G` Greeks (`d,g,t,v,r`, `all`, or `none`), `-o` output
(`normal`, `quiet`, `verbose`, `json`, `csv`), `-p` show paths, `-s` seed, `-h`.
Verified: an at-the-money call priced 10.45 by Monte Carlo against a 10.45
analytic Black-Scholes value.

## `monte_carlo_cli.c` -> `monte_carlo` (command-line simulator)

**In one sentence:** run a geometric-Brownian-motion price simulation from the
command line and report the distribution of outcomes.

**What it does.** Simulates terminal asset prices under risk-neutral geometric
Brownian motion (using the Box-Muller Gaussian transform described above) and
reports the mean, standard deviation, minimum, maximum, and a 95% confidence
interval. In normal mode it also prints the theoretical mean, `S0 * e^((r-q)T)`,
so you can watch the simulated mean converge toward it as the path count grows.

**Build/run:** `make monte_carlo`, then e.g. `./monte_carlo -n 100000 -p 100 -v 0.2 -r 0.05`.

Options: `-n` simulations, `-d` trading days per year, `-p` initial price, `-v`
volatility, `-r` risk-free rate, `-y` dividend yield, `-o` output (`normal`,
`json`, `csv`), `-s` seed, `-h`. With `-s`, the same seed reproduces the same
run exactly.

---

Part of the Quantum RNG examples. The full quantum-simulation toolkit and the
continued Bell-verified RNG live in Moonlab:
https://github.com/tsotchke/moonlab
