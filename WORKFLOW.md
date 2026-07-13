# WORKFLOW — A Rigorous but Inviting Pipeline for tsotchke's Work

This fork exists to cross-validate `tsotchke/*` math against an independent
implementation (`waefrebeorn/WuBuMath`) and to demonstrate a workflow that makes
"proven / quantum / non-deterministic" claims *falsifiable*. The goal is
collaboration, not confrontation: every finding is reproduced, documented, and
offered back as a PR.

## The pipeline (3 gates, all mandatory)

### Gate 1 — Read the source, not the README
Marketing docs describe *intent*. Code describes *fact*. We grep the actual
functions, read the bodies, and never trust a claim that isn't backed by a
function we can compile.

> quantum_rng: "63.999872 bits/sample" is hardcoded text, no code computes it.
> "quantum-inspired" = sin/cos/sqrt + splitmix64, no quantum mechanics.

### Gate 2 — Reproduce every claim numerically (C contract)
Standalone C test, no heavy deps, that checks the *statistical property* the
claim implies.

- quantum_rng: same seed → same stream must hold. It fails (seed ignored at init)
  → not a reproducible PRNG; stream keyed by wall-clock time + PID.

This lives in `audit/` (build + run is one `gcc` line; we patch the broken
upstream build via a wrapper). No LLVM, no CUDA.

### Gate 3 — Formalize the true claims (Lean 4)
Theorems that survive Gate 2 get a Lean proof in `waefrebeorn/WuBuMath/lean/WubuProofs/`
(CI: `lake build` + sorry-audit, 0 sorry enforced). This closes the loop from
"it ran" to "it's proven."

> Already proven (WuBuMath, 0 sorry): `PoincareBall`, `FiberBundle` (SO(3)
> closure), `NestedHyperbolicSpaces`, `PowerTower` (π↑↑4 bounds), and an RNG
> min-entropy audit pattern applicable here.

## How tsotchke's work plugs in (concrete next steps)

| tsotchke repo | What's true | What to do | WuBu artifact to build on |
|---|---|---|---|
| `quantum_rng` | competent classical mixer; false "quantum" claim | PR: fix build, real seed, measure entropy | WuBu RNG audit + Lean min-entropy lemma |
| `eshkol/manifold.esk` | Christoffel/curvature correct; exp-map buggy | PR fix + Lean `exp_map_geodesic_invariant` | `PoincareBall.lean` |
| `libirrep` | SO(3)/SE(3) exp-log, CG/Wigner — production-grade | Port proofs to `FiberBundle.lean` | existing SO(3) commutation proof |
| `quantum_geometric_tensor` | RK4 geodesic integrator (real) | Port + prove geodesic-eq contract | `wubu_manifold.c` (already done) |
| `moonlab` | SU(2)_k anyon fusion/R/6j (real) | Port + prove fusion invariants | `wubu_anyon.c` (already done) |

## Invitation template (rigorous but welcoming)

> "We audited your `<repo>` against an independent implementation. Your `<X>` is
> solid. We found `<one specific issue>` — reproduced here: `<path>`. Fix + test
> attached. Want to co-author a PR? We can also formalize `<invariant>` in Lean
> as a shared proof artifact."

## Principles (from the Mind Palace DA loop)
- **No theorem without a check** — Lean proof OR numeric contract, never neither.
- **Keep upstream verbatim** — document bugs, don't silently rewrite.
- **Reproduce, don't assert** — every finding has a one-command repro.
- **Assume good faith** — "quantum" is analogy until proven malice; fix is labeling.
- **Upstream PRs, not forks-as-truth** — the fork is evidence; the PR is the gift.

## Cross-reference
- quantum_rng-audit: `.hermes/mind-palace/{prestige_prompt,goal-mantra,state,overnight-map}.md`,
  `plans/devils_advocate_v1.md`, `AUDIT.md`, `audit/determinism_test.c`.
- eshkol-audit: same structure + `cross-validation/REPORT.md`, `ESHKOL_STRUCTURE.md`.
- Reference impl: `waefrebeorn/WuBuMath` (`lean/WubuProofs/`, `src/math/`, `VALIDATION.md`).
