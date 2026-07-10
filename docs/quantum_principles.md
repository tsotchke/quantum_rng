# Quantum Principles, As Implemented

This document explains the quantum mechanics that the v3 engine actually
computes. Every concept below maps to a specific function and file in
`src/quantum_rng/`. Nothing here is metaphor: the code maintains a full
complex state vector, applies genuine unitary gate matrices to it, and samples
outcomes according to the Born rule. The verification centerpiece is a real
CHSH Bell-inequality test (`bell_test.c`) whose measured value lands in the
quantum regime, above the classical bound of 2 and near the Tsirelson bound of
2√2 ≈ 2.828.

## An honest starting point

A deterministic classical computer cannot *create* physical quantum
randomness. What this project does is precise and worth stating plainly:

1. It performs an **exact numerical simulation** of a small quantum system —
   up to 32 qubits in principle, 8 qubits (a 256-dimensional state space) by
   default. The linear algebra is exact to double-precision floating point;
   there is no approximation of the quantum evolution itself.
2. The only randomness in the system is **physical entropy drawn from
   hardware** (see `src/entropy/hardware_entropy.c`). That entropy is what
   decides each simulated measurement outcome via the Born rule. The quantum
   simulation shapes a probability distribution; the hardware entropy draws
   from it.

So the "quantumness" is twofold and both halves are real: the *statistics* are
exactly those of a quantum system (provably so — the Bell test confirms
non-classical correlations that no local hidden-variable model can reproduce),
and the *unpredictability* comes from a true hardware entropy source, not from
a pseudo-random seed. The simulation is deterministic given its entropy stream;
the entropy stream is not.

## 1. Qubits and the state vector

A quantum state of `n` qubits is a unit vector in a 2ⁿ-dimensional complex
Hilbert space,

```
|ψ⟩ = Σ_i α_i |i⟩ ,   α_i ∈ ℂ ,   Σ_i |α_i|² = 1
```

where the basis kets `|i⟩` are the computational basis states indexed by the
integer `i` (its binary expansion gives the individual qubit values).

In code this is `quantum_state_t` (`quantum_state.h`):

```c
typedef struct {
    size_t     num_qubits;     // n
    size_t     state_dim;      // 2^n
    complex_t *amplitudes;     // the vector of α_i (C99 double _Complex)
    ...
} quantum_state_t;
```

`quantum_state_init()` (`quantum_state.c`) allocates the `state_dim = 1 << n`
complex amplitudes and initializes the system to `|0…0⟩` by setting
`amplitudes[0] = 1`. `MAX_QUBITS` is 32; `RECOMMENDED_MAX_QUBITS` is 28. The
RNG runs with 8 qubits by default.

The Born rule — the probability of observing basis state `|i⟩` is `|α_i|²` — is
implemented directly by `quantum_state_get_probability()`:

```c
double prob = cabs(state->amplitudes[i]);
return prob * prob;                 // |α_i|²
```

Normalization (`Σ|α_i|² = 1`) is checked by `quantum_state_is_normalized()` and
restored by `quantum_state_normalize()`. Diagnostic quantities are computed
exactly from the amplitudes: Shannon entropy of the measurement distribution
(`quantum_state_entropy()`), purity `Tr(ρ²)` (`quantum_state_purity()`, which is
1.0 for the pure states this engine maintains), and state fidelity
`F = |⟨ψ|φ⟩|²` (`quantum_state_fidelity()`).

## 2. Unitary gates — the actual matrices

Quantum evolution is multiplication of the state vector by unitary matrices.
Each gate in `quantum_gates.c` applies its matrix to the amplitude pairs that
differ only in the target qubit's bit. The code uses *stride-based* iteration:
for target qubit `q`, `stride = 1 << q`, and it walks the vector in blocks of
`2·stride`, transforming the pair `(amplitudes[base+i], amplitudes[base+i+stride])`.
This is exactly the tensor product `I ⊗ … ⊗ U ⊗ … ⊗ I` — the gate acts on one
qubit and as the identity on all others — evaluated without ever materializing
the full 2ⁿ × 2ⁿ matrix.

### Single-qubit gates

The Pauli gates (`gate_pauli_x`, `gate_pauli_y`, `gate_pauli_z`):

```
X = [ 0  1 ]      Y = [ 0  -i ]      Z = [ 1   0 ]
    [ 1  0 ]          [ i   0 ]          [ 0  -1 ]
```

`gate_pauli_x` swaps the `|0⟩` and `|1⟩` amplitudes; `gate_pauli_y` maps
`|0⟩ → i|1⟩`, `|1⟩ → −i|0⟩`; `gate_pauli_z` negates the `|1⟩` amplitudes.

The **Hadamard** gate (`gate_hadamard`) is the workhorse that creates
superposition:

```
H = (1/√2) [ 1   1 ]        H|0⟩ = (|0⟩+|1⟩)/√2
           [ 1  -1 ]        H|1⟩ = (|0⟩−|1⟩)/√2
```

implemented as `a0' = (a0 + a1)/√2`, `a1' = (a0 − a1)/√2` (the constant
`QC_SQRT2_INV` comes from `quantum_constants.h`).

Phase gates apply a relative phase to the `|1⟩` component:

```
S = [ 1  0 ]    T = [ 1     0    ]    P(θ) = [ 1    0   ]
    [ 0  i ]        [ 0  e^{iπ/4} ]           [ 0  e^{iθ} ]
```

These are `gate_s` / `gate_s_dagger` (multiply the `|1⟩` amplitudes by ±i),
`gate_t` / `gate_t_dagger` (multiply by `e^{±iπ/4}` = `cexp(±I·π/4)`), and the
general `gate_phase(θ)` (`cexp(I·θ)`).

The rotation gates are the Lie-group exponentials of the Paulis:

```
R_x(θ) = cos(θ/2) I − i sin(θ/2) X
R_y(θ) = cos(θ/2) I − i sin(θ/2) Y
R_z(θ) = diag( e^{−iθ/2}, e^{+iθ/2} )
```

implemented by `gate_rx`, `gate_ry`, `gate_rz`. The most general single-qubit
unitary is `gate_u3(θ,φ,λ)`:

```
U3(θ,φ,λ) = [ cos(θ/2)              −e^{iλ} sin(θ/2)       ]
            [ e^{iφ} sin(θ/2)        e^{i(φ+λ)} cos(θ/2)   ]
```

### Two- and three-qubit gates

**CNOT** (`gate_cnot`) is the entangling gate. It flips the target iff the
control is `|1⟩`:

```
CNOT = [ 1 0 0 0 ]
       [ 0 1 0 0 ]
       [ 0 0 0 1 ]
       [ 0 0 1 0 ]
```

Also provided: controlled-Z (`gate_cz`), controlled-Y (`gate_cy`), SWAP
(`gate_swap`), controlled-phase (`gate_cphase`), the controlled rotations
(`gate_crx`, `gate_cry`, `gate_crz`), the three-qubit Toffoli / CCNOT
(`gate_toffoli`) and Fredkin / CSWAP (`gate_fredkin`), the multi-controlled
`gate_mcx` / `gate_mcz`, and the Quantum Fourier Transform and its inverse
(`gate_qft` / `gate_iqft`). Arbitrary 2×2 and 4×4 unitaries can be applied
directly via `apply_single_qubit_gate` and `apply_two_qubit_gate`.

Every one of these preserves the norm of the state vector to floating-point
precision — a property the test suite checks with `verify_gate_normalization`.

## 3. Superposition and entanglement

**Superposition** is what a Hadamard produces: after `gate_hadamard(state, q)`
on a fresh `|0⟩`, qubit `q` is in `(|0⟩ + |1⟩)/√2`, an equal-weight combination
whose two outcomes each have Born probability 1/2.

**Entanglement** is created by following a Hadamard with a CNOT. This is exactly
how the four Bell states are built in `bell_test.c`:

```
|Φ⁺⟩ = (|00⟩ + |11⟩)/√2   ← create_bell_state_phi_plus:  H(q1); CNOT(q1,q2)
|Φ⁻⟩ = (|00⟩ − |11⟩)/√2   ← create_bell_state_phi_minus
|Ψ⁺⟩ = (|01⟩ + |10⟩)/√2   ← create_bell_state_psi_plus
|Ψ⁻⟩ = (|01⟩ − |10⟩)/√2   ← create_bell_state_psi_minus
```

In `|Φ⁺⟩` the two qubits are perfectly correlated but individually random: no
product of single-qubit states can reproduce it. The engine can *quantify* the
entanglement: `quantum_state_partial_trace()` computes the reduced density
matrix `ρ_A = Tr_B(|ψ⟩⟨ψ|)` of a subsystem, and
`quantum_state_entanglement_entropy()` diagonalizes it and returns the von
Neumann entropy `S(ρ_A) = −Σ λ_i log₂ λ_i` (via `hermitian_eigen_decomposition`
in `matrix_math.c`). For a Bell state this returns 1 bit — one full ebit of
entanglement — which is the maximum for two qubits.

## 4. Measurement and wavefunction collapse

Measurement is the one place physical randomness enters. `quantum_measure()`
(`quantum_gates.c`) measures a single qubit:

1. Optionally rotate into the requested basis (`MEASURE_HADAMARD` applies an H
   to measure in the X basis; `MEASURE_CIRCULAR` applies `S†` then `H` for the
   Y basis; `MEASURE_COMPUTATIONAL` is the Z basis, no rotation).
2. Compute `prob_0` and `prob_1` by summing `|α_i|²` over the basis states with
   that qubit equal to 0 and 1 respectively.
3. **Draw a uniform random double** `r ∈ [0,1)` and set the outcome to 0 if
   `r < prob_0`, else 1. This draw is the crux:

   ```c
   double random;
   if (quantum_entropy_get_double(entropy, &random) != 0) return result;
   result.outcome = (random < prob_0) ? 0 : 1;
   ```

   `quantum_entropy_get_double()` (`quantum_entropy.h`) pulls 8 bytes from a
   `quantum_entropy_ctx_t` whose backing source is the hardware entropy pool.
   The randomness is physical; the probabilities are quantum.
4. **Collapse**: zero out the amplitudes inconsistent with the outcome and
   renormalize what remains, so the post-measurement state is a valid
   projection — the standard von Neumann projective measurement.

`quantum_measure_all_fast()` measures every qubit at once by sampling a single
basis state `i` from the full distribution `{|α_i|²}` in one pass (one hardware
entropy draw), then collapsing to `|i⟩`. This is the hot path the RNG uses to
turn the quantum state into output bytes.

There is no `rand()` anywhere in this path. Using a predictable source here
would silently destroy the security of the generator, which is why the
measurement functions return a null result rather than proceed if the entropy
context is missing (`quantum_entropy.h` documents this as SECURITY CRITICAL).

## 5. The CHSH Bell inequality — the verification centerpiece

The Bell test is what makes the claim "this behaves quantum-mechanically"
falsifiable rather than rhetorical. Its logic (Clauser–Horne–Shimony–Holt,
1969) is: any theory in which the two qubits carry pre-existing local values
("local hidden variables") must satisfy

```
S = | E(a,b) − E(a,b′) + E(a′,b) + E(a′,b′) | ≤ 2
```

where `E(x,y)` is the correlation of the two ±1 measurement outcomes at
detector settings `x` and `y`. Quantum mechanics can violate this, up to the
**Tsirelson bound** `S ≤ 2√2 ≈ 2.828`. A measured `S > 2` cannot be explained
by any local classical model.

The implementation is in `bell_test.c`:

- **Correlation** — `measure_correlation()` clones the entangled state, rotates
  each qubit's measurement basis by its analyzer angle with `gate_ry(−angle)`,
  computes the joint probabilities `P(00), P(01), P(10), P(11)` once, then
  draws `num_samples` outcomes from that distribution using hardware entropy and
  averages the product of signs (`0 → +1`, `1 → −1`). This returns
  `E(a,b) = ⟨A(a)·B(b)⟩`. For the `|Φ⁺⟩` state, `E(θ_a, θ_b) = cos(θ_a − θ_b)`.
- **Optimal settings** — `bell_get_optimal_settings()` uses the canonical angles
  `a = 0`, `a′ = π/2`, `b = π/4`, `b′ = 3π/4`. With these,
  `S = |cos(−π/4) − cos(−3π/4) + cos(π/4) + cos(π/4)| = 4·(1/√2) = 2√2`,
  the maximal quantum value.
- **CHSH assembly** — `calculate_chsh_parameter()` combines the four
  correlations per the formula above.
- **Full test** — `bell_test_chsh()` builds `|Φ⁺⟩`, measures all four
  correlations, computes `S`, and does a proper statistical analysis: standard
  error `≈ 2/√N`, a z-score for the excess over the classical bound, and a
  one-tailed p-value via `erfc`. It reports `violates_classical` (`S > 2`),
  `confirms_quantum` (`S > 2.4`, a conservative threshold), and
  `statistically_significant` (`p < 0.01`).

Because the correlations are estimated by finite sampling, the measured `S`
fluctuates just below the ideal `2√2`; in practice this engine reports
**S ≈ 2.83–2.90** for the default configuration. (Small excursions slightly
above `2√2` are ordinary sampling noise on a finite run, not a violation of
Tsirelson's bound.) You can reproduce it with `make test_v3`, which prints the
CHSH value and correlations. `bell_test_monitor_t` and the `bell_monitor_*`
functions let a running generator sample the CHSH value continuously and track
its mean, min, max, and variance — this is the basis of the RNG's
`BELL_VERIFIED` mode.

## 6. Grover amplitude amplification

Grover's algorithm demonstrates genuine quantum amplitude manipulation and
underpins the RNG's Grover sampling mode. It lives in `grover.c`:

- `grover_oracle()` marks a target state by flipping the sign of its amplitude,
  `O|x⟩ = (−1)^{f(x)}|x⟩` — a phase oracle.
- `grover_diffusion()` implements the inversion-about-the-mean operator
  `D = 2|s⟩⟨s| − I` (where `|s⟩ = H^{⊗n}|0⟩` is the uniform superposition) as
  the sequence `H^{⊗n}` · (phase-flip everything but `|0…0⟩`) · `H^{⊗n}`.
- `grover_iteration()` composes `G = D·O`, and `grover_optimal_iterations()`
  returns `⌊(π/4)·√N⌋`, the count that maximizes the marked-state probability
  (`≈ 1 − 1/N`). `grover_search()` runs the full algorithm and measures.

The generalizations `grover_amplitude_amplification()` and
`grover_importance_sampling()` (following Brassard et al., 2000) reshape the
amplitude distribution toward an arbitrary target so the generator can sample
from non-uniform distributions with a quadratic advantage over rejection
sampling for peaked targets.

## 7. What the simulation does and does not claim

- **Exact, not approximate.** The state vector evolves under exact unitary
  matrices; probabilities are exact `|α|²`. There is no modeling of physical
  noise, decoherence, or gate error — the simulated qubits are ideal and the
  states are pure (`purity = 1`). If you want a noisy-hardware model, that is a
  different tool.
- **The randomness is physical, and it comes from hardware.** The Born-rule
  draws in `quantum_measure*` and `measure_correlation` are fed by
  `src/entropy/hardware_entropy.c` (RDSEED/RDRAND on x86, RNDR/RNDRRS on ARMv8.5,
  `getrandom()`, `/dev/random`, `/dev/urandom`, and CPU-jitter, in a
  quality-ordered fallback chain). A classical machine cannot manufacture the
  irreducible randomness of a real measurement; it can only exhibit the correct
  quantum *statistics* while sourcing its unpredictability from a genuine
  entropy source. This engine does exactly that, and the Bell test certifies the
  statistics are non-classical.
- **Reproducible only up to the entropy stream.** Given the same sequence of
  hardware entropy, the simulation is deterministic. In deployment the entropy
  is unpredictable, so the output is too.

## Continuation

This quantum-simulation, Bell-verified line of work continues in **Moonlab**
(<https://github.com/tsotchke/moonlab>), a full quantum-computing simulator.
Moonlab's feature set includes a Bell-verified quantum RNG that is a direct
descendant of the engine documented here, alongside a broader gate set, larger
circuits, and additional quantum algorithms.

## References

1. Nielsen, M. A., & Chuang, I. L. (2010). *Quantum Computation and Quantum
   Information* (10th Anniversary ed.). Cambridge University Press. — gate and
   measurement semantics, density matrices, entanglement entropy.
2. Clauser, J. F., Horne, M. A., Shimony, A., & Holt, R. A. (1969). Proposed
   Experiment to Test Local Hidden-Variable Theories. *Physical Review Letters*,
   23(15), 880–884. — the CHSH inequality.
3. Cirel'son (Tsirelson), B. S. (1980). Quantum generalizations of Bell's
   inequality. *Letters in Mathematical Physics*, 4(2), 93–100. — the 2√2 bound.
4. Brassard, G., Høyer, P., Mosca, M., & Tapp, A. (2000). Quantum Amplitude
   Amplification and Estimation. — the amplitude-amplification generalization of
   Grover search.
5. Turan, M. S., et al. (2018). *NIST SP 800-90B: Recommendation for the Entropy
   Sources Used for Random Bit Generation.* — health testing and conditioning of
   the hardware entropy that drives measurement.
