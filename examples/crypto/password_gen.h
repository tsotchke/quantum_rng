#ifndef PASSWORD_GEN_H
#define PASSWORD_GEN_H

#include <stddef.h>

// Password generation configuration
typedef struct {
    int min_length;
    int max_length;
    int require_uppercase;
    int require_lowercase;
    int require_numbers;
    int require_special;
    const char* allowed_special;
} PasswordConfig;

// Initialize default password configuration
void init_password_config(PasswordConfig* config);

// Generate a quantum random password based on configuration
// Returns 0 on success, negative value on error
int generate_password(const PasswordConfig* config, char* password, size_t buffer_size);

// Validate password strength against configuration
// Returns 0 if valid, negative value if invalid
int validate_password(const char* password, const PasswordConfig* config);

// Calculate password entropy in bits
double calculate_password_entropy(const char* password);

// Generate multiple unique passwords
// Returns number of passwords generated, negative value on error
int generate_multiple_passwords(const PasswordConfig* config, 
                              char** passwords, 
                              size_t num_passwords,
                              size_t buffer_size);

#endif // PASSWORD_GEN_H
