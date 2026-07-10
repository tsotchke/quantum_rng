# Science examples

These four programs use the quantum random number generator as a physics-grade
noise and sampling source inside classic scientific simulations: molecular
dynamics, quantum walks, colored-noise synthesis, and a toy weather model. Each
one shows that the RNG is not just "good enough for a demo" but stable and
statistically well-behaved enough to drive real numerical methods (a Langevin
thermostat, Maxwell-Boltzmann velocity sampling, Fisher-Yates shuffling of a
Perlin gradient table, stochastic precipitation, and so on).

All four link the lower-level `qrng_*` API directly (`qrng_init`,
`qrng_double`, `qrng_range32`, `qrng_uint64`), not the higher-level
`secure_rng` interface. That API is a real 8-qubit state-vector simulation; the
same core is what the CHSH Bell test verifies elsewhere in the repo. For
production randomness the recommended entry point is `secure_rng` (hardware
entropy + NIST SP 800-90B health tests + quantum mixing, with a VERIFIED mode
that runs a real Bell test before serving bytes); these examples deliberately
use the raw quantum core because they are studying its statistics, not shipping
secrets.

## Building and running

Build the library once from the repo root:

```
make
```

Then build any single example by its Makefile target, or all of them at once:

```
make molecular_dynamics      # one program
make examples_all            # every example in the repo
```

Binaries are written to the repo root, so run them from there, e.g.
`./molecular_dynamics`. The examples link against the library, which is
compiled with OpenMP, so on macOS make libomp visible at runtime:

```
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib ./molecular_dynamics
```

`quantum_noise` writes its output files (`noise_0.csv` … `noise_4.csv`) into
the current working directory, so run it from wherever you want those files.

---

## molecular_dynamics

**In one sentence:** it simulates a box of 100 atoms bouncing off each other and
shows that the quantum RNG can keep the simulated temperature steady instead of
letting it blow up.

**What it does (technical).** A 3-D Lennard-Jones fluid integrated with velocity
Verlet under a Langevin thermostat. Atoms start on the smallest cubic lattice
that holds them (random placement would create overlapping pairs whose r^-12
repulsion instantly diverges), with a small quantum jitter added. Initial
velocities are Maxwell-Boltzmann distributed, sampled from the RNG via
Box-Muller. Forces use the minimum-image convention and a 2.5-sigma cutoff, with
a floor at r_min = 0.8 sigma so a single close approach cannot produce a
non-finite force. The thermostat adds friction `-gamma*v*dt` plus balanced
quantum noise of amplitude `sqrt(2*gamma*T*dt/m)`, which is what keeps the
temperature near the target rather than heating without bound. Over the last 100
of 1000 steps it accumulates a radial distribution function g(r) and normalizes
it by shell volume and density.

**What it teaches.** Correct velocity-Verlet structure (update all positions,
compute forces once, then update all velocities), the fluctuation-dissipation
balance behind a Langevin thermostat, and how RNG quality shows up as a stable
observable. A verification run held the temperature near the target of 1.0
across the whole trajectory (values in the ~0.96-1.18 range) and produced a
g(r) that settles toward 1.0 at large r, both signs of a physically sane
simulation.

**How to run.** Has `main(void)` and takes no command-line flags; all parameters
(100 particles, 1000 steps, dt = 0.002, T = 1.0, gamma = 0.5, box = 10) are
compile-time constants. Just run `./molecular_dynamics`. It prints a temperature/
energy table every 100 steps followed by the full g(r).

**Notes/caveats.** Educational scale: O(N^2) pairwise forces, 100 particles, no
neighbor lists. Reduced Lennard-Jones units throughout. The RNG seed is fixed
(`"mdsim"`), so runs are reproducible rather than fresh each time.

---

## quantum_walk

**In one sentence:** it races a quantum walker against an ordinary random walker
and shows the quantum one spreading out far faster.

**What it does (technical).** Discrete-time quantum walks in 1-3 dimensions with
a selectable coin operator: Hadamard, Grover diffusion, quantum Fourier
transform, or a classical control that quantum-randomly collapses to one
direction each step. The coin dimension is 2^d; the shift operator moves
amplitude along +/-1 in each axis according to the coin bits, merging duplicate
positions and renormalizing. It reports mean distance, standard deviation,
spreading rate, a "quantum speedup" versus the classical sqrt(steps) spread, and
the coin's von Neumann entropy (in bits).

**What it teaches.** The headline result of quantum walks: **ballistic vs
diffusive spreading.** A classical walk's mean distance grows like sqrt(n); a
Hadamard walk's grows linearly in n. A verification run at 150 steps made this
concrete: the Hadamard walk reached mean distance ~75 (speedup ~6x) while the
classical walk reached ~6.8 (speedup ~0.56x). This linear-vs-square-root
separation is the reason quantum walks underpin several quantum algorithms.

**How to run.** Has `main(int argc, char *argv[])` with a full getopt parser:

- `-t, --type` classical | hadamard | grover | fourier (default hadamard)
- `-n, --steps N` (10-5000, default 200)
- `-w, --walkers N` (1-1000, default 10)
- `-d, --dimensions N` (1-3, default 1)
- `-s, --seed STRING` RNG seed
- `-q` quiet, `-v` verbose, `-j` JSON output, `-c` CSV output
- `-H` hide histogram, `-S` hide statistics, `-a` animate (1-D ASCII),
  `-P` hide progress bar
- `-o, --output FILE` redirect all output to a file
- `-h, --help`

The `-c` flag emits CSV (config, metrics, and the position histogram) to
stdout; combine with `-o file.csv` to capture it. Example:
`./quantum_walk -t hadamard -n 150 -w 20 -d 1 -c -o walk.csv`.

**Notes/caveats.** The step ceiling (5000) is deliberate: the position
bookkeeping is quadratic in the support size, so runtime grows quickly. The
"quantum speedup" figure is a descriptive ratio to sqrt(steps), not a rigorous
complexity claim. The reported entanglement is the Shannon entropy of the coin
distribution in this simplified product-state model, not a full bipartite
entanglement measure.

---

## quantum_noise

**In one sentence:** it turns quantum randomness into five flavors of noise
(white, pink, brown, Perlin, fractal) and then measures each one's frequency
spectrum to prove it came out right.

**What it does (technical).** For each noise type it generates an 8192-sample
buffer at a nominal 44.1 kHz, writes it to CSV, and runs a direct DFT to report
the power spectrum in octave bands, plus mean/std/min/max.

- **White**: uniform `2*qrng_double - 1`, flat spectrum.
- **Pink**: four-pole IIR filter approximating a 1/f spectrum.
- **Brown**: bounded integration of white noise (random walk with clamping).
- **Perlin**: 1-D value-gradient noise over a permutation table that is
  Fisher-Yates shuffled by the quantum RNG. Crucially the gradients are a fixed
  function of the lattice coordinate; drawing fresh random gradients per call
  (the classic bug) would collapse Perlin noise back into white noise.
- **Fractal**: four octaves of Perlin noise with persistence 0.5.

**What it teaches.** How the same entropy source feeds very different spectral
shapes, and how to verify a noise generator by its spectrum rather than by eye.
A verification run of the white-noise stage produced mean ~0.006, std ~0.58, and
a flat octave-band spectrum (~0.004-0.006 across bands), consistent with genuine
white noise. It also teaches the correct role of an RNG in procedural noise:
seed the structure once, then sample a deterministic field.

**How to run.** Has `main(void)` and takes no flags. Run `./quantum_noise`; it
generates all five types in sequence, printing analysis to stdout and writing
`noise_0.csv` (white) through `noise_4.csv` (fractal), each `Sample,Value` with
8192 rows, into the current directory.

**Notes/caveats.** The spectrum uses a plain O(N^2) DFT, fine for 8192 samples
but not an FFT. The 44.1 kHz sample rate is a label for the band-frequency
readout, not real audio output. Seed is fixed (`"noise"`).

---

## weather_sim

**In one sentence:** a small toy atmosphere on a grid, where quantum randomness
moves pressure systems around and drives a rough rain cycle.

**What it does (technical).** A 50x50 cell grid with five moving pressure
systems. Each hour the systems drift, wander in strength, and wrap around the
grid edges; every cell then recomputes pressure from the nearby systems and gets
quantum-random perturbations to temperature, wind, and humidity. A simple
moisture cycle relaxes humidity toward a 55% baseline, condenses high humidity
into cloud cover, and triggers stochastic precipitation when humidity and cloud
cover are both high, with rain then removing moisture and thinning clouds. It
simulates 24 hours and prints statistics every 6 hours.

**What it teaches.** How to keep a coupled stochastic system inside physical
bounds (humidity/wind/cloud are all clamped), and why a closed model needs a
moisture source: without the evaporation-toward-baseline term the humidity would
decay monotonically and rain could never occur. It is a compact example of
quantum randomness driving spatially-correlated dynamics rather than
independent samples.

**How to run.** Has `main(void)` and takes no flags. Run `./weather_sim`; it
prints per-cell-averaged temperature, pressure, humidity, cloud cover, rain
coverage, accumulated precipitation, and the five pressure systems' positions,
every 6 hours and at the end.

**Notes/caveats.** This is a qualitative toy, not a numerical weather model:
there is no real fluid dynamics, no advection of moisture, and the units are
illustrative. Grid size, 24-hour horizon, and five systems are compile-time
constants. Seed is fixed (`"weather"`).

---

Part of the Quantum RNG examples. The full quantum-simulation toolkit and the
continued Bell-verified RNG live in Moonlab:
https://github.com/tsotchke/moonlab
