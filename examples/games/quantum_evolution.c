#include "../../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define POPULATION_SIZE 1000
#define GENOME_LENGTH 64
#define GENERATIONS 100
#define NUM_OBJECTIVES 3

typedef struct {
    uint64_t genome;
    double fitness[NUM_OBJECTIVES];
    double total_fitness;
} organism_t;

typedef struct {
    organism_t population[POPULATION_SIZE];
    double avg_fitness;
    double max_fitness;
    double objective_avgs[NUM_OBJECTIVES];
    double objective_maxs[NUM_OBJECTIVES];
    int generation;
} ecosystem_t;

// Multiple fitness objectives
void calculate_fitness(organism_t *org) {
    // Objective 1: Maximize number of 1s
    org->fitness[0] = (double)__builtin_popcountll(org->genome) / GENOME_LENGTH;
    
    // Objective 2: Maximize alternating patterns (101010...)
    int alternations = 0;
    uint64_t pattern = org->genome;
    for(int i = 1; i < GENOME_LENGTH; i++) {
        if(((pattern >> i) & 1) != ((pattern >> (i-1)) & 1)) {
            alternations++;
        }
    }
    org->fitness[1] = (double)alternations / (GENOME_LENGTH - 1);
    
    // Objective 3: Maximize symmetry
    int symmetry = 0;
    for(int i = 0; i < GENOME_LENGTH/2; i++) {
        if(((pattern >> i) & 1) == ((pattern >> (GENOME_LENGTH-1-i)) & 1)) {
            symmetry++;
        }
    }
    org->fitness[2] = (double)symmetry / (GENOME_LENGTH/2);
    
    // Calculate total fitness as weighted sum
    org->total_fitness = (org->fitness[0] * 0.4 + 
                         org->fitness[1] * 0.3 + 
                         org->fitness[2] * 0.3);
}

void quantum_entangle_genomes(qrng_ctx *ctx, uint64_t *genome1, uint64_t *genome2) {
    uint8_t state1[8], state2[8];
    memcpy(state1, genome1, 8);
    memcpy(state2, genome2, 8);
    
    qrng_entangle_states(ctx, state1, state2, 8);
    
    memcpy(genome1, state1, 8);
    memcpy(genome2, state2, 8);
}

void evolve_population(qrng_ctx *ctx, ecosystem_t *eco) {
    organism_t new_pop[POPULATION_SIZE];
    
    // Tournament selection and evolution
    for(int i = 0; i < POPULATION_SIZE; i += 2) {
        // Select parents through tournament
        organism_t *parent1 = NULL, *parent2 = NULL;
        for(int t = 0; t < 3; t++) {
            int idx = qrng_uint64(ctx) % POPULATION_SIZE;
            if(!parent1 || eco->population[idx].total_fitness > parent1->total_fitness) {
                parent1 = &eco->population[idx];
            }
        }
        for(int t = 0; t < 3; t++) {
            int idx = qrng_uint64(ctx) % POPULATION_SIZE;
            if(!parent2 || eco->population[idx].total_fitness > parent2->total_fitness) {
                parent2 = &eco->population[idx];
            }
        }
        
        // Create two children through quantum crossover
        uint64_t mask = qrng_uint64(ctx);
        new_pop[i].genome = (parent1->genome & mask) | (parent2->genome & ~mask);
        new_pop[i+1].genome = (parent2->genome & mask) | (parent1->genome & ~mask);
        
        // Quantum entangle children
        quantum_entangle_genomes(ctx, &new_pop[i].genome, &new_pop[i+1].genome);
        
        // Apply quantum mutations
        double mutation_rate = 0.01 * (1.0 - eco->max_fitness); // Adaptive mutation
        if(qrng_double(ctx) < mutation_rate) {
            new_pop[i].genome ^= 1ULL << (qrng_uint64(ctx) % GENOME_LENGTH);
        }
        if(qrng_double(ctx) < mutation_rate) {
            new_pop[i+1].genome ^= 1ULL << (qrng_uint64(ctx) % GENOME_LENGTH);
        }
        
        // Calculate fitness for new organisms
        calculate_fitness(&new_pop[i]);
        calculate_fitness(&new_pop[i+1]);
    }
    
    // Update statistics
    double sum_fitness = 0;
    double sum_objectives[NUM_OBJECTIVES] = {0};
    eco->max_fitness = 0;
    
    for(int i = 0; i < NUM_OBJECTIVES; i++) {
        eco->objective_maxs[i] = 0;
    }
    
    for(int i = 0; i < POPULATION_SIZE; i++) {
        eco->population[i] = new_pop[i];
        sum_fitness += new_pop[i].total_fitness;
        
        if(new_pop[i].total_fitness > eco->max_fitness) {
            eco->max_fitness = new_pop[i].total_fitness;
        }
        
        for(int j = 0; j < NUM_OBJECTIVES; j++) {
            sum_objectives[j] += new_pop[i].fitness[j];
            if(new_pop[i].fitness[j] > eco->objective_maxs[j]) {
                eco->objective_maxs[j] = new_pop[i].fitness[j];
            }
        }
    }
    
    eco->avg_fitness = sum_fitness / POPULATION_SIZE;
    for(int i = 0; i < NUM_OBJECTIVES; i++) {
        eco->objective_avgs[i] = sum_objectives[i] / POPULATION_SIZE;
    }
    
    eco->generation++;
}

void print_progress_bar(double value, int width) {
    int pos = width * value;
    printf("[");
    for(int i = 0; i < width; i++) {
        if(i < pos) printf("=");
        else if(i == pos) printf(">");
        else printf(" ");
    }
    printf("] %.3f\n", value);
}

void print_generation_stats(const ecosystem_t *eco) {
    printf("\nGeneration %d:\n", eco->generation);
    printf("============\n");
    
    printf("Overall Fitness:\n");
    printf("  Average: ");
    print_progress_bar(eco->avg_fitness, 30);
    printf("  Maximum: ");
    print_progress_bar(eco->max_fitness, 30);
    
    printf("\nObjectives:\n");
    printf("1. Ones Count:\n   Avg: ");
    print_progress_bar(eco->objective_avgs[0], 20);
    printf("   Max: ");
    print_progress_bar(eco->objective_maxs[0], 20);
    
    printf("\n2. Alternations:\n   Avg: ");
    print_progress_bar(eco->objective_avgs[1], 20);
    printf("   Max: ");
    print_progress_bar(eco->objective_maxs[1], 20);
    
    printf("\n3. Symmetry:\n   Avg: ");
    print_progress_bar(eco->objective_avgs[2], 20);
    printf("   Max: ");
    print_progress_bar(eco->objective_maxs[2], 20);
    
    printf("\n");
}

int main() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)"evoseed", 7);
    
    ecosystem_t eco = {0};
    
    // Initialize population with quantum genomes
    for(int i = 0; i < POPULATION_SIZE; i++) {
        eco.population[i].genome = qrng_uint64(ctx);
        calculate_fitness(&eco.population[i]);
    }
    
    // Evolution loop
    printf("Starting Quantum Evolution Simulation...\n");
    printf("=======================================\n");
    
    for(int gen = 0; gen < GENERATIONS; gen++) {
        evolve_population(ctx, &eco);
        
        // Print detailed stats every 10 generations
        if(gen % 10 == 0 || gen == GENERATIONS-1) {
            print_generation_stats(&eco);
        }
    }
    
    // Print best organism
    printf("\nBest Organism:\n");
    printf("=============\n");
    organism_t *best = &eco.population[0];
    for(int i = 1; i < POPULATION_SIZE; i++) {
        if(eco.population[i].total_fitness > best->total_fitness) {
            best = &eco.population[i];
        }
    }
    
    printf("Genome: ");
    for(int i = 0; i < GENOME_LENGTH; i++) {
        printf("%d", (int)((best->genome >> i) & 1));
    }
    printf("\n");
    
    printf("Fitness Scores:\n");
    printf("- Ones Count: %.3f\n", best->fitness[0]);
    printf("- Alternations: %.3f\n", best->fitness[1]);
    printf("- Symmetry: %.3f\n", best->fitness[2]);
    printf("Total Fitness: %.3f\n", best->total_fitness);
    
    qrng_free(ctx);
    return 0;
}
