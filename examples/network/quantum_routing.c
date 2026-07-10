#include "quantum_routing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>

typedef struct {
    int num_nodes;
    int simulation_time;
    int packet_batch;
    double connection_probability;
    double quantum_factor;
    bool verbose;
    bool visualize;
    bool json_output;
    bool show_progress;
    topology_type_t topology;
    char output_file[256];
} routing_config_t;

// Map a topology name to its enum value; returns -1 on an unknown name.
static int parse_topology_name(const char *name) {
    if (strcmp(name, "random") == 0) return TOPO_RANDOM;
    if (strcmp(name, "mesh")   == 0) return TOPO_MESH;
    if (strcmp(name, "ring")   == 0) return TOPO_RING;
    if (strcmp(name, "star")   == 0) return TOPO_STAR;
    if (strcmp(name, "line")   == 0) return TOPO_LINE;
    return -1;
}

static const char *topology_name(topology_type_t t) {
    switch (t) {
        case TOPO_RANDOM: return "random";
        case TOPO_MESH:   return "mesh";
        case TOPO_RING:   return "ring";
        case TOPO_STAR:   return "star";
        case TOPO_LINE:   return "line";
    }
    return "unknown";
}

void init_routing_config(routing_config_t *config) {
    config->num_nodes = 10;
    config->simulation_time = SIMULATION_TIME;
    config->packet_batch = PACKET_BATCH;
    config->connection_probability = 0.7;
    config->quantum_factor = 1.0;
    config->verbose = false;
    config->visualize = false;
    config->json_output = false;
    config->show_progress = true;
    config->topology = TOPO_RANDOM;
    config->output_file[0] = '\0';
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -n, --nodes N           Number of nodes (2-%d, default: 10)\n", MAX_NODES);
    printf("  -t, --time N            Simulation time in steps (default: %d)\n", SIMULATION_TIME);
    printf("  -b, --batch N           Packet batch size (default: %d)\n", PACKET_BATCH);
    printf("  -c, --connectivity N    Connection probability for 'random' topology (0.0-1.0, default: 0.7)\n");
    printf("  -q, --quantum N         Quantum routing factor (0.0-1.0, default: 1.0):\n");
    printf("                          0.0 = always take the best route (deterministic),\n");
    printf("                          1.0 = sample routes by weight (spread load)\n");
    printf("  -f, --topology NAME     Topology preset: random, mesh, ring, star, line (default: random)\n");
    printf("  -v, --verbose           Show detailed output\n");
    printf("  -V, --visualize         Enable network visualization\n");
    printf("  -j, --json              Output results in JSON format\n");
    printf("  -P, --no-progress       Hide progress bar\n");
    printf("  -o, --output FILE       Write output to file\n");
    printf("  -h, --help              Show this help message\n\n");
    printf("Example:\n");
    printf("  %s -n 20 -t 2000 -b 50 -q 0.8 -v\n", program_name);
    printf("  %s --nodes 15 --quantum 0.9 --visualize --json\n", program_name);
}

void parse_args(int argc, char *argv[], routing_config_t *config) {
    static struct option long_options[] = {
        {"nodes",        required_argument, 0, 'n'},
        {"time",         required_argument, 0, 't'},
        {"batch",        required_argument, 0, 'b'},
        {"connectivity", required_argument, 0, 'c'},
        {"quantum",      required_argument, 0, 'q'},
        {"topology",     required_argument, 0, 'f'},
        {"verbose",      no_argument,       0, 'v'},
        {"visualize",    no_argument,       0, 'V'},
        {"json",         no_argument,       0, 'j'},
        {"no-progress",  no_argument,       0, 'P'},
        {"output",       required_argument, 0, 'o'},
        {"help",         no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "n:t:b:c:q:f:vVjPo:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'n':
                config->num_nodes = atoi(optarg);
                if (config->num_nodes < 2 || config->num_nodes > MAX_NODES) {
                    fprintf(stderr, "Error: Number of nodes must be between 2 and %d\n", MAX_NODES);
                    exit(1);
                }
                break;

            case 't':
                config->simulation_time = atoi(optarg);
                if (config->simulation_time <= 0) {
                    fprintf(stderr, "Error: Simulation time must be positive\n");
                    exit(1);
                }
                break;

            case 'b':
                config->packet_batch = atoi(optarg);
                if (config->packet_batch <= 0) {
                    fprintf(stderr, "Error: Packet batch size must be positive\n");
                    exit(1);
                }
                break;

            case 'c':
                config->connection_probability = atof(optarg);
                if (config->connection_probability < 0.0 || config->connection_probability > 1.0) {
                    fprintf(stderr, "Error: Connection probability must be between 0.0 and 1.0\n");
                    exit(1);
                }
                break;

            case 'q':
                config->quantum_factor = atof(optarg);
                if (config->quantum_factor < 0.0 || config->quantum_factor > 1.0) {
                    fprintf(stderr, "Error: Quantum factor must be between 0.0 and 1.0\n");
                    exit(1);
                }
                break;

            case 'f': {
                int t = parse_topology_name(optarg);
                if (t < 0) {
                    fprintf(stderr, "Error: unknown topology '%s' "
                            "(expected: random, mesh, ring, star, line)\n", optarg);
                    exit(1);
                }
                config->topology = (topology_type_t)t;
                break;
            }

            case 'v':
                config->verbose = true;
                break;

            case 'V':
                config->visualize = true;
                break;

            case 'j':
                config->json_output = true;
                break;

            case 'P':
                config->show_progress = false;
                break;

            case 'o':
                strncpy(config->output_file, optarg, sizeof(config->output_file) - 1);
                break;

            case 'h':
                print_usage(argv[0]);
                exit(0);

            case '?':
                exit(1);

            default:
                fprintf(stderr, "Error: Unknown option %c\n", c);
                exit(1);
        }
    }
}

// ---------------------------------------------------------------------------
// Core network implementation
// ---------------------------------------------------------------------------

// Look up the latency of the direct link a -> b (returns < 0 if none)
static double link_latency(const network_t *net, int a, int b) {
    const node_t *node = &net->nodes[a];
    for (int i = 0; i < node->num_connections; i++) {
        if (node->connections[i] == b) {
            return node->latencies[i];
        }
    }
    return -1.0;
}

static void add_link(qrng_ctx *ctx, network_t *net, int a, int b) {
    double latency = 1.0 + qrng_double(ctx) * 19.0;  // 1-20 ms
    node_t *na = &net->nodes[a];
    node_t *nb = &net->nodes[b];
    na->connections[na->num_connections] = b;
    na->latencies[na->num_connections] = latency;
    na->num_connections++;
    nb->connections[nb->num_connections] = a;
    nb->latencies[nb->num_connections] = latency;
    nb->num_connections++;
}

// Is there already a direct link between a and b? (avoids duplicate edges)
static bool linked(const network_t *net, int a, int b) {
    return link_latency(net, a, b) >= 0.0;
}

// Can we safely add another edge to node n without overflowing its arrays?
static bool has_capacity(const network_t *net, int n) {
    return net->nodes[n].num_connections < MAX_NODES - 1;
}

void init_network(qrng_ctx *ctx, network_t *net, int num_nodes,
                 double connection_probability, topology_type_t topology) {
    memset(net, 0, sizeof(*net));
    net->num_nodes = num_nodes;

    for (int i = 0; i < num_nodes; i++) {
        node_t *node = &net->nodes[i];
        node->id = i;
        node->load = 0.0;
        node->capacity = 100.0 + qrng_double(ctx) * 900.0;  // packets/step
        node->failure_prob = qrng_double(ctx) * 0.02;       // 0-2% per hop
        node->num_connections = 0;
    }

    switch (topology) {
        case TOPO_MESH:
            // Fully connected: link every pair (already connected by design).
            for (int j = 1; j < num_nodes; j++) {
                for (int i = 0; i < j; i++) {
                    if (has_capacity(net, i) && has_capacity(net, j)) {
                        add_link(ctx, net, i, j);
                    }
                }
            }
            break;

        case TOPO_RING:
            // Chain the nodes, then close the loop (needs >= 3 nodes to be a
            // true ring; 2 nodes collapse to a single link).
            for (int i = 0; i + 1 < num_nodes; i++) {
                add_link(ctx, net, i, i + 1);
            }
            if (num_nodes > 2) {
                add_link(ctx, net, num_nodes - 1, 0);
            }
            break;

        case TOPO_STAR:
            // Hub node 0 connects to every other node.
            for (int i = 1; i < num_nodes; i++) {
                add_link(ctx, net, 0, i);
            }
            break;

        case TOPO_LINE:
            // Single path graph: 0 - 1 - 2 - ... - (n-1).
            for (int i = 0; i + 1 < num_nodes; i++) {
                add_link(ctx, net, i, i + 1);
            }
            break;

        case TOPO_RANDOM:
        default:
            // Erdos-Renyi style: include each edge with the given probability,
            // using quantum randomness for the coin flip.
            for (int j = 1; j < num_nodes; j++) {
                for (int i = 0; i < j; i++) {
                    if (has_capacity(net, i) && has_capacity(net, j) &&
                        qrng_double(ctx) < connection_probability) {
                        add_link(ctx, net, i, j);
                    }
                }
                // Guarantee connectivity: attach isolated nodes to an
                // earlier node, so by induction node 0 reaches everything.
                if (net->nodes[j].num_connections == 0) {
                    add_link(ctx, net, qrng_range32(ctx, 0, j - 1), j);
                }
            }
            break;
    }

    // Safety net: no matter the topology, ensure every node past 0 has at
    // least one link back into the already-connected component. This is a
    // no-op for the connected presets above but protects against edge cases
    // (e.g. a capacity limit having skipped a link).
    for (int j = 1; j < num_nodes; j++) {
        bool reachable = false;
        for (int i = 0; i < j; i++) {
            if (linked(net, i, j)) { reachable = true; break; }
        }
        if (!reachable && has_capacity(net, j)) {
            int target = qrng_range32(ctx, 0, j - 1);
            if (has_capacity(net, target)) {
                add_link(ctx, net, target, j);
            }
        }
    }
}

void find_paths(const network_t *net, int current, int dest,
               route_t *routes, int *num_routes,
               int *visited, int *current_path, int path_len) {
    if (*num_routes >= MAX_PATHS || path_len >= MAX_NODES) {
        return;
    }

    if (current == dest) {
        route_t *route = &routes[*num_routes];
        route->length = path_len;
        route->latency = 0.0;
        route->reliability = 1.0;
        for (int i = 0; i < path_len; i++) {
            route->path[i] = current_path[i];
            route->reliability *= 1.0 - net->nodes[current_path[i]].failure_prob;
            if (i > 0) {
                route->latency += link_latency(net, current_path[i-1],
                                               current_path[i]);
            }
        }
        (*num_routes)++;
        return;
    }

    visited[current] = 1;
    const node_t *node = &net->nodes[current];
    for (int i = 0; i < node->num_connections && *num_routes < MAX_PATHS; i++) {
        int next = node->connections[i];
        if (!visited[next]) {
            current_path[path_len] = next;
            find_paths(net, next, dest, routes, num_routes,
                       visited, current_path, path_len + 1);
        }
    }
    visited[current] = 0;
}

int select_route(qrng_ctx *ctx, const route_t *routes, int num_routes,
                const network_t *net, double quantum_factor) {
    if (num_routes <= 0) {
        return -1;
    }

    // Weight each route by reliability, inverse latency and inverse load. A
    // higher weight is a "better" route (more reliable, faster, less loaded).
    double weights[MAX_PATHS];
    double total = 0.0;
    int best = 0;
    for (int i = 0; i < num_routes; i++) {
        double avg_load = 0.0;
        for (int j = 0; j < routes[i].length; j++) {
            avg_load += net->nodes[routes[i].path[j]].load;
        }
        avg_load /= routes[i].length;
        weights[i] = routes[i].reliability /
                     ((1.0 + routes[i].latency) * (1.0 + avg_load));
        total += weights[i];
        if (weights[i] > weights[best]) {
            best = i;
        }
    }

    if (total <= 0.0) {
        return qrng_range32(ctx, 0, num_routes - 1);
    }

    // quantum_factor blends the two selection styles. With probability
    // (1 - quantum_factor) we act deterministically and take the single
    // highest-weight route; otherwise we sample quantum-randomly in
    // proportion to the weights, spreading traffic across good paths. So
    // factor 0.0 -> always best path, factor 1.0 -> full roulette.
    if (quantum_factor < 1.0 && qrng_double(ctx) >= quantum_factor) {
        return best;
    }

    double r = qrng_double(ctx) * total;
    double cumsum = 0.0;
    for (int i = 0; i < num_routes; i++) {
        cumsum += weights[i];
        if (r < cumsum) {
            return i;
        }
    }
    return num_routes - 1;  // Floating-point rounding fallback
}

void route_packets(qrng_ctx *ctx, network_t *net,
                  int source, int dest, int num_packets,
                  double quantum_factor) {
    route_t routes[MAX_PATHS];
    int num_routes = 0;
    int visited[MAX_NODES] = {0};
    int current_path[MAX_NODES];

    current_path[0] = source;
    find_paths(net, source, dest, routes, &num_routes, visited,
               current_path, 1);

    if (num_routes == 0) {
        net->dropped_packets += num_packets;
        return;
    }

    for (int p = 0; p < num_packets; p++) {
        int r = select_route(ctx, routes, num_routes, net, quantum_factor);
        const route_t *route = &routes[r];

        // Per-hop quantum failure and congestion checks
        bool delivered = true;
        for (int i = 0; i < route->length; i++) {
            node_t *node = &net->nodes[route->path[i]];
            if (qrng_double(ctx) < node->failure_prob || node->load >= 1.0) {
                delivered = false;
                break;
            }
        }

        if (delivered) {
            for (int i = 0; i < route->length; i++) {
                node_t *node = &net->nodes[route->path[i]];
                node->load += 1.0 / node->capacity;
            }
            net->total_throughput += 1.0;
            net->avg_latency += route->latency;  // Converted to mean on output
        } else {
            net->dropped_packets++;
        }
    }
}

void update_loads(qrng_ctx *ctx, network_t *net) {
    for (int i = 0; i < net->num_nodes; i++) {
        node_t *node = &net->nodes[i];
        // Traffic drains between steps, with quantum jitter in the drain rate
        double drain = 0.5 + qrng_double(ctx) * 0.3;
        node->load *= (1.0 - drain);
        if (node->load < 0.0) {
            node->load = 0.0;
        }
    }
}

void print_network_stats(const network_t *net) {
    double total_load = 0.0;
    double max_load = 0.0;
    int total_connections = 0;
    for (int i = 0; i < net->num_nodes; i++) {
        total_load += net->nodes[i].load;
        if (net->nodes[i].load > max_load) {
            max_load = net->nodes[i].load;
        }
        total_connections += net->nodes[i].num_connections;
    }

    long attempted = (long)net->total_throughput + net->dropped_packets;
    printf("  Nodes: %d, Links: %d\n", net->num_nodes, total_connections / 2);
    printf("  Delivered packets: %.0f\n", net->total_throughput);
    printf("  Dropped packets: %d (%.2f%%)\n", net->dropped_packets,
           attempted > 0 ? 100.0 * net->dropped_packets / attempted : 0.0);
    printf("  Average latency: %.2f ms\n",
           net->total_throughput > 0 ?
           net->avg_latency / net->total_throughput : 0.0);
    printf("  Average node load: %.1f%%, peak: %.1f%%\n",
           100.0 * total_load / net->num_nodes, 100.0 * max_load);
}

void output_json_stats(const network_t *net, FILE *output) {
    fprintf(output, "{\n");
    fprintf(output, "  \"statistics\": {\n");
    fprintf(output, "    \"total_throughput\": %.2f,\n", net->total_throughput);
    fprintf(output, "    \"average_latency\": %.2f,\n",
            net->total_throughput > 0 ? net->avg_latency / net->total_throughput : 0);
    fprintf(output, "    \"dropped_packets\": %d,\n", net->dropped_packets);
    double attempted = net->total_throughput + net->dropped_packets;
    fprintf(output, "    \"packet_loss_rate\": %.2f\n",
            attempted > 0 ? 100.0 * net->dropped_packets / attempted : 0.0);
    fprintf(output, "  },\n");
    
    fprintf(output, "  \"nodes\": [\n");
    for (int i = 0; i < net->num_nodes; i++) {
        const node_t *node = &net->nodes[i];
        fprintf(output, "    {\n");
        fprintf(output, "      \"id\": %d,\n", node->id);
        fprintf(output, "      \"load\": %.2f,\n", node->load * 100.0);
        fprintf(output, "      \"capacity\": %.0f,\n", node->capacity);
        fprintf(output, "      \"connections\": %d\n", node->num_connections);
        fprintf(output, "    }%s\n", i < net->num_nodes - 1 ? "," : "");
    }
    fprintf(output, "  ]\n");
    fprintf(output, "}\n");
}

static void demonstrate_routing(const routing_config_t *config) {
    qrng_ctx *ctx;
    if (qrng_init(&ctx, (uint8_t*)"routing", 7) != QRNG_SUCCESS) {
        fprintf(stderr, "Error: failed to initialize quantum RNG\n");
        exit(1);
    }
    
    FILE *output = config->output_file[0] ? fopen(config->output_file, "w") : stdout;
    if (!output) {
        fprintf(stderr, "Error: Could not open output file '%s'\n", config->output_file);
        qrng_free(ctx);
        return;
    }

    if (!config->json_output) {
        fprintf(output, "Quantum Network Routing Demo\n");
        fprintf(output, "===========================\n\n");
        fprintf(output, "Configuration:\n");
        fprintf(output, "  Nodes: %d\n", config->num_nodes);
        fprintf(output, "  Simulation Time: %d steps\n", config->simulation_time);
        fprintf(output, "  Packet Batch: %d\n", config->packet_batch);
        fprintf(output, "  Topology: %s\n", topology_name(config->topology));
        fprintf(output, "  Quantum Factor: %.2f (0=best-path, 1=weighted roulette)\n\n",
                config->quantum_factor);
    }

    // Initialize network
    network_t net;
    init_network(ctx, &net, config->num_nodes, config->connection_probability,
                 config->topology);
    
    if (!config->json_output) {
        fprintf(output, "Initial Network State:\n");
        print_network_stats(&net);
    }
    
    // Simulate traffic
    if (config->verbose && !config->json_output) {
        fprintf(output, "\nSimulating network traffic...\n");
    }

    for(int t = 0; t < config->simulation_time; t++) {
        // Generate random source-destination pairs
        int source = qrng_uint64(ctx) % net.num_nodes;
        int dest;
        do {
            dest = qrng_uint64(ctx) % net.num_nodes;
        } while(dest == source);
        
        // Route batch of packets
        route_packets(ctx, &net, source, dest, config->packet_batch,
                      config->quantum_factor);
        
        // Update network state
        update_loads(ctx, &net);
        
        // Print progress (interval guarded against short simulations)
        if(config->show_progress && !config->json_output) {
            int interval = config->simulation_time / 10;
            if (interval < 1) interval = 1;
            if((t + 1) % interval == 0) {
                fprintf(output, "\nTime Step %d:\n", t + 1);
                if (config->verbose) {
                    print_network_stats(&net);
                }
            }
        }
    }
    
    if (config->json_output) {
        output_json_stats(&net, output);
    } else {
        fprintf(output, "\nFinal Network State:\n");
        print_network_stats(&net);
    }
    
    if (output != stdout) {
        fclose(output);
    }
    qrng_free(ctx);
}

int main(int argc, char *argv[]) {
    routing_config_t config;
    init_routing_config(&config);
    parse_args(argc, argv, &config);
    demonstrate_routing(&config);
    return 0;
}
