#ifndef QUANTUM_ROUTING_H
#define QUANTUM_ROUTING_H

#include "../../src/quantum_rng/quantum_rng.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_NODES 100
#define MAX_PATHS 10
#define SIMULATION_TIME 1000
#define PACKET_BATCH 100

typedef struct {
    int id;
    double load;
    double capacity;
    double failure_prob;
    int num_connections;
    int connections[MAX_NODES];
    double latencies[MAX_NODES];
} node_t;

typedef struct {
    node_t nodes[MAX_NODES];
    int num_nodes;
    double total_throughput;
    double avg_latency;
    int dropped_packets;
} network_t;

typedef struct {
    int path[MAX_NODES];
    int length;
    double latency;
    double reliability;
} route_t;

// Selectable topology presets for the generated network. All of them are
// guaranteed to produce a connected graph.
typedef enum {
    TOPO_RANDOM,  // Erdos-Renyi: each edge present with connection probability
    TOPO_MESH,    // Fully connected: every pair of nodes is linked
    TOPO_RING,    // Each node linked to its two neighbours in a cycle
    TOPO_STAR,    // Every node linked to hub node 0
    TOPO_LINE     // Nodes linked in a single chain (path graph)
} topology_type_t;

// Network initialization. `topology` selects the wiring pattern; for
// TOPO_RANDOM the edge density is controlled by connection_probability
// (ignored by the deterministic topologies).
void init_network(qrng_ctx *ctx, network_t *net, int num_nodes,
                 double connection_probability, topology_type_t topology);

// Path finding (depth-first search; call with visited[] zeroed,
// current_path[0] = source and path_len = 1)
void find_paths(const network_t *net, int current, int dest,
               route_t *routes, int *num_routes,
               int *visited, int *current_path, int path_len);

// Route selection. `quantum_factor` (0.0-1.0) blends between deterministic
// best-path routing (0.0: always take the single highest-weight route) and
// quantum-roulette selection (1.0: sample routes in proportion to their
// weights, spreading load across good paths).
int select_route(qrng_ctx *ctx, const route_t *routes, int num_routes,
                const network_t *net, double quantum_factor);

// Network operations
void update_loads(qrng_ctx *ctx, network_t *net);
void route_packets(qrng_ctx *ctx, network_t *net,
                  int source, int dest, int num_packets,
                  double quantum_factor);
void print_network_stats(const network_t *net);

#endif // QUANTUM_ROUTING_H
