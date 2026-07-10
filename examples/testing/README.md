# Testing examples

These two programs stress and characterize the random number generator itself.
One hammers ordinary code with a flood of randomized inputs to shake out
crashes; the other puts the quantum RNG head-to-head with a classical
pseudo-random generator and runs the one test a classical generator can never
pass, the CHSH Bell test.

The two use different layers of the stack. `fuzz_test` uses the lower-level
`qrng_*` API directly (`qrng_init`, `qrng_uint64`) purely as a source of random
bytes. `quantum_vs_classical` is the one that exercises the recommended
production path: it uses `secure_rng` (hardware entropy + NIST SP 800-90B health
tests + quantum mixing; modes FAST / QUANTUM / HYBRID / VERIFIED) and calls the
real Bell-test machinery (`quantum_state`, `quantum_gates`, `bell_test`) on
genuine 8-qubit state-vector entanglement.

## Building and running

Build the library once from the repo root:

```
make
```

Then build either example, or all of them:

```
make fuzz_test
make quantum_vs_classical
make examples_all
```

Binaries land in the repo root; run them from there. The examples link the
library, which is compiled with OpenMP, so on macOS expose libomp at runtime:

```
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib ./fuzz_test
```

---

## fuzz_test

**In one sentence:** it uses quantum randomness to generate thousands of weird
inputs and mutations, throws them at some sample functions, and checks that
nothing crashes and every result is still correct.

**What it does (technical).** Combines property-based testing with mutation
fuzzing, both driven by the quantum RNG. It generates random strings and integer
arrays, then:

- **Property tests** verify invariants on clean inputs: string reverse is its
  own inverse, bubble sort produces a non-decreasing array, and the average of
  an array matches an independent recomputation.
- **Fuzz tests** apply random mutations (character/value change, bit flip,
  boundary-value injection such as `INT_MAX`/`INT_MIN`, adjacent-value copy) and
  confirm the operations neither crash nor return NaN/Inf.

It runs 1000 iterations, with 10 mutations per iteration on the fuzz side, and
prints failure and crash counts per category.

**What it teaches.** How a good entropy source powers both property-based testing
(random valid inputs to check invariants) and mutation fuzzing (adversarial
inputs to check robustness), and why boundary values deserve special attention.
A verification run reported 0 failures across all property tests and 0 crashes
across all fuzz tests, i.e. the sample functions held up and the RNG produced a
clean, varied input stream.

**How to run.** Has `main()` and takes no flags. Run `./fuzz_test`; it prints the
property-test results followed by the fuzz-test results. Seed is fixed
(`"fuzztest"`), so the run is reproducible.

**Notes/caveats.** The functions under test (`string_reverse`, `array_sort`,
`calculate_average`) are small in-file examples chosen to demonstrate the
harness, not the library's own API surface. This is a teaching example for the
fuzzing/property-testing pattern, not a full test suite.

---

## quantum_vs_classical

**In one sentence:** it runs the same battery of tests on a classical
random-number generator and on the quantum one, and shows the single decisive
difference, the quantum generator can pass a physics test (the Bell test) that
no classical generator can.

**What it does (technical).** Side by side, it pits a self-contained
Mersenne-Twister PRNG against the `secure_rng` quantum generator across several
tests. The centerpiece is **Test 1, the CHSH Bell inequality**:

- **Classical side:** a genuine local-hidden-variable (LHV) model. Two parties
  share a random hidden variable and each decides a +/-1 outcome by a *local*
  deterministic rule (Malus-law thresholding) with modeled detector
  imperfection. This is exactly the class of models Bell's theorem constrains,
  so it *must* satisfy |CHSH| <= 2. Nothing is hard-coded; the four correlations
  are simulated at the optimal measurement angles and combined.
- **Quantum side:** a maximally entangled Bell state (Phi+) is created on a real
  2-qubit state vector, and `bell_test_chsh` measures the four CHSH correlations
  using secure-RNG-backed measurement sampling.

**Why this is the significant test.** Shannon entropy, autocorrelation, and
throughput (Tests 3-6) do *not* separate a good PRNG from a QRNG; the program
says so explicitly, both reach ~8 bits/byte. The Bell test is different. The
classical bound |CHSH| <= 2 is a theorem about *every possible* local-realistic
process, not a property of one algorithm, so a classical generator crossing it
is a mathematical impossibility, not merely unlikely. The quantum bound is
Tsirelson's 2*sqrt(2) ~= 2.828. A generator that measurably exceeds 2 is
therefore demonstrating genuine quantum entanglement, and that is what makes
quantum randomness *certifiable*, verifiable by physics rather than by trusting
the vendor.

A verification run showed the split clearly: the classical LHV model scored
CHSH ~= 1.56 (comfortably below 2, i.e. behaving classically as it must), while
the quantum Bell state scored CHSH ~= 2.83, violating the classical bound.

The remaining tests round out the comparison: predictability (two identically
seeded PRNGs produce identical streams; two secure-RNG instances diverge),
lag-1 autocorrelation, Shannon entropy, an explicit certification-capability
argument, and a FAST-vs-QUANTUM-mode throughput trade-off.

**How to run.** Has `main(void)` and takes no flags, but it is **interactive**:
it waits for you to press Enter between tests (`getchar`). Run it directly:

```
./quantum_vs_classical
```

To run it non-interactively, pipe newlines in:

```
printf '\n\n\n\n\n\n\n' | ./quantum_vs_classical
```

**Notes/caveats.** CHSH is a statistical estimate from a finite sample (5000
measurements per side here), so the quantum value fluctuates run to run around
Tsirelson's 2.828 and a single run can land slightly above or below it; the
meaningful, repeatable fact is that it clears the classical bound of 2 while the
LHV model never does. The classical LHV score sits well under 2 partly because
the modeled detector imperfection lowers its correlations. The summary table
prints representative figures (e.g. throughput and entropy) for orientation;
treat the live measured numbers, not the table, as the actual result of your
run.

---

Part of the Quantum RNG examples. The full quantum-simulation toolkit and the
continued Bell-verified RNG live in Moonlab:
https://github.com/tsotchke/moonlab
