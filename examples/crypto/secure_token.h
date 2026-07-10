#ifndef SECURE_TOKEN_H
#define SECURE_TOKEN_H

#include <stddef.h>
#include <time.h>

// Token configuration
typedef struct {
    size_t token_length;      // Length of generated token
    time_t expiration_time;   // Token expiration in seconds
    int use_uppercase;        // Include uppercase letters
    int use_lowercase;        // Include lowercase letters
    int use_numbers;         // Include numbers
    int use_special;         // Include special characters
} TokenConfig;

// Token metadata
typedef struct {
    unsigned char* token;
    time_t creation_time;
    time_t expiration_time;
    size_t length;
    unsigned char signature[64];
} TokenMetadata;

// Initialize token configuration with default values
void init_token_config(TokenConfig* config);

// Generate a new secure token with quantum randomness
// Returns 0 on success, negative value on error
int generate_secure_token(const TokenConfig* config, TokenMetadata* metadata);

// Validate a token against its metadata
// Returns 0 if valid, negative value if invalid
int validate_token(const unsigned char* token, const TokenMetadata* metadata);

// Check if a token has expired
// Returns 1 if expired, 0 if valid, negative value on error
int is_token_expired(const TokenMetadata* metadata);

// Revoke a token before its expiration
// Returns 0 on success, negative value on error
int revoke_token(TokenMetadata* metadata);

// Convert token to string representation
// Returns 0 on success, negative value on error
int token_to_string(const TokenMetadata* metadata, char* str, size_t str_size);

// Parse token from string representation
// Returns 0 on success, negative value on error
int token_from_string(const char* str, TokenMetadata* metadata);

// Clean up token metadata resources
void cleanup_token_metadata(TokenMetadata* metadata);

// Generate multiple unique tokens
// Returns number of tokens generated, negative value on error
int generate_multiple_tokens(const TokenConfig* config,
                           TokenMetadata* tokens,
                           size_t num_tokens);

#endif // SECURE_TOKEN_H
