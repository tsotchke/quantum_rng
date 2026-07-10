# Network examples

These two programs use the quantum random number generator to drive network
simulations: one builds a random-but-connected topology and chooses routes
through it, the other pushes a stream of packets across a network in real time
and watches how congestion, bursts, and drops play out. In both, the RNG decides
link placement, link quality, per-hop failures, source/destination pairs, and
traffic bursts, so the "network" is a fresh random world each run rather than a
hand-drawn diagram.

Both link the lower-level `qrng_*` API directly (`qrng_init`, `qrng_double`,
`qrng_range32`, `qrng_uint64`, and in the traffic simulator also
`qrng_entangle_states` and `qrng_get_entropy_estimate`), not the higher-level
`secure_rng` interface. That quantum core is a real 8-qubit state-vector
simulation; the CHSH Bell test used elsewhere in the repo is what verifies it.
For production randomness the recommended entry point is `secure_rng` (hardware
entropy + NIST SP 800-90B health tests + quantum mixing, with a VERIFIED mode
that runs a real Bell test before serving bytes); these demos use the raw core
because they only need a good stochastic driver, not cryptographic guarantees.

## Building and running

Build the library once from the repo root:

```
make
```

Then build either example by its Makefile target, or everything at once:

```
make quantum_routing
make traffic_sim
make examples_all
```

Binaries land in the repo root; run them from there. The examples link the
library, which is built with OpenMP, so on macOS expose libomp at runtime:

```
DYLD_LIBRARY_PATH=$(brew --prefix libomp)/lib ./quantum_routing
```

---

## quantum_routing

**In one sentence:** it wires up a random network, finds the possible paths
between two points, and picks a route the way a smart router would, favoring
reliable, fast, uncongested paths while still spreading traffic around.

**What it does (technical).** Builds a network of up to 100 nodes where each
possible edge is included with a tunable probability, using quantum randomness
for link presence, per-link latency (1-20 ms), per-node capacity, and per-node
failure probability (0-2%). Connectivity is guaranteed: any node left isolated
is attached to an earlier node, so by induction the whole graph is reachable
from node 0. For each source/destination pair it enumerates simple paths with a
depth-first search (bounded by `MAX_PATHS`), scores each route by
`reliability / ((1 + latency) * (1 + load))`, and then samples a route
**quantum-randomly in proportion to those weights** rather than always taking
the single best one. Each simulated step routes a batch of packets with per-hop
failure and congestion checks, accumulates delivered/dropped counts and latency,
then drains node load with a quantum-jittered rate.

**What it teaches.** Weighted probabilistic route selection (load-balancing that
avoids hammering one "best" path), how reliability compounds multiplicatively
along a path, and how a guaranteed-connected random graph is constructed. This
file was previously a stub; it is now a working simulation. A verification run
(12 nodes, 200 steps, batch 20) delivered ~3710 packets with ~7% loss and an
average latency near 92 ms, and reported per-node load and peak load.

**How to run.** Has `main(int argc, char *argv[])` with a getopt parser:

- `-n, --nodes N` (2-100, default 10)
- `-t, --time N` simulation steps (default 1000)
- `-b, --batch N` packets per step (default 100)
- `-c, --connectivity N` edge probability 0.0-1.0 for the `random` topology (default 0.7)
- `-q, --quantum N` quantum factor 0.0-1.0 (default 1.0) — see below
- `-f, --topology NAME` network shape: `random`, `mesh`, `ring`, `star`, `line` (default `random`)
- `-v` verbose, `-V` visualize, `-j` JSON output, `-P` hide progress
- `-o, --output FILE` write output to a file
- `-h, --help`

Example: `./quantum_routing -n 20 -t 2000 -b 50 -q 0.8 -f ring -v`. JSON mode
(`-j`) emits aggregate statistics plus a per-node array.

**The `-q` quantum factor** blends two route-selection strategies. Each route is
scored by `reliability / ((1 + latency) * (1 + load))`. With probability `1 - q`
the router takes the single highest-scoring route (deterministic best path);
otherwise it samples routes quantum-randomly in proportion to their scores. So
`-q 0.0` always takes the best path, and `-q 1.0` is full weighted roulette,
which spreads load across the network. The denominators use `1 + ...`, so there
is no division by zero.

**The `-f` topology** selects the generated wiring: `random` (Erdős–Rényi at the
`-c` density), `mesh` (fully connected), `ring` (a cycle), `star` (hub at node 0),
or `line` (a path). Every topology is guaranteed connected, and an unknown name
exits with an error. Verified link counts on 12 nodes: ring 12, star 11, mesh 66.

Path finding is an exhaustive depth-first search capped at `MAX_PATHS` (10)
routes, so on dense graphs it considers a bounded subset of all simple paths.
"Latency" and "capacity" are dimensionless model units.

---

## traffic_sim

**In one sentence:** it runs a live packet-forwarding simulation for a few real
seconds, sending packets across a random network and reporting how many arrived,
how many were dropped, and how congested things got.

**What it does (technical).** Generates a proximity-plus-quantum-randomness
topology (nodes placed on a 100x100 grid, links more likely between nearby
nodes), again with a connectivity guarantee so every node is reachable. It then
runs a store-and-forward simulation in wall-clock time: each packet is routed
along the shortest hop path found by breadth-first search, consuming bandwidth
and queue slots at every hop, with drops on missing routes, full queues, or
exhausted bandwidth. QoS assigns priority by protocol (ICMP > UDP > TCP, with a
bump for entangled packets), traffic bursts scale the offered load and decay
back to normal, connection bandwidth recovers and picks up quantum noise each
tick, and an anomaly detector flags steps whose quantum entropy estimate falls
below a threshold. Optionally it creates "entangled" packet pairs via
`qrng_entangle_states` that get priority scheduling.

**What it teaches.** The mechanics of a store-and-forward network under load:
BFS shortest-path routing, per-hop queue and bandwidth accounting, QoS
prioritization, and burst dynamics. It was previously a stub and is now a
working simulator. A verification run (`-t 2 -q`, 12 nodes, 200 pkt/s) delivered
~331 packets with 0 drops, 62 entangled packets, and an average latency near
38 ms.

**How to run.** Has `main(int argc, char *argv[])` with a getopt parser. It runs
for `-t` **real seconds** (wall-clock), sleeping 100 ms per tick, so keep `-t`
small for a quick look:

- `-t, --time N` duration in seconds (default 5) — use `-t 2` for a fast run
- `-r, --rate N` packets per second (default 200)
- `-n, --nodes N` (2-100, default 12)
- `-p, --protocol` tcp | udp | icmp (default tcp)
- `-e, --entangle RATE` entangled-pair rate 0.0-1.0 (default 0.1)
- `-b, --bursts PROB` burst probability 0.0-1.0 (default 0.2)
- `-a, --anomaly THRESH` anomaly entropy threshold (default 0.5)
- `-f, --topology FILE` load a network from a topology file (see format below)
- `-Q, --no-qos` disable QoS priorities
- `-q, --quiet` disable the live visualization
- `-h, --help`

Quick command: `./traffic_sim -t 2 -q -n 12 -r 200`. Without `-q`, it draws a
live status line and an ASCII topology/quantum-state view.

**Loading a topology from file (`-f`).** Instead of a generated network you can
supply a text file. A working sample, `sample_topology.txt` (6 nodes, 7 edges),
ships in this directory. The format is:

```
nodes <count>                 # 2..100, required, must appear first
node  <id> <x> <y>            # optional position for the ASCII view
edge  <src> <dst> [bw] [lat]  # undirected; bandwidth (Mbps) and latency (ms) optional
```

Blank lines and `#` comments are ignored. Every field is bounds-checked: a
missing or out-of-range `nodes` count, a `node`/`edge` before `nodes`, an
endpoint out of range, a self-loop, a duplicate edge, or an unreadable file each
prints the offending `file:line` and exits with an error — there is no silent
fallback. Run it with `./traffic_sim -t 2 -q -f examples/network/sample_topology.txt`.

Because the loop is timed against wall-clock seconds, total work depends on
machine speed and the 100 ms tick, so exact packet counts vary run to run. The
entanglement and anomaly features are illustrative uses of the quantum core, not
a security mechanism. Network state is held in file-scope globals, so one process
runs one simulation at a time.

---

Part of the Quantum RNG examples. The full quantum-simulation toolkit and the
continued Bell-verified RNG live in Moonlab:
https://github.com/tsotchke/moonlab
