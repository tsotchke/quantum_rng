#include "../../src/quantum_rng/quantum_rng.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_PARTICLES 10000
#define NUM_ATTRACTORS 4
#define PI 3.14159265358979323846

typedef struct {
    float x, y, z;           // Position in 3D space
    float vx, vy, vz;        // Velocity
    float ax, ay, az;        // Acceleration
    float mass;              // Particle mass
    float charge;            // Particle charge (for electromagnetic-like forces)
    float lifetime;          // Remaining lifetime
    float spin;              // Quantum spin property
    int entangled_with;      // Index of entangled particle, -1 if none
                             // (must be able to index the whole pool AND
                             // hold a "none" sentinel -- uint8_t cannot)
    void* user_data;         // Custom data for game integration
} particle_t;

typedef struct {
    float x, y, z;          // Position
    float strength;         // Attraction/repulsion strength
    float radius;          // Influence radius
} attractor_t;

typedef struct {
    particle_t* particles;
    size_t count;
    size_t capacity;
    attractor_t attractors[NUM_ATTRACTORS];
    qrng_ctx* rng;
    float time;
    float quantum_flux;    // Global quantum field strength
} particle_system_t;

particle_system_t* ps_create(size_t max_particles) {
    particle_system_t* sys = malloc(sizeof(particle_system_t));
    if (!sys) return NULL;
    
    sys->particles = malloc(max_particles * sizeof(particle_t));
    if (!sys->particles) {
        free(sys);
        return NULL;
    }
    
    if (qrng_init(&sys->rng, (uint8_t*)"particles", 9) != QRNG_SUCCESS) {
        free(sys->particles);
        free(sys);
        return NULL;
    }
    sys->count = 0;
    sys->capacity = max_particles;
    sys->time = 0;
    sys->quantum_flux = 1.0f;
    
    // Initialize quantum attractors
    for (int i = 0; i < NUM_ATTRACTORS; i++) {
        sys->attractors[i].x = qrng_double(sys->rng) * 2.0f - 1.0f;
        sys->attractors[i].y = qrng_double(sys->rng) * 2.0f - 1.0f;
        sys->attractors[i].z = qrng_double(sys->rng) * 2.0f - 1.0f;
        sys->attractors[i].strength = (qrng_double(sys->rng) - 0.5f) * 2.0f;
        sys->attractors[i].radius = qrng_double(sys->rng) * 0.5f + 0.5f;
    }
    
    return sys;
}

void ps_destroy(particle_system_t* sys) {
    if (sys) {
        free(sys->particles);
        qrng_free(sys->rng);
        free(sys);
    }
}

size_t ps_emit(particle_system_t* sys, float x, float y, float z) {
    if (sys->count >= sys->capacity) return (size_t)-1;
    
    particle_t* p = &sys->particles[sys->count];
    
    // Position
    p->x = x;
    p->y = y;
    p->z = z;
    
    // Quantum-derived velocity
    float theta = qrng_double(sys->rng) * 2 * PI;
    float phi = qrng_double(sys->rng) * PI;
    float speed = qrng_double(sys->rng) * 2.0f;
    
    p->vx = speed * sin(phi) * cos(theta);
    p->vy = speed * sin(phi) * sin(theta);
    p->vz = speed * cos(phi);
    
    // Quantum properties
    p->mass = qrng_double(sys->rng) * 0.5f + 0.5f;
    p->charge = qrng_double(sys->rng) * 2.0f - 1.0f;
    p->lifetime = qrng_double(sys->rng) * 5.0f + 1.0f;
    p->spin = qrng_double(sys->rng) * 2.0f - 1.0f;
    
    // Quantum entanglement: ~10% of particles pair up with an existing one
    if (qrng_double(sys->rng) < 0.1f && sys->count > 0) {
        int partner = (int)(qrng_uint64(sys->rng) % sys->count);

        // If the chosen partner is already entangled, break its old link
        // first so no third particle is left pointing at it.
        int old = sys->particles[partner].entangled_with;
        if (old >= 0 && (size_t)old < sys->count) {
            sys->particles[old].entangled_with = -1;
        }

        p->entangled_with = partner;
        sys->particles[partner].entangled_with = (int)sys->count;
    } else {
        p->entangled_with = -1;
    }
    
    p->user_data = NULL;
    return sys->count++;
}

void ps_update(particle_system_t* sys, float dt) {
    sys->time += dt;
    
    // Update quantum flux
    sys->quantum_flux = 0.8f + 0.2f * sin(sys->time * 0.5f);
    
    size_t i = 0;
    while (i < sys->count) {
        particle_t* p = &sys->particles[i];
        
        // Reset acceleration
        p->ax = p->ay = p->az = 0;
        
        // Apply attractor forces
        for (int j = 0; j < NUM_ATTRACTORS; j++) {
            float dx = sys->attractors[j].x - p->x;
            float dy = sys->attractors[j].y - p->y;
            float dz = sys->attractors[j].z - p->z;
            float dist = sqrt(dx*dx + dy*dy + dz*dz);
            
            if (dist > 0.001f && dist < sys->attractors[j].radius) {
                float force = sys->attractors[j].strength / (dist * dist);
                force *= p->charge * sys->quantum_flux;
                
                p->ax += (dx / dist) * force;
                p->ay += (dy / dist) * force;
                p->az += (dz / dist) * force;
            }
        }
        
        // Apply quantum entanglement effects
        if (p->entangled_with >= 0 && (size_t)p->entangled_with < sys->count) {
            particle_t* other = &sys->particles[p->entangled_with];
            float dx = other->x - p->x;
            float dy = other->y - p->y;
            float dz = other->z - p->z;
            float dist = sqrt(dx*dx + dy*dy + dz*dz);
            
            if (dist > 0.001f) {
                float force = 0.1f * p->spin * other->spin * sys->quantum_flux;
                p->ax += dx * force;
                p->ay += dy * force;
                p->az += dz * force;
            }
        }
        
        // Update velocity and position
        p->vx += p->ax * dt;
        p->vy += p->ay * dt;
        p->vz += p->az * dt;
        
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->z += p->vz * dt;
        
        // Update lifetime
        p->lifetime -= dt;
        
        // Remove dead particles (swap-with-last, then shrink)
        if (p->lifetime <= 0) {
            // Break the dying particle's entanglement so its partner
            // no longer points at a recycled slot.
            if (p->entangled_with >= 0 && (size_t)p->entangled_with < sys->count) {
                sys->particles[p->entangled_with].entangled_with = -1;
            }

            size_t last = sys->count - 1;
            if (i < last) {
                sys->particles[i] = sys->particles[last];

                // The particle formerly at `last` now lives at `i`;
                // re-point its partner's back-reference.
                int partner = sys->particles[i].entangled_with;
                if (partner >= 0 && (size_t)partner < last) {
                    sys->particles[partner].entangled_with = (int)i;
                }
            }
            sys->count--;
        } else {
            i++;
        }
    }
}

// Utility functions for game integration
void ps_set_attractor(particle_system_t* sys, int index, float x, float y, float z, float strength, float radius) {
    if (index >= 0 && index < NUM_ATTRACTORS) {
        sys->attractors[index].x = x;
        sys->attractors[index].y = y;
        sys->attractors[index].z = z;
        sys->attractors[index].strength = strength;
        sys->attractors[index].radius = radius;
    }
}

const particle_t* ps_get_particles(const particle_system_t* sys, size_t* count) {
    if (count) *count = sys->count;
    return sys->particles;
}

void ps_set_quantum_flux(particle_system_t* sys, float flux) {
    sys->quantum_flux = flux;
}

float ps_get_quantum_flux(const particle_system_t* sys) {
    return sys->quantum_flux;
}

void* ps_get_particle_user_data(const particle_system_t* sys, size_t index) {
    if (index < sys->count) {
        return sys->particles[index].user_data;
    }
    return NULL;
}

void ps_set_particle_user_data(particle_system_t* sys, size_t index, void* data) {
    if (index < sys->count) {
        sys->particles[index].user_data = data;
    }
}

// ============================================================================
// DEMO DRIVER
// ============================================================================

static void print_system_stats(const particle_system_t* sys, float t) {
    size_t entangled = 0;
    double avg_speed = 0.0;

    for (size_t i = 0; i < sys->count; i++) {
        const particle_t* p = &sys->particles[i];
        if (p->entangled_with >= 0) entangled++;
        avg_speed += sqrt(p->vx * p->vx + p->vy * p->vy + p->vz * p->vz);
    }
    if (sys->count > 0) avg_speed /= sys->count;

    printf("t=%5.2fs  alive=%5zu  entangled=%4zu  flux=%.3f  avg_speed=%.3f\n",
           t, sys->count, entangled, sys->quantum_flux, avg_speed);
}

int main(void) {
    printf("Quantum Particle System Demo\n");
    printf("============================\n\n");

    particle_system_t* sys = ps_create(MAX_PARTICLES);
    if (!sys) {
        fprintf(stderr, "Failed to create particle system\n");
        return 1;
    }

    // Burst-emit 2000 particles from three emitters
    const float emitters[3][3] = {
        { 0.0f,  0.0f, 0.0f},
        { 0.5f,  0.5f, 0.2f},
        {-0.5f, -0.3f, 0.4f}
    };
    for (int e = 0; e < 3; e++) {
        for (int i = 0; i < 2000; i++) {
            if (ps_emit(sys, emitters[e][0], emitters[e][1], emitters[e][2])
                    == (size_t)-1) {
                fprintf(stderr, "Emit failed: pool exhausted\n");
                ps_destroy(sys);
                return 1;
            }
        }
    }

    printf("Emitted %zu particles from 3 emitters\n\n", sys->count);

    // Simulate 8 seconds at 50 Hz; particles decay over 1-6 s lifetimes
    const float dt = 0.02f;
    float t = 0.0f;
    print_system_stats(sys, t);
    for (int step = 1; step <= 400; step++) {
        ps_update(sys, dt);
        t += dt;
        if (step % 50 == 0) {
            print_system_stats(sys, t);
        }
    }

    // Verify entanglement bookkeeping survived all the swap-removals
    size_t broken_links = 0;
    for (size_t i = 0; i < sys->count; i++) {
        int partner = sys->particles[i].entangled_with;
        if (partner >= 0) {
            if ((size_t)partner >= sys->count ||
                sys->particles[partner].entangled_with != (int)i) {
                broken_links++;
            }
        }
    }
    printf("\nEntanglement link check: %zu broken links (expect 0)\n", broken_links);

    ps_destroy(sys);

    if (broken_links > 0) {
        fprintf(stderr, "FAIL: inconsistent entanglement indices\n");
        return 1;
    }
    printf("Particle system demo completed successfully.\n");
    return 0;
}
