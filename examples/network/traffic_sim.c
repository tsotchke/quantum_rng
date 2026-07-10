#include "traffic_sim.h"
#include "../../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>

// Helper functions - declare early
static int min(int a, int b) {
    return a < b ? a : b;
}

static int max(int a, int b) {
    return a > b ? a : b;
}

// Global state
static node_t nodes[MAX_NODES];
static connection_t connections[MAX_CONNECTIONS];
static int num_nodes = 0;
static int num_connections = 0;
static traffic_stats_t current_stats = {0};
static uint8_t entangled_states[MAX_ENTANGLED_PAIRS][64];
static int num_entangled_pairs = 0;
static config_t *global_config = NULL;  // Store config globally for anomaly detection

void generate_quantum_topology(qrng_ctx *ctx, int requested_nodes) {
    num_nodes = requested_nodes;
    num_connections = 0;
    
    // Generate node positions using quantum randomness
    for (int i = 0; i < num_nodes; i++) {
        nodes[i].id = i;
        nodes[i].x = qrng_double(ctx) * 100.0;  // Scale to 100x100 grid
        nodes[i].y = qrng_double(ctx) * 100.0;
        nodes[i].num_connections = 0;
        
        // Generate quantum state for node
        qrng_bytes(ctx, nodes[i].quantum_state, sizeof(nodes[i].quantum_state));
    }
    
    // Create connections based on proximity and quantum randomness
    for (int j = 1; j < num_nodes; j++) {
        for (int i = 0; i < j; i++) {
            // Calculate distance between nodes
            double dx = nodes[i].x - nodes[j].x;
            double dy = nodes[i].y - nodes[j].y;
            double distance = sqrt(dx*dx + dy*dy);
            
            // Quantum probability of connection
            if (qrng_double(ctx) < exp(-distance/30.0) && num_connections < MAX_CONNECTIONS) {
                connection_t *conn = &connections[num_connections];
                conn->id = num_connections;
                conn->source = i;
                conn->destination = j;
                
                // Use quantum randomness for connection properties
                conn->bandwidth = 500.0 + qrng_double(ctx) * 1000.0;  // 500-1500 Mbps
                conn->latency = 5.0 + qrng_double(ctx) * 20.0;       // 5-25ms
                conn->queue_size = 0;
                conn->qos_enabled = true;
                conn->quantum_noise = qrng_double(ctx) * 0.1;        // 0-10% noise
                
                // Update node connections
                nodes[i].connections[nodes[i].num_connections++] = j;
                nodes[j].connections[nodes[j].num_connections++] = i;
                
                num_connections++;
            }
        }

        // Guarantee global connectivity: if node j has no link to any
        // earlier node, attach it to a quantum-randomly chosen one. By
        // induction every node is then reachable from node 0.
        bool linked = false;
        for (int c = 0; c < num_connections && !linked; c++) {
            if (connections[c].source == j || connections[c].destination == j) {
                linked = true;
            }
        }
        if (!linked && num_connections < MAX_CONNECTIONS) {
            int i = qrng_range32(ctx, 0, j - 1);
            connection_t *conn = &connections[num_connections];
            conn->id = num_connections;
            conn->source = i;
            conn->destination = j;
            conn->bandwidth = 500.0 + qrng_double(ctx) * 1000.0;
            conn->latency = 5.0 + qrng_double(ctx) * 20.0;
            conn->queue_size = 0;
            conn->qos_enabled = true;
            conn->quantum_noise = qrng_double(ctx) * 0.1;
            nodes[i].connections[nodes[i].num_connections++] = j;
            nodes[j].connections[nodes[j].num_connections++] = i;
            num_connections++;
        }
    }
}

void create_entangled_packets(qrng_ctx *ctx, packet_t *p1, packet_t *p2) {
    if (num_entangled_pairs >= MAX_ENTANGLED_PAIRS) return;
    
    // Generate entangled quantum states
    uint8_t *state = entangled_states[num_entangled_pairs];
    qrng_entangle_states(ctx, state, state + 32, 32);
    
    // Link packets
    p1->is_entangled = true;
    p2->is_entangled = true;
    p1->entangled_with = p2->id;
    p2->entangled_with = p1->id;
    
    num_entangled_pairs++;
    current_stats.entangled_packets += 2;
}

void apply_quantum_noise(qrng_ctx *ctx, connection_t *conn) {
    // Apply quantum noise to connection properties
    double noise = conn->quantum_noise * qrng_double(ctx);
    conn->bandwidth *= (1.0 - noise);
    conn->latency *= (1.0 + noise);
}

void generate_traffic_burst(qrng_ctx *ctx) {
    // Generate quantum-based traffic burst
    current_stats.burst_magnitude = 1.0 + qrng_double(ctx) * 4.0;  // 1x-5x burst
}

bool detect_quantum_anomaly(qrng_ctx *ctx, const traffic_stats_t *stats) {
    (void)stats;  // Statistics snapshot reserved for richer detectors

    // Use quantum entropy to detect network anomalies
    double entropy = qrng_get_entropy_estimate(ctx);
    current_stats.quantum_entropy = entropy;

    return entropy < global_config->anomaly_threshold;
}

static connection_t *find_connection(int a, int b) {
    for (int i = 0; i < num_connections; i++) {
        if ((connections[i].source == a && connections[i].destination == b) ||
            (connections[i].source == b && connections[i].destination == a)) {
            return &connections[i];
        }
    }
    return NULL;
}

static void process_packet(packet_t *packet) {
    if (packet->source >= num_nodes || packet->destination >= num_nodes) {
        current_stats.dropped_packets++;
        return;
    }

    // Breadth-first search for the shortest hop path (store-and-forward)
    int parent[MAX_NODES];
    int queue[MAX_NODES];
    int head = 0, tail = 0;
    for (int i = 0; i < num_nodes; i++) parent[i] = -1;
    parent[packet->source] = packet->source;
    queue[tail++] = packet->source;
    while (head < tail && parent[packet->destination] == -1) {
        int u = queue[head++];
        for (int i = 0; i < nodes[u].num_connections; i++) {
            int v = nodes[u].connections[i];
            if (parent[v] == -1) {
                parent[v] = u;
                queue[tail++] = v;
            }
        }
    }

    if (parent[packet->destination] == -1) {
        current_stats.dropped_packets++;  // No route to destination
        return;
    }

    // Collect the connections along the path (destination back to source)
    connection_t *path[MAX_NODES];
    int num_hops = 0;
    for (int v = packet->destination; v != packet->source; v = parent[v]) {
        path[num_hops++] = find_connection(parent[v], v);
    }

    // Check queue capacity along the chosen path
    for (int i = 0; i < num_hops; i++) {
        if (path[i]->queue_size >= MAX_QUEUE_SIZE) {
            current_stats.dropped_packets++;
            return;
        }
    }
    
    // Apply QoS priority based on protocol and quantum state
    if (path[0]->qos_enabled) {
        switch (packet->protocol) {
            case PROTOCOL_ICMP:
                packet->priority = 7;
                break;
            case PROTOCOL_UDP:
                packet->priority = packet->is_entangled ? 6 : 5;
                break;
            case PROTOCOL_TCP:
                packet->priority = packet->is_entangled ? 4 : 3;
                break;
        }
    }
    
    // Path latency is the sum over all hops
    double path_latency = 0.0;
    for (int i = 0; i < num_hops; i++) {
        path_latency += path[i]->latency;
    }

    // Calculate delivery time with quantum effects
    double delivery_time = packet->creation_time + path_latency;
    if (path[0]->qos_enabled && packet->priority > 4) {
        delivery_time *= 0.8;
    }
    if (packet->is_entangled) {
        delivery_time *= 0.9;  // Entangled packets get priority scheduling
    }

    packet->delivery_time = delivery_time;

    // Simulate bandwidth consumption along the whole path
    double bandwidth_used = packet->size / 1e6;
    for (int i = 0; i < num_hops; i++) {
        if (path[i]->bandwidth < bandwidth_used) {
            current_stats.dropped_packets++;
            return;
        }
    }
    for (int i = 0; i < num_hops; i++) {
        path[i]->bandwidth -= bandwidth_used;
        path[i]->queue_size++;
    }
    current_stats.avg_latency += path_latency;
    current_stats.total_packets++;
}

void visualize_network_topology(void) {
    printf("\nNetwork Topology:\n");
    printf("Nodes: %d, Connections: %d\n\n", num_nodes, num_connections);
    
    // Create simple ASCII visualization
    char grid[20][20] = {0};
    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 20; j++) {
            grid[i][j] = '.';
        }
    }
    
    // Plot connections
    for (int i = 0; i < num_connections; i++) {
        int x1 = (int)(nodes[connections[i].source].x / 5.0);
        int y1 = (int)(nodes[connections[i].source].y / 5.0);
        int x2 = (int)(nodes[connections[i].destination].x / 5.0);
        int y2 = (int)(nodes[connections[i].destination].y / 5.0);
        
        // Draw simple line
        int dx = abs(x2 - x1);
        int dy = abs(y2 - y1);
        if (dx == 0 && dy == 0) {
            continue;  // Both endpoints map to the same cell; nothing to draw
        }
        if (dx > dy) {
            for (int x = min(x1, x2); x <= max(x1, x2); x++) {
                int y = y1 + (x - x1) * (y2 - y1) / (x2 - x1);
                if (x >= 0 && x < 20 && y >= 0 && y < 20) {
                    grid[y][x] = '*';
                }
            }
        } else {
            for (int y = min(y1, y2); y <= max(y1, y2); y++) {
                int x = x1 + (y - y1) * (x2 - x1) / (y2 - y1);
                if (x >= 0 && x < 20 && y >= 0 && y < 20) {
                    grid[y][x] = '*';
                }
            }
        }
    }
    
    // Plot nodes last so they are not overwritten by connection lines
    for (int i = 0; i < num_nodes; i++) {
        int x = (int)(nodes[i].x / 5.0);
        int y = (int)(nodes[i].y / 5.0);
        if (x >= 0 && x < 20 && y >= 0 && y < 20) {
            grid[y][x] = 'O';
        }
    }

    // Print grid
    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 20; j++) {
            printf("%c ", grid[i][j]);
        }
        printf("\n");
    }
    printf("\nLegend: O=Node *=Connection\n\n");
}

void visualize_quantum_states(void) {
    printf("\nQuantum States:\n");
    printf("Entangled Pairs: %d\n", num_entangled_pairs);
    printf("Current Entropy: %.4f\n", current_stats.quantum_entropy);
    if (current_stats.burst_magnitude > 1.0) {
        printf("Active Burst: %.1fx magnitude\n", current_stats.burst_magnitude);
    }
}

// Count how many nodes are reachable from node 0 via a breadth-first walk.
static int count_reachable_nodes(void) {
    if (num_nodes == 0) return 0;
    bool seen[MAX_NODES] = {false};
    int queue[MAX_NODES];
    int head = 0, tail = 0, count = 0;
    seen[0] = true;
    queue[tail++] = 0;
    while (head < tail) {
        int u = queue[head++];
        count++;
        for (int i = 0; i < nodes[u].num_connections; i++) {
            int v = nodes[u].connections[i];
            if (!seen[v]) {
                seen[v] = true;
                queue[tail++] = v;
            }
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
// Topology-file loading
//
// Text format (one directive per line; '#' starts a comment; blank lines and
// leading whitespace are ignored). Directive order: 'nodes' first, then any
// number of 'node' and 'edge' lines.
//
//   nodes <count>                     Number of nodes, 2..MAX_NODES (required,
//                                     appears exactly once, before node/edge)
//   node  <id> <x> <y>                Optional 2D position for a node id in
//                                     [0,count); used only for visualization.
//                                     Unspecified nodes get a random position.
//   edge  <src> <dst> [bw] [latency]  Undirected link between two distinct
//                                     node ids. bw (Mbps) and latency (ms) are
//                                     optional; omitted/<=0 values are filled
//                                     with quantum-random defaults.
//
// Any malformed line, out-of-range id, self-loop, or capacity overflow is a
// hard error: the function prints the offending file/line and exits(1) rather
// than silently falling back to a generated network.
// ---------------------------------------------------------------------------
static void load_topology_file(qrng_ctx *ctx, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open topology file '%s'\n", path);
        exit(1);
    }

    num_nodes = 0;
    num_connections = 0;
    bool have_count = false;
    bool positioned[MAX_NODES] = {false};

    char line[512];
    int lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;

        // Strip comments, then read the leading keyword (skips blank lines).
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        char keyword[32];
        if (sscanf(line, "%31s", keyword) != 1) continue;

        if (strcmp(keyword, "nodes") == 0) {
            if (have_count) {
                fprintf(stderr, "Error: %s line %d: duplicate 'nodes' directive\n",
                        path, lineno);
                fclose(f); exit(1);
            }
            int n;
            if (sscanf(line, "%*s %d", &n) != 1) {
                fprintf(stderr, "Error: %s line %d: 'nodes' needs an integer count\n",
                        path, lineno);
                fclose(f); exit(1);
            }
            if (n < 2 || n > MAX_NODES) {
                fprintf(stderr, "Error: %s line %d: node count %d out of range (2..%d)\n",
                        path, lineno, n, MAX_NODES);
                fclose(f); exit(1);
            }
            num_nodes = n;
            have_count = true;
            for (int i = 0; i < num_nodes; i++) {
                nodes[i].id = i;
                nodes[i].x = 0.0;
                nodes[i].y = 0.0;
                nodes[i].num_connections = 0;
                qrng_bytes(ctx, nodes[i].quantum_state, sizeof(nodes[i].quantum_state));
            }
        } else if (strcmp(keyword, "node") == 0) {
            if (!have_count) {
                fprintf(stderr, "Error: %s line %d: 'node' before 'nodes' directive\n",
                        path, lineno);
                fclose(f); exit(1);
            }
            int id; double x, y;
            if (sscanf(line, "%*s %d %lf %lf", &id, &x, &y) != 3) {
                fprintf(stderr, "Error: %s line %d: 'node' needs '<id> <x> <y>'\n",
                        path, lineno);
                fclose(f); exit(1);
            }
            if (id < 0 || id >= num_nodes) {
                fprintf(stderr, "Error: %s line %d: node id %d out of range (0..%d)\n",
                        path, lineno, id, num_nodes - 1);
                fclose(f); exit(1);
            }
            nodes[id].x = x;
            nodes[id].y = y;
            positioned[id] = true;
        } else if (strcmp(keyword, "edge") == 0) {
            if (!have_count) {
                fprintf(stderr, "Error: %s line %d: 'edge' before 'nodes' directive\n",
                        path, lineno);
                fclose(f); exit(1);
            }
            int a, b; double bw = 0.0, lat = 0.0;
            int got = sscanf(line, "%*s %d %d %lf %lf", &a, &b, &bw, &lat);
            if (got < 2) {
                fprintf(stderr, "Error: %s line %d: 'edge' needs at least '<src> <dst>'\n",
                        path, lineno);
                fclose(f); exit(1);
            }
            if (a < 0 || a >= num_nodes || b < 0 || b >= num_nodes) {
                fprintf(stderr, "Error: %s line %d: edge endpoint out of range (0..%d)\n",
                        path, lineno, num_nodes - 1);
                fclose(f); exit(1);
            }
            if (a == b) {
                fprintf(stderr, "Error: %s line %d: self-loop on node %d not allowed\n",
                        path, lineno, a);
                fclose(f); exit(1);
            }
            if (find_connection(a, b)) {
                fprintf(stderr, "Error: %s line %d: duplicate edge %d-%d\n",
                        path, lineno, a, b);
                fclose(f); exit(1);
            }
            if (num_connections >= MAX_CONNECTIONS) {
                fprintf(stderr, "Error: %s line %d: too many edges (max %d)\n",
                        path, lineno, MAX_CONNECTIONS);
                fclose(f); exit(1);
            }
            if (nodes[a].num_connections >= MAX_NODES ||
                nodes[b].num_connections >= MAX_NODES) {
                fprintf(stderr, "Error: %s line %d: node degree limit exceeded\n",
                        path, lineno);
                fclose(f); exit(1);
            }
            if (got < 3 || bw <= 0.0) {
                bw = 500.0 + qrng_double(ctx) * 1000.0;
            }
            if (got < 4 || lat <= 0.0) {
                lat = 5.0 + qrng_double(ctx) * 20.0;
            }
            connection_t *conn = &connections[num_connections];
            conn->id = num_connections;
            conn->source = a;
            conn->destination = b;
            conn->bandwidth = bw;
            conn->latency = lat;
            conn->queue_size = 0;
            conn->qos_enabled = true;
            conn->quantum_noise = qrng_double(ctx) * 0.1;
            nodes[a].connections[nodes[a].num_connections++] = b;
            nodes[b].connections[nodes[b].num_connections++] = a;
            num_connections++;
        } else {
            fprintf(stderr, "Error: %s line %d: unknown directive '%s'\n",
                    path, lineno, keyword);
            fclose(f); exit(1);
        }
    }
    fclose(f);

    if (!have_count) {
        fprintf(stderr, "Error: %s: no 'nodes' directive found\n", path);
        exit(1);
    }

    // Give any node without explicit coordinates a quantum-random position so
    // the ASCII visualization still has something to plot.
    for (int i = 0; i < num_nodes; i++) {
        if (!positioned[i]) {
            nodes[i].x = qrng_double(ctx) * 100.0;
            nodes[i].y = qrng_double(ctx) * 100.0;
        }
    }

    printf("Loaded topology '%s': %d nodes, %d edges\n",
           path, num_nodes, num_connections);
    int reachable = count_reachable_nodes();
    if (reachable < num_nodes) {
        printf("Warning: topology is not fully connected "
               "(%d/%d nodes reachable from node 0); "
               "packets with no route will be dropped\n",
               reachable, num_nodes);
    }
}

void init_traffic_sim(config_t *config, qrng_ctx *ctx) {
    memset(&current_stats, 0, sizeof(traffic_stats_t));
    current_stats.burst_magnitude = 1.0;  // Initialize to normal level
    num_entangled_pairs = 0;
    global_config = config;  // Store config globally
    
    if (config->topology_file[0]) {
        load_topology_file(ctx, config->topology_file);
        // Reflect the loaded node count back into the config so the run
        // summary reports the real network, not the CLI default.
        config->num_nodes = num_nodes;
    } else {
        generate_quantum_topology(ctx, config->num_nodes);
    }
}

void simulate_traffic(qrng_ctx *ctx, const config_t *config) {
    time_t start_time = time(NULL);
    double last_viz_update = 0;
    int total_packets = config->simulation_time * config->flow_rate;
    int packets_processed = 0;
    current_stats.burst_magnitude = 1.0;  // Initialize to normal level
    
    while ((time(NULL) - start_time) < config->simulation_time) {
        double elapsed = time(NULL) - start_time;
        
        // Generate packets based on flow rate and current burst magnitude
        int expected_packets = (int)(config->flow_rate * 0.1 * current_stats.burst_magnitude);
        int batch_size = min(expected_packets, total_packets - packets_processed);
        
        for (int i = 0; i < batch_size && packets_processed < total_packets; i++) {
            // Create new packet
            packet_t packet = {
                .id = packets_processed,
                .protocol = config->protocol,
                .source = qrng_range32(ctx, 0, num_nodes - 1),
                .destination = 0,  /* chosen below, != source */
                .size = qrng_range32(ctx, 1000, 2000),
                .priority = 0,
                .creation_time = elapsed,
                .delivery_time = 0,
                .is_entangled = false,
                .entangled_with = -1
            };
            if (num_nodes > 1) {
                do {
                    packet.destination = qrng_range32(ctx, 0, num_nodes - 1);
                } while (packet.destination == packet.source);
            }
            
            // Possibly create entangled pair
            if (config->use_entanglement && 
                qrng_double(ctx) < config->entangle_rate && 
                packets_processed + 1 < total_packets) {
                
                packet_t entangled_packet = packet;
                entangled_packet.id = packets_processed + 1;
                entangled_packet.source = packet.destination;
                entangled_packet.destination = packet.source;
                
                create_entangled_packets(ctx, &packet, &entangled_packet);
                
                process_packet(&packet);
                process_packet(&entangled_packet);
                packets_processed += 2;
            } else {
                process_packet(&packet);
                packets_processed++;
            }
        }
        
        // Update network state
        for (int i = 0; i < num_connections; i++) {
            apply_quantum_noise(ctx, &connections[i]);
            connections[i].bandwidth += (1500.0 - connections[i].bandwidth) * 0.1;
            if (connections[i].queue_size > 0) {
                connections[i].queue_size--;
            }
        }
        
        // Bursts decay back toward the normal level over time
        current_stats.burst_magnitude = 1.0 + (current_stats.burst_magnitude - 1.0) * 0.8;

        // Check for quantum-based events (less frequently)
        if (config->generate_bursts && qrng_double(ctx) < config->burst_probability * 0.1) {
            generate_traffic_burst(ctx);
        }
        
        // Check for anomalies
        if (config->detect_anomalies && 
            detect_quantum_anomaly(ctx, &current_stats)) {
            current_stats.anomalies_detected++;
        }
        
        // Update visualization less frequently (every 1 second)
        if (config->visualize && (elapsed - last_viz_update >= 1.0)) {
            printf("\rTime: %.1fs, Processed: %d, Dropped: %d, Entangled: %d, Anomalies: %d",
                   elapsed, current_stats.total_packets,
                   current_stats.dropped_packets, current_stats.entangled_packets,
                   current_stats.anomalies_detected);
            fflush(stdout);
            
            if ((int)elapsed % 5 == 0) {
                visualize_quantum_states();
            }
            last_viz_update = elapsed;
        }
        
        // Sleep for a short time to prevent overwhelming the system
        usleep(100000);  // 100ms sleep
    }
    
    if (config->visualize) {
        printf("\nSimulation completed.\n");
    }
}

traffic_stats_t get_traffic_stats(void) {
    // avg_latency accumulates per-packet latency; convert to a mean in the
    // returned copy without mutating the accumulator (so repeated calls
    // stay correct).
    traffic_stats_t stats = current_stats;
    if (stats.total_packets > 0) {
        stats.avg_latency /= stats.total_packets;
    }
    return stats;
}

void visualize_traffic_stats(const traffic_stats_t *stats) {
    int attempted = stats->total_packets + stats->dropped_packets;
    printf("\nTraffic Statistics:\n");
    printf("- Delivered packets: %d\n", stats->total_packets);
    printf("- Dropped packets: %d (%.1f%%)\n", stats->dropped_packets,
           attempted > 0 ? 100.0 * stats->dropped_packets / attempted : 0.0);
    printf("- Entangled packets: %d\n", stats->entangled_packets);
    printf("- Average latency: %.2f ms\n", stats->avg_latency);
    printf("- Quantum entropy estimate: %.4f\n", stats->quantum_entropy);
    printf("- Anomalies detected: %d\n", stats->anomalies_detected);
}

void cleanup_traffic_sim(void) {
    memset(nodes, 0, sizeof(nodes));
    memset(connections, 0, sizeof(connections));
    memset(&current_stats, 0, sizeof(current_stats));
    memset(entangled_states, 0, sizeof(entangled_states));
    num_nodes = 0;
    num_connections = 0;
    num_entangled_pairs = 0;
    global_config = NULL;
}

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -t, --time N          Simulation duration in seconds (default: 5)\n");
    printf("  -r, --rate N          Packet flow rate per second (default: 200)\n");
    printf("  -n, --nodes N         Number of nodes (2-%d, default: 12)\n", MAX_NODES);
    printf("  -p, --protocol PROTO  Protocol: tcp, udp, icmp (default: tcp)\n");
    printf("  -e, --entangle RATE   Entangled pair generation rate 0.0-1.0 (default: 0.1)\n");
    printf("  -b, --bursts PROB     Traffic burst probability 0.0-1.0 (default: 0.2)\n");
    printf("  -a, --anomaly THRESH  Anomaly entropy threshold (default: 0.5)\n");
    printf("  -f, --topology FILE   Load network topology from a text file (see format below)\n");
    printf("  -Q, --no-qos          Disable QoS priorities\n");
    printf("  -q, --quiet           Disable live visualization\n");
    printf("  -h, --help            Show this help message\n");
    printf("\nTopology file format (one directive per line, '#' = comment):\n");
    printf("  nodes <count>                   number of nodes, 2..%d (required, first)\n", MAX_NODES);
    printf("  node  <id> <x> <y>              optional node position for visualization\n");
    printf("  edge  <src> <dst> [bw] [lat]    undirected link; bw (Mbps) and lat (ms) optional\n");
    printf("When -f is given, -n is ignored (node count comes from the file).\n");
}

int main(int argc, char *argv[]) {
    config_t config = {
        .simulation_time = 5,
        .flow_rate = 200,
        .protocol = PROTOCOL_TCP,
        .qos_enabled = true,
        .visualize = true,
        .use_entanglement = true,
        .generate_bursts = true,
        .detect_anomalies = true,
        .topology_file = {0},
        .num_nodes = 12,
        .entangle_rate = 0.1,
        .burst_probability = 0.2,
        .anomaly_threshold = 0.5
    };

    static struct option long_options[] = {
        {"time",     required_argument, 0, 't'},
        {"rate",     required_argument, 0, 'r'},
        {"nodes",    required_argument, 0, 'n'},
        {"protocol", required_argument, 0, 'p'},
        {"entangle", required_argument, 0, 'e'},
        {"bursts",   required_argument, 0, 'b'},
        {"anomaly",  required_argument, 0, 'a'},
        {"topology", required_argument, 0, 'f'},
        {"no-qos",   no_argument,       0, 'Q'},
        {"quiet",    no_argument,       0, 'q'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "t:r:n:p:e:b:a:f:Qqh", long_options, NULL)) != -1) {
        switch (c) {
            case 't':
                config.simulation_time = atoi(optarg);
                if (config.simulation_time <= 0) {
                    fprintf(stderr, "Error: simulation time must be positive\n");
                    return 1;
                }
                break;
            case 'r':
                config.flow_rate = atoi(optarg);
                if (config.flow_rate <= 0) {
                    fprintf(stderr, "Error: flow rate must be positive\n");
                    return 1;
                }
                break;
            case 'n':
                config.num_nodes = atoi(optarg);
                if (config.num_nodes < 2 || config.num_nodes > MAX_NODES) {
                    fprintf(stderr, "Error: nodes must be between 2 and %d\n", MAX_NODES);
                    return 1;
                }
                break;
            case 'p':
                if (strcmp(optarg, "tcp") == 0) config.protocol = PROTOCOL_TCP;
                else if (strcmp(optarg, "udp") == 0) config.protocol = PROTOCOL_UDP;
                else if (strcmp(optarg, "icmp") == 0) config.protocol = PROTOCOL_ICMP;
                else {
                    fprintf(stderr, "Error: unknown protocol '%s'\n", optarg);
                    return 1;
                }
                break;
            case 'e':
                config.entangle_rate = atof(optarg);
                if (config.entangle_rate < 0.0 || config.entangle_rate > 1.0) {
                    fprintf(stderr, "Error: entangle rate must be in [0,1]\n");
                    return 1;
                }
                break;
            case 'b':
                config.burst_probability = atof(optarg);
                if (config.burst_probability < 0.0 || config.burst_probability > 1.0) {
                    fprintf(stderr, "Error: burst probability must be in [0,1]\n");
                    return 1;
                }
                break;
            case 'a':
                config.anomaly_threshold = atof(optarg);
                break;
            case 'f':
                strncpy(config.topology_file, optarg, sizeof(config.topology_file) - 1);
                config.topology_file[sizeof(config.topology_file) - 1] = '\0';
                break;
            case 'Q':
                config.qos_enabled = false;
                break;
            case 'q':
                config.visualize = false;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    qrng_ctx *ctx;
    if (qrng_init(&ctx, (uint8_t*)"traffic", 7) != QRNG_SUCCESS) {
        fprintf(stderr, "Error: failed to initialize quantum RNG\n");
        return 1;
    }

    printf("Quantum Traffic Simulation\n");
    printf("==========================\n");

    // Build the network first (this may load a topology file and update
    // config.num_nodes), so the summary line below reports the real network.
    init_traffic_sim(&config, ctx);

    printf("Nodes: %d, Duration: %ds, Rate: %d pkt/s, Protocol: %s\n",
           config.num_nodes, config.simulation_time, config.flow_rate,
           config.protocol == PROTOCOL_TCP ? "TCP" :
           config.protocol == PROTOCOL_UDP ? "UDP" : "ICMP");

    if (config.visualize) {
        visualize_network_topology();
    }

    simulate_traffic(ctx, &config);

    traffic_stats_t stats = get_traffic_stats();
    visualize_traffic_stats(&stats);

    cleanup_traffic_sim();
    qrng_free(ctx);
    return 0;
}
