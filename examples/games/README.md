# Games and Procedural Content

This directory shows how a quantum random-number generator plugs into the kinds
of systems games actually need: dice, slot machines, loot drops, particle
effects, procedural worlds, and even a genetic-algorithm "ecosystem." In plain
terms, every place a game would normally call `rand()`, these examples call the
quantum RNG instead, and a couple of them go further and *prove* their fairness
with a physics test.

Two APIs appear here:

- **`qrng_*`** — the lower-level quantum RNG. A real 8-qubit state-vector
  simulation drives the entropy; the API surface used here is ordinary
  (`qrng_init`, `qrng_double`, `qrng_uint64`, `qrng_range32`,
  `qrng_entangle_states`). Most demos in this folder use it directly, usually
  seeded with a fixed string so runs are reproducible.
- **`secure_rng`** — the recommended production-facing API (hardware entropy +
  NIST SP 800-90B health tests + quantum mixing, with FAST/QUANTUM/HYBRID/
  VERIFIED modes). Only `bell_certified_lottery` uses it, because that example
  needs the verified, entanglement-backed path.

Honest framing: these are teaching demos. The lottery's "commitment" hash is a
toy XOR, not SHA-256, and none of this is hardened for real money or security.
What is genuine is the randomness source and the Bell test underneath the
lottery.

## Building and running

The library builds from the repo root:

```sh
make                       # build the core library
make <name>                # build one example (targets listed per program below)
make examples_all          # build every example in the repo
```

Binaries are written to the repo root, so run them as `./quantum_dice_demo`,
`./quantum_slots`, and so on.

These examples link the full library, which is OpenMP-parallelized. On macOS,
prefix runs with the libomp path so the runtime loader can find it:

```sh
export DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib
./quantum_dice_demo
```

Measured timings below are from this machine and will vary; they are only meant
to flag which programs are instant and which take a while.

---

## `quantum_dice` (library, no `main`)

**Plain:** a fair polyhedral-dice roller (d4, d6, d8, d10, d12, d20, d100).

**Technical:** `quantum_dice.c`/`quantum_dice.h` define an opaque
`quantum_dice_t` wrapping a `qrng_ctx`. `quantum_dice_roll()` performs
**exactly-unbiased rejection sampling**: it draws a 64-bit value with
`qrng_uint64()` and rejects any draw that falls in the unfair tail
(`r < 2^64 mod sides`) before returning `r % sides + 1`. Because that tail is
discarded, every face is equally likely with zero residual bias — not merely
below a detection threshold. `quantum_dice_batch_roll()` applies the same
rejection scheme per element across an array (capped at 1000), deliberately
using one accepted draw per roll (an earlier version averaged several uniforms,
which produced a bell-shaped Irwin-Hall distribution biased toward the middle
faces). `quantum_dice_reset()` discards a few outputs; it does not re-seed.

**Teaches:** how to turn a raw uniform integer generator into provably fair
discrete outcomes with rejection sampling, and why a plain modulo is subtly
biased when the range does not divide the generator's period evenly.

**Build/run:** no standalone binary. This object is linked into
`quantum_dice_demo` and `quantum_dice_test` below.

---

## `quantum_dice_demo` (`main`)

**Plain:** a quick, human-readable tour of the dice roller.

**Technical:** rolls a d6 and d20 a few times, does a 10-roll batch, then rolls
a d6 1,000 times and prints the per-face distribution as percentages. Uses the
default `qrng_init(&ctx, NULL, 0)` seeding.

**Teaches:** basic usage of the `quantum_dice` API end to end.

**Build/run:** `make quantum_dice_demo`, then `./quantum_dice_demo`. No flags.
Runs instantly.

---

## `quantum_dice_test` (`main`)

**Plain:** a statistical test suite that checks the dice are actually fair.

**Technical:** four test blocks, each 1,000,000 rolls:

1. **D6 distribution** — chi-square against the uniform expectation (df = 5,
   critical value 11.070 at 95%).
2. **Fairness across sizes** — repeats the chi-square check for d4/d6/d8/d10/
   d12/d20.
3. **Sequential independence** — tallies all 36 consecutive-pair combinations
   and chi-square tests them for serial correlation.
4. **Stress** — 1,000 rapid create/destroy cycles and 1,000,000 rolls checked
   for out-of-range values.

Each block prints a PASS/FAIL verdict against the tabulated critical value.

**Teaches:** how to validate an RNG's uniformity and independence with
chi-square tests.

**Build/run:** `make quantum_dice_test`, then `./quantum_dice_test`. No flags.
**Runs long** — roughly 15 seconds here (about 9M+ rolls total). Expect a wall
of numbers followed by PASS lines.

---

## `bell_certified_lottery` (`main`)

**Plain:** a lottery that doesn't just claim to be fair — it runs a physics
experiment on each draw and shows you the result.

**Technical:** the headline example, and the only one using the `secure_rng`
API. For each ticket it (1) draws a random reveal value, (2) builds a two-qubit
Bell state `|Φ⁺⟩`, (3) runs a **CHSH Bell test** on that state
(`bell_test_chsh`, default 5,000 measurements), (4) certifies the draw only if
the measured CHSH statistic exceeds the classical bound of 2.0 and is
statistically significant, then (5) draws six unique numbers in `[1, 49]` from
the certified secure RNG and binds them with a commitment hash.

**Why the Bell test matters:** the CHSH quantity has a hard ceiling for *any*
classical/deterministic system — the classical bound of **2.0**. Quantum
entanglement can push it up to Tsirelson's bound, **2√2 ≈ 2.828**. So a value
above 2.0 is something no pseudo-random generator can fake: it is evidence the
numbers came from genuine quantum measurement, not a riggable algorithm. The
built-in `--demo` mode makes this concrete by measuring CHSH on a *non*-entangled
(product) state as a stand-in for a classical RNG.

Measured on this machine:

- Classical/product state: **CHSH ≈ 1.36** — below 2.0, cannot be certified.
- Entangled quantum state: **CHSH ≈ 2.88** — above the classical bound (and, due
  to finite-sample statistical fluctuation at 5,000 measurements, even a touch
  above the ideal 2.828).

**Teaches:** device-independent randomness certification — the idea that you can
*prove* randomness is real rather than asking users to trust the operator.

**Caveats:** educational. `hash_commitment()` is a toy XOR mixer (the source even
notes "in production, use SHA-256"), the classical comparison is a simulated
product state rather than an external PRNG, and this is not audited gambling
software.

**Build/run:** `make bell_certified_lottery`, then:

```sh
./bell_certified_lottery           # single certified ticket (default)
./bell_certified_lottery --multi   # 3 tickets + audit report (3,000 samples each)
./bell_certified_lottery --demo    # classical-vs-quantum CHSH comparison
```

Each Bell test runs thousands of measurements, so a run takes a few seconds.

---

## `quantum_slots` (`main`)

**Plain:** a 3-reel slot machine with weighted symbols and a fairness auditor.

**Technical:** seven symbols with fixed weights and payouts (cherry is common
and pays 3×; the "7" is rare, weight 2, and pays 100×). `select_symbol()` does
weighted sampling via `qrng_range32()` over the cumulative weight total; a win
requires all three reels matching. This is a fair weighted-outcome demo: the
odds are exactly the declared weights, with no hidden house tilt beyond the
payout table. `-f/--fairness` runs a 1,000,000-spin audit that prints each
symbol's observed vs. expected count in sigma units plus the effective
return-to-player.

**Teaches:** weighted random selection and how to statistically audit a
payout table.

**Build/run:** `make quantum_slots`, then `./quantum_slots [OPTIONS]`. Key flags
(getopt):

- `-c N` starting credits (default 100)
- `-b N` bet amount (1–100, default 1)
- `-n N` number of automatic spins (`0` = run until credits bust)
- `-a` disable the spin animation
- `-s STR` seed string; `-q`/`-v`/`-j` quiet/verbose/JSON output
- `-f` run the fairness audit; `-i` interactive mode; `-o FILE` write to a file

**Runs long by default:** with no `-n`, it keeps spinning and animating until the
credits are gone, and the animation sleeps ~100 ms per frame. For a quick,
scriptable run use `-a -n N`, e.g. `./quantum_slots -a -n 20`.

---

## `loot_system` (`main`)

**Plain:** an RPG loot generator — random rarity, procedural item names, affixes,
and critical-hit rolls.

**Technical:** `generate_rarity()` samples a five-tier rarity from fixed weights
(Common 0.60 → Legendary 0.01) with `qrng_double()`. Higher rarity grants more
affixes, drawn without duplicates from a pool via `qrng_uint64() % pool_size`;
names are assembled from prefix/base/suffix tables. `calculate_damage()` shows a
crit system: a `qrng_double()` roll against total crit chance, with a further
quantum ±10% damage variation. Fixed seed `"loot"`, so output is reproducible.
Another fair weighted-outcome demo, this time in an item-drop context.

**Teaches:** weighted rarity tables, procedural name/affix generation, and
quantum-driven combat variance.

**Build/run:** `make loot_system`, then `./loot_system`. No flags. Prints nine
sample items (low/mid/high level) and ten sample attacks. Runs instantly.

---

## `particle_system` (`main`)

**Plain:** a 3D particle simulator where some particles are "entangled" so their
motion is linked.

**Technical:** particles carry position/velocity/mass/charge/spin/lifetime and
an `entangled_with` index. On emit, ~10% of particles pair with an existing one;
`ps_update()` applies attractor forces, an entanglement coupling term, and
lifetime decay, removing dead particles with swap-with-last. The interesting
engineering is the **entanglement bookkeeping**: whenever a particle is
re-paired, dies, or is moved by a swap-removal, the code re-points its partner's
back-reference so no particle ever points at a stale or recycled slot. The
`main` driver emits 6,000 particles from three emitters, simulates 8 s at 50 Hz,
and **ends with a consistency check** that scans every surviving particle and
verifies each entanglement link is mutual — printing "0 broken links (expect 0)"
and failing the program if any link is inconsistent (verified: 0 here).

**Teaches:** using quantum randomness for emission/velocity/properties, and
correct index bookkeeping in a mutating pool — the entanglement metaphor doubles
as a real data-structure-integrity exercise.

**Build/run:** `make particle_system`, then `./particle_system`. No flags.
Prints periodic stats and the final link check. Runs quickly.

---

## `procedural_worlds` (`main`)

**Plain:** generates a small ASCII world map — oceans, forests, deserts, snow —
from quantum noise.

**Technical:** a 128×128 map built from `quantum_noise2d()`, a cosine noise
function seeded by `qrng_uint64()`. Heights sum four octaves (amplitude halving,
frequency doubling) and are normalized by the octave-amplitude sum (1.875) so
they span `[0, 1]`; temperature and moisture use low-frequency noise. A
threshold table maps (height, temperature, moisture) to a biome character. Fixed
seed `"worldseed"`.

**Teaches:** octave/fractal noise for terrain and biome classification from
climate fields.

**Build/run:** `make procedural_worlds`, then `./procedural_worlds`. No flags.
Prints one ASCII map plus a legend. Runs instantly.

---

## `terrain_generation` (`main`, plus reusable library API)

**Plain:** a larger, richer world generator — heightmaps, mountains, rivers,
caves, and biomes on a wrapping 256×256 map.

**Technical:** the fuller cousin of `procedural_worlds`, split into a reusable
API (`terrain_generation.h`) plus a demo `main`. `generate_terrain()` runs, in
order: multi-octave base heightmap (6 octaves) blended with 4-octave *ridged*
noise for mountains; climate fields with temperature falling by elevation;
downhill-carved rivers with erosion from five sources; cave-density noise; and a
biome classifier. Game-integration queries (`get_height`, `get_slope`,
`is_buildable`, `is_water`, `get_biome`, …) treat the map as toroidal. Fixed
seed `"terrainseed"`. The demo allocates the ~1.6 MB map on the heap, renders a
4×-downsampled biome overview, and prints **measured** statistics (height
min/mean/max, river/water/buildable tile counts, biome histogram) counted from
the generated map rather than assumed.

**Teaches:** layered procedural generation (base + ridged noise, rivers,
erosion, caves), toroidal map queries, and honest reporting of generated stats.

**Build/run:** `make terrain_generation`, then `./terrain_generation`. No flags.
Prints the biome map and statistics. Runs quickly.

---

## `quantum_evolution` (`main`)

**Plain:** a genetic-algorithm demo — a population of 64-bit "genomes" evolves
over 100 generations toward several competing goals.

**Technical:** 1,000 organisms, each a `uint64_t` genome scored on three
objectives (bit-count, alternating pattern, palindromic symmetry) combined into
a weighted fitness. `evolve_population()` does tournament selection (best of 3),
single-mask uniform crossover, and adaptive mutation whose rate shrinks as max
fitness rises. It also calls `qrng_entangle_states()` to "quantum-entangle" each
pair of children — a genuine call into the quantum RNG's entanglement mixing,
used here as an extra stochastic operator. Progress bars print every 10
generations, ending with the best organism's genome and per-objective scores.
Fixed seed `"evoseed"`.

**Teaches:** a compact multi-objective genetic algorithm driven end-to-end by
quantum randomness (selection, crossover, mutation, and entanglement mixing).

**Build/run:** `make quantum_evolution`, then `./quantum_evolution`. No flags.
Runs quickly.

---

Part of the Quantum RNG examples. The full quantum-simulation toolkit and the
continued Bell-verified RNG live in Moonlab:
https://github.com/tsotchke/moonlab
