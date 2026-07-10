#include "../../src/quantum_rng/quantum_rng.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_PARTICLES 1000
#define DIM 3
#define CUTOFF_RADIUS 2.5
#define TIME_STEP 0.002
#define TEMPERATURE 1.0
#define BOX_SIZE 10.0
#define LANGEVIN_GAMMA 0.5   /* Langevin thermostat friction coefficient */

// Checked allocation helper
static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "Error: out of memory (%zu bytes)\n", size);
        exit(1);
    }
    return p;
}

// Standard normal deviate via Box-Muller (u1 in (0,1] so log() is finite)
static double gaussian(qrng_ctx *ctx) {
    double u1 = 1.0 - qrng_double(ctx);
    double u2 = qrng_double(ctx);
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

typedef struct {
    double position[DIM];
    double velocity[DIM];
    double force[DIM];
    double mass;
} particle_t;

typedef struct {
    particle_t *particles;
    int num_particles;
    double temperature;
    double box_size;
    double potential_energy;
    double kinetic_energy;
    double total_energy;
} system_t;

// Initialize system: particles on a cubic lattice (random placement would
// allow overlaps, and the r^-12 Lennard-Jones repulsion then produces
// astronomically large forces that blow the integration up immediately).
// Velocities are Maxwell-Boltzmann distributed using quantum randomness.
void init_system(qrng_ctx *ctx, system_t *sys, int num_particles) {
    sys->particles = xmalloc(num_particles * sizeof(particle_t));
    sys->num_particles = num_particles;
    sys->temperature = TEMPERATURE;
    sys->box_size = BOX_SIZE;

    // Smallest cubic lattice that holds all particles
    int cells = (int)ceil(cbrt((double)num_particles));
    double spacing = sys->box_size / cells;

    for(int i = 0; i < num_particles; i++) {
        particle_t *p = &sys->particles[i];
        p->mass = 1.0;

        // Lattice position plus a small quantum jitter (up to 5% of spacing)
        int cx = i % cells;
        int cy = (i / cells) % cells;
        int cz = i / (cells * cells);
        p->position[0] = (cx + 0.5) * spacing + (qrng_double(ctx) - 0.5) * 0.1 * spacing;
        p->position[1] = (cy + 0.5) * spacing + (qrng_double(ctx) - 0.5) * 0.1 * spacing;
        p->position[2] = (cz + 0.5) * spacing + (qrng_double(ctx) - 0.5) * 0.1 * spacing;

        // Maxwell-Boltzmann velocities using quantum randomness
        double velocity_scale = sqrt(sys->temperature / p->mass);
        for(int d = 0; d < DIM; d++) {
            p->velocity[d] = velocity_scale * gaussian(ctx);
        }

        // Initialize forces to zero
        for(int d = 0; d < DIM; d++) {
            p->force[d] = 0.0;
        }
    }
}

// Lennard-Jones potential and force calculation
void calculate_forces(system_t *sys) {
    // Reset forces and energy
    sys->potential_energy = 0.0;
    
    for(int i = 0; i < sys->num_particles; i++) {
        for(int d = 0; d < DIM; d++) {
            sys->particles[i].force[d] = 0.0;
        }
    }
    
    // Calculate forces between all pairs
    for(int i = 0; i < sys->num_particles; i++) {
        for(int j = i + 1; j < sys->num_particles; j++) {
            double rij[DIM];
            double r2 = 0.0;
            
            // Calculate distance with periodic boundary conditions
            for(int d = 0; d < DIM; d++) {
                rij[d] = sys->particles[i].position[d] - 
                        sys->particles[j].position[d];
                // Minimum image convention
                rij[d] -= sys->box_size * round(rij[d] / sys->box_size);
                r2 += rij[d] * rij[d];
            }
            
            if(r2 < CUTOFF_RADIUS * CUTOFF_RADIUS) {
                // Cap extreme close approaches so a single overlapping pair
                // cannot produce a non-finite force
                if(r2 < 0.64) r2 = 0.64;  // r_min = 0.8 sigma
                double r6i = 1.0 / (r2 * r2 * r2);
                double r12i = r6i * r6i;
                double force_magnitude = 24.0 * (2.0 * r12i - r6i) / r2;
                sys->potential_energy += 4.0 * (r12i - r6i);
                
                // Apply forces to both particles
                for(int d = 0; d < DIM; d++) {
                    double force = force_magnitude * rij[d];
                    sys->particles[i].force[d] += force;
                    sys->particles[j].force[d] -= force;
                }
            }
        }
    }
}

// Velocity Verlet integration with a Langevin thermostat driven by quantum
// randomness. The previous version recomputed *all* forces inside the
// per-particle loop (O(N^3) and physically wrong, since particles saw a
// mixture of old and new positions) and injected noise without friction,
// so the system heated up without bound. Standard velocity Verlet updates
// all positions first, computes forces once, then updates all velocities;
// the Langevin friction/noise pair keeps the temperature near the target.
void integrate(qrng_ctx *ctx, system_t *sys) {
    // Half-step velocity update and full position update
    for(int i = 0; i < sys->num_particles; i++) {
        particle_t *p = &sys->particles[i];
        for(int d = 0; d < DIM; d++) {
            p->velocity[d] += 0.5 * p->force[d] / p->mass * TIME_STEP;
            p->position[d] += p->velocity[d] * TIME_STEP;

            // Periodic boundary conditions
            p->position[d] -= sys->box_size * floor(p->position[d] / sys->box_size);
        }
    }

    // Compute forces once, with all positions updated
    calculate_forces(sys);

    // Second half-step velocity update plus Langevin thermostat
    sys->kinetic_energy = 0.0;
    for(int i = 0; i < sys->num_particles; i++) {
        particle_t *p = &sys->particles[i];
        for(int d = 0; d < DIM; d++) {
            p->velocity[d] += 0.5 * p->force[d] / p->mass * TIME_STEP;

            // Langevin thermostat: friction plus balanced quantum noise
            double noise_amplitude = sqrt(2.0 * LANGEVIN_GAMMA * sys->temperature *
                                          TIME_STEP / p->mass);
            p->velocity[d] += -LANGEVIN_GAMMA * p->velocity[d] * TIME_STEP +
                              noise_amplitude * gaussian(ctx);
        }

        for(int d = 0; d < DIM; d++) {
            sys->kinetic_energy += 0.5 * p->mass * p->velocity[d] * p->velocity[d];
        }
    }

    sys->total_energy = sys->kinetic_energy + sys->potential_energy;
}

// Calculate system temperature from kinetic energy
double calculate_temperature(const system_t *sys) {
    return 2.0 * sys->kinetic_energy / (3.0 * sys->num_particles);
}

// Accumulate one radial distribution function sample into the histogram
// (call accumulate_rdf once per sampled configuration, then normalize_rdf)
void accumulate_rdf(const system_t *sys, double *rdf_hist, int num_bins) {
    double dr = CUTOFF_RADIUS / num_bins;

    // Calculate pair distances
    for(int i = 0; i < sys->num_particles; i++) {
        for(int j = i + 1; j < sys->num_particles; j++) {
            double r2 = 0.0;
            
            for(int d = 0; d < DIM; d++) {
                double rij = sys->particles[i].position[d] - 
                           sys->particles[j].position[d];
                rij -= sys->box_size * round(rij / sys->box_size);
                r2 += rij * rij;
            }
            
            double r = sqrt(r2);
            if(r < CUTOFF_RADIUS) {
                int bin = (int)(r / dr);
                if(bin < num_bins) {
                    rdf_hist[bin] += 2.0;  // Count each pair twice
                }
            }
        }
    }
}

// Convert the accumulated histogram into g(r), averaged over num_samples
void normalize_rdf(const system_t *sys, double *rdf_hist, int num_bins,
                   int num_samples) {
    double dr = CUTOFF_RADIUS / num_bins;
    double volume = sys->box_size * sys->box_size * sys->box_size;
    double density = sys->num_particles / volume;

    for(int i = 0; i < num_bins; i++) {
        double r = (i + 0.5) * dr;
        double shell_volume = 4.0 * M_PI * r * r * dr;
        rdf_hist[i] /= shell_volume * density * sys->num_particles * num_samples;
    }
}

void run_simulation(void) {
    qrng_ctx *ctx;
    if (qrng_init(&ctx, (uint8_t*)"mdsim", 5) != QRNG_SUCCESS) {
        fprintf(stderr, "Error: failed to initialize quantum RNG\n");
        exit(1);
    }
    
    printf("Quantum Molecular Dynamics Simulation\n");
    printf("====================================\n\n");
    
    // Initialize system
    system_t sys;
    init_system(ctx, &sys, 100);  // 100 particles
    
    // RDF calculation
    const int num_bins = 50;
    double *rdf = calloc(num_bins, sizeof(double));
    if (!rdf) {
        fprintf(stderr, "Error: out of memory\n");
        exit(1);
    }
    int rdf_samples = 0;

    // Initial forces for the first velocity-Verlet step
    calculate_forces(&sys);
    
    printf("Starting simulation...\n");
    printf("Time    Temperature    Potential    Kinetic    Total\n");
    printf("----------------------------------------------------\n");
    
    // Run simulation
    const int num_steps = 1000;
    for(int step = 0; step < num_steps; step++) {
        integrate(ctx, &sys);
        
        if(step % 100 == 0) {
            printf("%-8d%-14.3f%-13.3f%-11.3f%-8.3f\n",
                   step,
                   calculate_temperature(&sys),
                   sys.potential_energy,
                   sys.kinetic_energy,
                   sys.total_energy);
        }
        
        // Accumulate RDF samples over the last 100 steps
        if(step >= num_steps - 100) {
            accumulate_rdf(&sys, rdf, num_bins);
            rdf_samples++;
        }
    }

    normalize_rdf(&sys, rdf, num_bins, rdf_samples);

    // Print final RDF
    printf("\nRadial Distribution Function:\n");
    printf("r        g(r)\n");
    printf("-------------\n");
    
    double dr = CUTOFF_RADIUS / num_bins;
    for(int i = 0; i < num_bins; i++) {
        double r = (i + 0.5) * dr;
        printf("%-8.3f%-8.3f\n", r, rdf[i]);
    }
    
    free(rdf);
    free(sys.particles);
    qrng_free(ctx);
}

int main(void) {
    run_simulation();
    return 0;
}
