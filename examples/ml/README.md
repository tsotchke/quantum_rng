# Machine-learning examples

These programs show where high-quality randomness actually matters in neural
networks: setting the **initial weights** of a network, sampling a **generator's**
inputs, and picking the **next token** when a language model generates text. In
plain terms, before a network learns anything it has to start from random
numbers, and the quality of those numbers affects whether training behaves well.
Each example uses the quantum RNG for that randomness and then does something
concrete with it — measuring the resulting weight statistics, training a small
GAN, or running a tiny transformer text generator.

All three programs draw randomness from the lower-level `qrng_*` API
(`qrng_init`, `qrng_double`, `qrng_free`), not the higher-level `secure_rng`
interface. `qrng_double` returns a uniform in `[0, 1)`; where a Gaussian is
needed the code applies the **Box-Muller transform**
(`z = √(−2 ln u1) · cos(2π u2)`, using `1 − qrng_double` so `ln` never sees 0).
The RNG core is a real state-vector quantum simulation whose statistical quality
is verified elsewhere in the toolkit with a CHSH Bell test (classical bound 2,
Tsirelson bound 2√2 ≈ 2.828); these examples consume its output rather than
re-running the Bell test.

These are educational implementations. The networks are deliberately tiny and
train on toy data; the point is to make the mechanics — initialization variance,
GAN backpropagation, transformer attention and sampling — visible and correct,
not to reach state-of-the-art results.

## Building and running

Build the RNG library from the repository root, then build each ML program by
target name (the Makefile links each single-file example against the full RNG
object set):

```
make                     # builds libquantumrng.so etc.
make neural_init
make quantum_gan
make quantum_transformer
make examples_all         # builds every example across all domains
```

On macOS the binaries are built with OpenMP via Homebrew `libomp` and need it on
the loader path at runtime (these ML binaries link the RNG objects statically, so
`libomp` is the only runtime dependency):

```
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib ./neural_init
```

On Linux, OpenMP is linked directly and no special path is usually needed. None
of these programs take command-line arguments; each runs a fixed demonstration.

---

## `neural_init.c` / `neural_init.h` (has `main`)

**What it does.** Demonstrates how to initialise a neural network's weights from
quantum randomness, and then *measures* the resulting weight distributions to
confirm they match the textbook targets. It also builds a small network and runs
a forward pass with and without dropout.

```
make neural_init
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib ./neural_init
```

**The math.**

- **Xavier/Glorot initialisation** (`quantum_xavier_init`): draws each weight
  uniformly on `[−L, L]` with `L = √(6/(fan_in+fan_out))`. A uniform on `[−L,L]`
  has variance `L²/3 = 2/(fan_in+fan_out)`, so the target standard deviation is
  `√(2/(fan_in+fan_out))`. The program computes the empirical mean and standard
  deviation of the generated weights and prints them next to that expected value.
- **He/Kaiming initialisation** (`quantum_he_init`): draws each weight from a
  Gaussian (via Box-Muller) with standard deviation `√(2/fan_in)`, the standard
  choice for ReLU networks.
- **Dropout mask** (`generate_dropout_mask`): keeps each unit with probability
  `1 − rate`; the demo averages over 1,000 masks and checks the active rate lands
  near the expected 0.5.

**What it demonstrates.** For a 784→512 layer, a representative run prints Xavier
measured standard deviation ≈ 0.0393 against an expected 0.0393 — i.e. the
quantum-seeded weights reproduce the theoretical initialisation variance. It then
reports He statistics and the dropout active rate, and finally constructs a
4→8→8→2 network (He-initialised hidden weights, small quantum biases) and prints
one forward pass in training mode (dropout on, with inverted-dropout rescaling)
and one in inference mode (dropout off).

**Note on the header.** `neural_init.h` was rewritten to match the `.c`: it
declares exactly the structs (`neuron_t`, `layer_t`, `network_t`) and functions
(`quantum_xavier_init`, `quantum_he_init`, `init_network`,
`generate_dropout_mask`, `forward_pass`, `relu`, `relu_derivative`, the two
demonstration entry points) that the implementation defines, with the momentum
buffers the code allocates. There are no stale or missing declarations.

**Caveat.** This is initialisation and inference only — there is no training loop
here. The momentum buffers are allocated for realism but not exercised; ReLU on
the output layer means the tiny demo network's outputs are non-negative.

## `quantum_gan.c` (has `main`)

**What it does.** Trains a small **Generative Adversarial Network** to reproduce
a 2-D target distribution: points scattered on a ring (a noisy circle). A
generator tries to turn random latent vectors into ring-like points while a
discriminator learns to tell real ring points from generated ones; the two train
against each other.

```
make quantum_gan
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib ./quantum_gan
```

**The architecture and math.** Both networks are two-layer MLPs (hidden dim 16).
The generator maps a 10-D latent vector (sampled as standard normals via
Box-Muller) through ReLU to a `tanh` output in `[−1,1]²`; the discriminator maps
a 2-D point through ReLU to a sigmoid probability. Weights use Xavier scaling.
Real samples are drawn on a ring of radius uniform in `[0.8, 1.0]` at a random
angle. Training runs 2,000 epochs of 32 mini-steps with separate learning rates
(discriminator 5e-4, generator 2e-3).

**The backpropagation is done properly.** The discriminator is trained with
binary cross-entropy, where the combination of the BCE derivative and the sigmoid
derivative collapses to a clean pre-activation gradient of `D − target`. The
generator is trained with the **non-saturating** loss `−log D(G(z))`: the code
backpropagates that loss *through the discriminator* (using the discriminator's
gradient buffers as scratch, without updating its weights) to obtain the true
gradient with respect to the generated sample, then continues the chain rule back
through the generator. Probabilities are clamped away from 0 and 1 so the logs
stay finite. This is the correct GAN gradient, not a hand-waved surrogate.

**What you see, honestly.** The program prints losses and sample points every 400
epochs, then a final batch of samples with their discriminator probabilities, and
finally the mean and standard deviation of the generated points' radius next to
the target ring's (mean 0.900, range [0.8, 1.0]). Whether the generator actually
converges onto the ring varies with the initial conditions: because the model is
tiny and GAN training is notoriously unstable, some starting states collapse
instead of matching the ring — an observed run finished with mean generated
radius ≈ 0.32 rather than ≈ 0.90. (The generator seeds itself from hardware
entropy by default, so successive runs start differently; the underlying RNG
supports seeded, reproducible runs when a seed is supplied.) The value of the
example is the correct adversarial training machinery and the
built-in radius diagnostic that lets you *see* how close it got, not a guaranteed
clean ring.

## `quantum_transformer.c` (has `main`)

**What it does.** A minimal, initialise-and-run **transformer** text generator.
It builds a small multi-layer transformer, seeds all of its weights from the
quantum RNG, and autoregressively generates a sequence of token IDs from a short
prompt using temperature sampling. It shows how a transformer's forward pass and
sampling loop fit together.

```
make quantum_transformer
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib ./quantum_transformer
```

**The architecture and math.** Dimensions: vocabulary 1000, embedding 64, 4
attention heads of head-dim 16, feed-forward dim 256, 3 layers, max sequence 128.
All weight matrices are initialised via Box-Muller Gaussians scaled by
`√(2/(rows+cols))` (Xavier-style). The forward pass is a standard transformer
block: token embeddings plus positional embeddings; **scaled dot-product
self-attention** per head (`softmax(QKᵀ/√d) · V`) with a small quantum-noise term
added to the scores for exploration; head outputs averaged and combined; a
residual connection and layer normalisation; a ReLU feed-forward network; then a
second residual-plus-layernorm. A final projection maps to vocabulary logits.

**Token generation.** `sample_token` divides the final-position logits by a
temperature (0.8), applies softmax, and does **inverse-CDF sampling** with a
quantum uniform: it walks the cumulative distribution and returns the first token
whose cumulative probability exceeds the draw, falling back to the last token if
floating-point rounding leaves the draw at the very top (rather than silently
biasing toward token 0). Generation stops at the assumed end token (ID 1) or when
the length budget is reached; the sequence-length guard prevents overrunning the
positional-embedding table.

**What you see.** The program prints the prompt token IDs and then the generated
token IDs as they are produced. Because the model is randomly initialised and
never trained, the "text" is a stream of token IDs with no linguistic meaning —
the example demonstrates the transformer *mechanics* (attention, residuals,
layer norm, temperature sampling driven by quantum randomness), not a trained
language model. It is `O(seq_len²)` in attention and recomputes the full forward
pass for each new token, which is fine for this toy size.

---

Part of the Quantum RNG examples. The full quantum-simulation toolkit and the
continued Bell-verified RNG live in Moonlab:
https://github.com/tsotchke/moonlab
