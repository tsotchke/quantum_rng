# AGENTS.md — waefrebeorn/quantum_rng_audit

Fork of `tsotchke/quantum_rng` for **devil's-advocate auditing**. Upstream:
`git@github.com:tsotchke/quantum_rng.git` (remote `upstream`).

## What this fork is for

Independently verify (or refute) upstream's claims — "quantum-inspired",
"verified non-deterministic", "63.999872 bits/sample entropy". The audit
conclusion (see `AUDIT.md`): it is a **classical dual-mode PRNG** — a pure
function of the seed when seeded (reproducible across PID/time), non-deterministic
when unseeded — with quantum-themed naming and (now-fixed) entropy claims.
Upstream code is audited here; verified defects are fixed in place under
`src/` and cross-checked by tools under `audit/`.

## Repo reality

- **Language:** C11. Single core file `src/quantum_rng/quantum_rng.c` (~560
  lines) + header. Examples (dice, crypto key-exchange, finance Monte Carlo) in
  `examples/`. Tests (chi-square uniformity) in `tests/`.
- **No quantum mechanics.** "Qubits" = array indices; "Hadamard/Pauli gates" =
  `splitmix64`/`xorshift` bit-mixing; magic constants named after physical
  quantities are arbitrary 64-bit ints.
- **Entropy source:** `gettimeofday` + `getpid` + `clock` + `rdtsc` at init.
  Stream is deterministic given the init timestamp.

## Build (note: upstream build is BROKEN)

Upstream `make all` fails: `quantum_rng.h` uses `pid_t` without `<sys/types.h>`;
`quantum_rng.c` uses `M_PI`/`M_E` without definition. The audit test patches
this via a wrapper (see `audit/determinism_test.c`):

```bash
cd audit
gcc -std=c11 -O2 determinism_test.c -o determinism_test -lm
./determinism_test
```

To fix upstream properly: add `#include <sys/types.h>` to the header and ensure
`M_PI`/`M_E` are defined (e.g. `#define _USE_MATH_DEFINES` before `<math.h>`, or
guard-define them).

## Audit deliverables

- `AUDIT.md` — full findings (claim-vs-reality table, empirical results,
  build defects, verdict, upstream-PR recommendations).
- `audit/determinism_test.c` — reproducible proof that **the seed governs the
  stream** (run in separate processes: same seed → byte-identical output across
  PID/time). Corrected 2026-07-12.

## Conventions for agents

- **Do not "fix" upstream silently.** Reproduce issues in `audit/`, document in
  `AUDIT.md`, keep upstream files verbatim. Submit fixes upstream via PR.
- **Math/claims:** every claim needs a check (compile + run + numeric). No
  theorem without verification.
- **Upstream sync:** `git fetch upstream && git merge upstream/main` on a branch,
  then PR. Never force-push `main`.
- **No secrets.** None in the fork; do not add any.
- **Big files:** GitHub rejects >100 MB on push; keep build artifacts local.

## Known pitfalls

- The "63.999872 bits/sample" number is hardcoded marketing text — no code
  computes it; no NIST/Dieharder suite exists. The only test is chi-square
  *uniformity* (validates a PRNG is uniform, not that it has quantum entropy).
- `qrng_init` **honors the seed** in the current code (seeded mode = pure
  function of `absorb_seed(seed)`, reproducible across runs; unseeded mode draws
  `gettimeofday`/`getpid`). `audit/determinism_test.c` proves it (run in separate
  processes).
- Not suitable for cryptography/security despite "key exchange" examples — the
  unseeded entropy is predictable (time + PID), and the `secure_rng` hardware
  layer is still unverified by us.
