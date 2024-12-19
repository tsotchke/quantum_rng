#ifndef QUANTUM_KEY_EXCHANGE_H
#define QUANTUM_KEY_EXCHANGE_H

#include <stdint.h>
#include <stddef.h>
#include "../../src/quantum_rng/quantum_rng.h"

// Key exchange parameters
#define KEY_SIZE 32           // Size of final shared key in bytes
#define NONCE_SIZE 16         // Size of nonce in bytes
#define HASH_SIZE 32          // Size of hash output in bytes
#define MAX_MESSAGE_SIZE 1024 // Maximum message size
#define MAX_ROUNDS 5          // Maximum number of exchange rounds
#define MIN_ENTROPY 4.5       // Adjusted minimum entropy per byte required

// Protocol phases
typedef enum {
    PHASE_INIT,
    PHASE_EXCHANGE,
    PHASE_VERIFY,
    PHASE_FINALIZE
} exchange_phase_t;

// Output modes
typedef enum {
    OUTPUT_NORMAL,
    OUTPUT_QUIET,
    OUTPUT_VERBOSE,
    OUTPUT_JSON,
    OUTPUT_HEX
} output_mode_t;

// Party roles
typedef enum {
    ROLE_INITIATOR,
    ROLE_RESPONDER
} party_role_t;

// Key material structure
typedef struct {
    uint8_t public_key[KEY_SIZE];
    uint8_t private_key[KEY_SIZE];
    uint8_t shared_secret[KEY_SIZE];
    uint8_t session_key[KEY_SIZE];
    uint8_t nonce[NONCE_SIZE];
} key_material_t;

// Protocol configuration
typedef struct {
    party_role_t role;
    char seed[256];
    int seed_length;
    output_mode_t output_mode;
    int show_progress;
    char output_file[1024];
    int verify_entropy;
    int rounds;
    int interactive;
} exchange_config_t;

// Exchange state
typedef struct {
    exchange_phase_t phase;
    key_material_t keys;
    uint8_t transcript_hash[HASH_SIZE];
    double entropy_estimate;
    int round;
} exchange_state_t;

// Function declarations
void init_exchange_config(exchange_config_t *config);
void parse_exchange_args(int argc, char *argv[], exchange_config_t *config);
void print_exchange_usage(const char *program_name);
exchange_state_t run_key_exchange(const exchange_config_t *config);
void print_results(const exchange_state_t *state, const exchange_config_t *config);
void output_results_json(const exchange_state_t *state, const exchange_config_t *config);
void output_results_hex(const exchange_state_t *state, const exchange_config_t *config);
void verify_key_material(const exchange_state_t *state);
void run_interactive_mode(const exchange_config_t *config);
void simulate_network_exchange(exchange_state_t *initiator, exchange_state_t *responder);

// Utility functions
void generate_key_material(qrng_ctx *ctx, key_material_t *keys);
void derive_session_key(key_material_t *keys, const uint8_t *transcript, size_t transcript_len);
void update_transcript_hash(uint8_t *hash, const void *data, size_t len);
double estimate_entropy(const uint8_t *data, size_t len);
void print_hex(const char *label, const uint8_t *data, size_t len);

#endif /* QUANTUM_KEY_EXCHANGE_H */
