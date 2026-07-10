# Crypto examples

These programs use the quantum RNG as a source of randomness for a set of
small, **educational** cryptography demos: password-based key derivation, a toy
key exchange, a hash-linked "quantum chain," Wiesner quantum money, and
random-token / password generators.

Read this first, before you copy anything into a real system:

> **These are teaching tools, not production security.** Where they hash, they
> use a real, from-scratch SHA-256 (FIPS 180-4, see `sha256.h`) so that "hash"
> always means a genuine cryptographic hash. But the *protocols* around those
> hashes are deliberately simplified so you can read them end to end. Several
> are explicitly breakable and say so in their own source comments. For real
> systems use vetted primitives (Argon2id/scrypt for KDFs, X25519 or a KEM for
> key exchange, HMAC or signatures for authentication, libcrypto/liboqs for the
> plumbing).

## How the quantum RNG fits in

The library at the repo root is a real state-vector quantum simulation whose
randomness is validated with a CHSH Bell test (classical bound 2, Tsirelson
bound 2√2 ≈ 2.828 — a value above 2 is the signature of genuine quantum
correlations). Two API layers show up in these examples:

- **`qrng_*`** — the lower-level generator (`qrng_init`, `qrng_bytes`,
  `qrng_measure_state`). Used by `key_derivation`, `key_exchange`,
  `quantum_chain`, `secure_token`, and `password_gen`.
- **`secure_rng`** — the **recommended** API: hardware entropy + NIST SP 800-90B
  health tests + quantum mixing, with modes `FAST` / `QUANTUM` (default) /
  `HYBRID` / `VERIFIED` (which runs a real Bell test before serving bytes).
  Used by `quantum_money`.

## Building and running

The library builds from the repo root:

```sh
cd ../..          # repo root: quantum_rng_rebuild/
make              # builds libquantumrng.so + libsecure_qrng.so
```

Each program has a Makefile target and is emitted **into the repo root** (not
this directory). Build them individually or all at once:

```sh
make key_derivation_test      # KDF self-test
make key_verification         # KDF determinism / verification suite
make key_exchange_test        # key-exchange test suite
make quantum_chain_test       # hash-chain test suite
make quantum_money            # Wiesner quantum money demo (via `make showcase` too)
make examples_all             # every example in the repo
```

Run from the repo root, e.g. `./key_verification`. `key_derivation_test`,
`key_verification`, and `quantum_money` link the library objects statically, so
they run as-is. `key_exchange_test` and `quantum_chain_test` link the shared
library, so if the loader cannot find it, prefix the run with
`DYLD_LIBRARY_PATH=.` on macOS (`LD_LIBRARY_PATH=.` on Linux). None of the
crypto examples use OpenMP.

None of these programs take command-line flags except `quantum_money` (see
below).

---

## Program reference

The files split into **library modules** (no `main()` — compiled into another
program: `key_derivation`, `key_derivation_utils`, `key_exchange`,
`quantum_chain`) and **runnable programs** (`key_derivation_test`,
`key_verification`, `key_exchange_test`, `quantum_chain_test`, `quantum_money`,
`secure_token`, `password_gen`).

### key_derivation (library module)

**Plain:** Turns a password plus a salt into a fixed-length key, the way a login
system stretches a password into an encryption key.

**Technical:** A PBKDF2-style construction over iterated SHA-256
(`key_derivation.c`, no `main()`). `state₀ = SHA-256(password ‖ salt ‖ key_size
‖ quantum_mix)`; then `stateᵢ = SHA-256(stateᵢ₋₁ ‖ password ‖ salt ‖ i)` for the
requested iteration count; then the final state is expanded to `key_size` bytes
in SHA-256 counter mode. The derivation is fully deterministic given `(password,
salt, iterations, key_size, quantum_mix)` — a KDF must be reproducible so a key
can be re-derived to check a password. **The quantum RNG's only role is
generating a fresh salt** (via `qrng_bytes` + `qrng_measure_state`) when the
caller doesn't supply one. Config in `key_derivation.h`: key size 16–64 bytes,
iterations 10k–1M (default 100k), 16-byte salt.

**Teaches:** why a KDF is deterministic, salting, iteration/stretching, and
counter-mode expansion.

**Caveats:** The file's own header says it plainly — this is **not memory-hard**
and has **no security proof**; use Argon2id or scrypt for real systems. The
`memory_size` and `num_threads` fields in `kdf_config_t` are vestigial (only
printed, never used); the derivation is single-threaded.

**Links into:** `key_derivation_test`, `key_verification`.

### key_derivation_utils (library module)

**Plain:** Helper routines that print, format, and sanity-check derived keys.

**Technical:** `key_derivation_utils.c` (no `main()`) provides `init_kdf_config`
(defaults), `free_kdf_result` (wipes the key before freeing), `estimate_entropy`
(Shannon entropy in bits/byte), and the `print_results` / JSON / hex output
helpers plus `verify_key_strength`. **Links into:** `key_derivation_test`,
`key_verification`.

### key_derivation_test (`main()`)

**Plain:** A minimal smoke test that derives one key and checks it isn't
obviously broken.

**Technical:** Derives a 16-byte key from password `"test"` with 10 iterations
and asserts the derived key exists and its Shannon entropy clears the
(intentionally low) `BASIC_ENTROPY` = 3.5 bits/byte bar, then prints timing.

**Run:** `./key_derivation_test`

**Caveats:** 3.5 bits/byte over 16 bytes is a coarse smoke check, not a
randomness guarantee — Shannon entropy over such a short buffer is noisy by
nature.

### key_verification (`main()`)

**Plain:** Checks the core promise a KDF must keep — same password and salt
always give the same key, and a different password or salt gives a different
one.

**Technical:** Despite the name, this is a **test suite** (`key_verification.c`,
`main()`), not a library. `test_key_derivation` derives keys for several test
passwords, asserts determinism (re-deriving with the same config matches
byte-for-byte), asserts a changed salt changes the key, and runs a 100-iteration
timing benchmark. `test_key_verification` shows the password-check pattern:
re-derive and `memcmp`, with a wrong password producing a different key.

**Run:** `./key_verification`

### key_exchange (library module)

**Plain:** A stripped-down illustration of how two parties run a handshake —
generate key material, hash a shared transcript, and derive a session key — so
you can see the moving parts.

**Technical:** `key_exchange.c` (no `main()`) implements
`simulate_network_exchange`: both parties generate a private key from quantum
randomness (four mixing rounds plus a conditioned entropy-pool pass), publish
`public_key = SHA-256(private_key)` and a nonce, form
`shared_secret = public_key_A XOR public_key_B`, fold both public keys and both
nonces into a running transcript hash (`hash = SHA-256(hash ‖ data)`), and
derive `session_key = SHA-256("QKX-session-v1" ‖ shared_secret ‖ transcript)`.
Hashing is real SHA-256. Its source comments document several bugs that were
fixed (an "enhanced_sha256" that wasn't a hash, a transcript mixer with no
positional dependence, an entropy pool consumed before it was filled).

**Teaches:** the *shape* of a handshake — key material, transcript binding,
KDF-style session-key derivation — and, by construction, a worked eavesdropper
attack.

**Caveats (read the header):** This is a **toy**. The "shared secret" is the XOR
of the two public values, both of which travel in the clear, so **a passive
eavesdropper who sees both public keys can compute the shared secret and every
session key derived from it** — there is no Diffie-Hellman-style hardness here.
`key_exchange.h` also declares `parse_exchange_args` / `print_exchange_usage`
for a standalone CLI, but no such `main()` is provided or built; the code is
exercised only through `key_exchange_test`.

**Links into:** `key_exchange_test`.

### key_exchange_test (`main()`)

**Plain:** Runs the toy handshake and confirms both sides end up with the same
session key.

**Technical:** `key_exchange_test.c` (`main()`) checks that generated key
material is non-zero and unique across runs, that both parties derive an
identical session key and transcript hash, that config defaults are sane, and
that the `NORMAL` / `QUIET` / `JSON` output modes run. Entropy thresholds use
`MIN_ENTROPY` = 4.5 bits/byte.

**Run:** `DYLD_LIBRARY_PATH=. ./key_exchange_test`

### quantum_chain (library module)

**Plain:** A tiny tamper-evident ledger: each record stores a hash of the
previous one, so editing an old record breaks the chain (a blockchain-style data
structure, minus the network and consensus).

**Technical:** `quantum_chain.c` (no `main()`). Each `QuantumBlock` holds an
index, timestamp, `prev_hash`, a per-block 64-byte `quantum_signature` drawn
from `qrng_bytes`, up to 1 KB of data, and a 64-byte `hash` computed as
`H ‖ SHA-256(H)` where `H = SHA-256(index ‖ timestamp ‖ prev_hash ‖
quantum_signature ‖ data_size ‖ data)`. `quantum_chain_verify` walks from
genesis, checking each block's `prev_hash` link and recomputing its hash into a
scratch buffer. Also supports export/import to a file (with bounds-checked,
validated reads of untrusted input) and chain statistics.

**Teaches:** hash chaining / tamper-evidence and careful serialization of
untrusted data.

**Caveats:** The per-block "quantum signature" is **random data, not a
cryptographic signature** — it makes each block unique but authenticates
nothing; there is no signing key and no verification of origin. Integrity here
means "the chain is internally consistent," not "the chain came from a trusted
author."

**Links into:** `quantum_chain_test`.

### quantum_chain_test (`main()`)

**Plain:** Builds a chain, tampers with a block, and shows verification catches
it.

**Technical:** `quantum_chain_test.c` (`main()`) covers init, block addition and
linkage, uniqueness of per-block signatures, verification of an intact chain,
detection of a tampered block (flipping one data byte fails verification),
round-trip export/import equality, statistics, and a 1000-block performance
timing.

**Run:** `DYLD_LIBRARY_PATH=. ./quantum_chain_test`

### quantum_money (`main()`)

**Plain:** A hands-on demo of Stephen Wiesner's 1983 idea: money whose
authenticity rests on a quantum state that, on real hardware, physically cannot
be copied (the no-cloning theorem). The bank mints notes, verifies genuine ones,
and watches a counterfeiting attempt fail.

**Technical:** `quantum_money.c` (`main()`), built on `secure_rng` +
`quantum_state` + `bell_test` + `quantum_entropy`. Minting draws a serial number
and one of the four Bell states via `secure_rng`, applies a secret random phase
rotation, and certifies it with a CHSH Bell test (`bell_test_chsh`, printing
CHSH against the classical bound 2 and Tsirelson bound 2.828). The bank keeps a
reference copy of every note's state. Verification recomputes state fidelity
`F = |⟨note|vault⟩|²` against the vault copy (accept threshold 0.999) plus a
fresh Bell test. `quantum_money_attempt_counterfeit` copies the public fields but
must **fabricate** a new quantum state (it doesn't know the original Bell type or
phase), which the fidelity check then rejects.

**Teaches:** the no-cloning theorem, Bell/CHSH certification, and fidelity-based
state comparison as an authenticity test.

**Run:**

```sh
./quantum_money             # quick demo (prompts for Enter)
./quantum_money --demo      # no-cloning demonstration + classical comparison
./quantum_money --protocol  # full mint / verify / counterfeit walkthrough
```

**Caveats (read the header):** This is a **classical simulation** of the
protocol — in a simulator the "quantum state" is ordinary memory and *could* be
copied; the no-cloning guarantee only holds on real quantum hardware. Acceptance
is statistical: the program itself notes that a fabricated state can occasionally
match the original's Bell type and phase closely enough to pass a single check,
which repeated or higher-precision verification would expose.

### secure_token (runnable program)

**Plain:** Generates random session-style tokens (like an API key or a
"remember me" cookie value) with an attached checksum and an expiry time.

**Technical:** Tokens are drawn from `qrng_bytes` over a configurable character
set using **rejection sampling** (`quantum_uniform_index`) so there is no modulo
bias. Each token carries a 64-byte integrity field (`H ‖ SHA-256(H)` over a
domain-separation label, length, and token), creation and expiration timestamps,
and helpers to (de)serialize, validate, revoke, and batch-generate unique tokens.
Its `main()` demonstrates the module end to end — generation at several lengths,
tamper detection (a flipped byte fails the checksum), a serialize/parse
round-trip, expiry/revocation, and a batch that reports the measured
character-class distribution.

**Build/run:** `make secure_token`, then e.g. `./secure_token -n 8 -l 40 -s`.
Options: `-n` count, `-l` length, `-s` include symbols, `-h`.

**Teaches:** unbiased random sampling and token lifecycle (issue → validate →
expire/revoke).

**Security note:** the integrity field is an **unkeyed** checksum — it detects
accidental corruption, not forgery. A production system would use an HMAC with a
server-side secret or a digital signature.

### password_gen (runnable program)

**Plain:** Generates strong random passwords that satisfy rules like "must
include an uppercase letter, a digit, and a symbol."

**Technical:** All character choices come from `qrng_bytes` via rejection
sampling (`quantum_uniform`), so selection is uniform with no modulo bias.
`generate_password` enforces length bounds and draws only from the requested
character classes (a fixed bug where every class leaked into every password),
guaranteeing each required class appears, with a bounded retry loop rather than
unbounded recursion. Its `main()` runs several preset policies plus a
getopt-driven custom policy, validating every password and printing the measured
character-class distribution and mean entropy estimate.

**Build/run:** `make password_gen`, then e.g. `./password_gen -n 5 -l 24 -c uln`.
Options: `-l` length, `-n` count, `-c` classes (`u`pper/`l`ower/`n`umber/`s`pecial), `-h`.

**Teaches:** bias-free random selection and constraint-satisfying generation.

**Note:** `calculate_password_entropy` reports the idealized entropy of a
uniformly random password over the chosen character set — an upper bound, not a
measurement of a specific password's guessability.

---

## A note on older analysis documents

Two earlier analysis files (`key_derivation_analysis.md`,
`key_exchange_analysis.md`) described superseded implementations and, in the key
exchange case, made security claims — "forward secrecy," "ready for production
use" — that contradicted the actual toy code. Because they were inaccurate and
potentially misleading, they have been moved to
`docs/archive/legacy-example-analysis/`. **This README and the source comments
are the authoritative description of the current code.**

---

Part of the Quantum RNG examples. The full quantum-simulation toolkit and the
continued Bell-verified RNG live in Moonlab:
https://github.com/tsotchke/moonlab
