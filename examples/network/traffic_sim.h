#ifndef TRAFFIC_SIM_H
#define TRAFFIC_SIM_H

#include "../../src/quantum_rng/quantum_rng.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_NODES 100
#define MAX_CONNECTIONS 1000
#define MAX_QUEUE_SIZE 1000
#define MAX_ENTANGLED_PAIRS 50

typedef enum {
    PROTOCOL_TCP,
    PROTOCOL_UDP,
    PROTOCOL_ICMP
} protocol_type_t;

// Network node structure
typedef struct {
    int id;
    double x, y;  // Position for topology visualization
    int num_connections;
    int connections[MAX_NODES];  // Connected node IDs
    uint8_t quantum_state[32];   // Quantum state for entanglement
} node_t;

// Connection between nodes
typedef struct {
    int id;
    int source;
    int destination;
    double bandwidth;     // Available bandwidth in Mbps
    double latency;       // Base latency in ms
    int queue_size;       // Current queue size
    bool qos_enabled;     // Whether QoS is enabled
    double quantum_noise; // Quantum-based noise factor
} connection_t;

// Network packet
typedef struct {
    int id;
    protocol_type_t protocol;
    int source;
    int destination;
    int size;            // Packet size in bytes
    int priority;        // QoS priority (0-7)
    double creation_time;
    double delivery_time;
    bool is_entangled;   // Part of entangled pair
    int entangled_with;  // ID of entangled packet
} packet_t;

// Traffic statistics
typedef struct {
    double total_bandwidth;
    double avg_latency;
    int dropped_packets;
    int total_packets;
    int entangled_packets;
    double quantum_entropy;
    int anomalies_detected;
    double burst_magnitude;
} traffic_stats_t;

// Simulation configuration
typedef struct {
    int simulation_time;     // Duration in seconds
    int flow_rate;          // Packets per second
    protocol_type_t protocol;
    bool qos_enabled;
    bool visualize;
    bool use_entanglement;  // Enable quantum entanglement
    bool generate_bursts;   // Enable quantum burst generation
    bool detect_anomalies;  // Enable quantum anomaly detection
    char topology_file[256];// Optional topology file
    int num_nodes;         // Number of nodes (if no topology file)
    double entangle_rate;  // Rate of entangled packet generation
    double burst_probability;
    double anomaly_threshold;
} config_t;

// Core simulation functions
void init_traffic_sim(config_t *config, qrng_ctx *ctx);
void simulate_traffic(qrng_ctx *ctx, const config_t *config);
traffic_stats_t get_traffic_stats(void);
void cleanup_traffic_sim(void);

// Quantum-specific functions
void generate_quantum_topology(qrng_ctx *ctx, int num_nodes);
void create_entangled_packets(qrng_ctx *ctx, packet_t *p1, packet_t *p2);
void apply_quantum_noise(qrng_ctx *ctx, connection_t *conn);
void generate_traffic_burst(qrng_ctx *ctx);
bool detect_quantum_anomaly(qrng_ctx *ctx, const traffic_stats_t *stats);

// Visualization functions
void visualize_network_topology(void);
void visualize_quantum_states(void);
void visualize_traffic_stats(const traffic_stats_t *stats);

#endif // TRAFFIC_SIM_H
