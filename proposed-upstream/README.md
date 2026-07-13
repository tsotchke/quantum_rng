# Proposed upstream changes (quantum_rng)

Copy-pasteable, self-contained patches for `tsotchke/quantum_rng`, derived
from the devil's-advocate audit in this fork (`waefrebeorn/quantum_rng_audit`).
Each file is independent — apply what you want.

## What the audit found (summary)
- It is a **competent classical dual-mode PRNG**, not quantum. "Qubits" /
  "gates" are array indices and splitmix64/xorshift mixes.
- Seeded mode is a **pure function of the seed** (reproducible across PID/time);
  unseeded mode is non-deterministic, drawn from `gettimeofday`/`getpid`.
- The "63.999872 bits/sample" number was hardcoded marketing text — no code
  computed it; no NIST/Dieharder suite exists. The fork replaced it with a real
  measured Shannon estimator (~7.95 bits/byte).
- A real hardware-entropy layer (`secure_rng`/`entropy`: RDSEED/RDRAND/`/dev/random`)
  was added upstream after our first pass — genuinely real *classical* entropy
  (health tests not yet run by us).

## Files in this directory
- `build-fix.patch` — adds the missing `<sys/types.h>` so `make` compiles
  (the actual upstream build break: `pid_t` used without the header).
- `readme-honesty.patch` — adjusts the "Verified non-deterministic output"
  wording to match the real seeded/unseeded contract.
- `audit-evidence.md` — pointer map to the reproducible proof in this fork.

All claims reproducible without special tooling:
```bash
# determinism (seed governs stream)
cd audit && gcc -std=c11 -O2 determinism_test.c -o det -lm && ./det
# real entropy measurement (from fork root)
gcc -std=c11 -O2 -Isrc -Isrc/quantum_rng audit/qrng_measure.c \
    src/quantum_rng/quantum_rng.c -o /tmp/qm -lm && /tmp/qm
```
