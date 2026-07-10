#include "../../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <math.h>

#define WORLD_SIZE 128
#define OCTAVES 4

typedef struct {
    float height[WORLD_SIZE][WORLD_SIZE];
    float temperature[WORLD_SIZE][WORLD_SIZE];
    float moisture[WORLD_SIZE][WORLD_SIZE];
} world_map;

float quantum_noise2d(qrng_ctx *ctx, int x, int y, float frequency) {
    float nx = x * frequency;
    float ny = y * frequency;
    
    uint64_t hash = qrng_uint64(ctx);
    float rx = (float)(hash & 0xFFFFFFFF) / UINT32_MAX;
    float ry = (float)(hash >> 32) / UINT32_MAX;
    
    return cos(nx * rx + ny * ry) * 0.5 + 0.5;
}

void generate_terrain(qrng_ctx *ctx, world_map *map) {
    for(int y = 0; y < WORLD_SIZE; y++) {
        for(int x = 0; x < WORLD_SIZE; x++) {
            float amplitude = 1.0;
            float frequency = 1.0;
            float height = 0;
            float amplitude_sum = 0;

            for(int o = 0; o < OCTAVES; o++) {
                height += quantum_noise2d(ctx, x, y, frequency) * amplitude;
                amplitude_sum += amplitude;
                amplitude *= 0.5;
                frequency *= 2.0;
            }

            // Normalize by the sum of octave amplitudes (1 + 1/2 + ... = 1.875
            // for 4 octaves) so height spans [0, 1]. Dividing by 2^OCTAVES - 1
            // (a previous bug) squashed everything below the ocean threshold.
            map->height[y][x] = height / amplitude_sum;
            map->temperature[y][x] = quantum_noise2d(ctx, x, y, 0.02);
            map->moisture[y][x] = quantum_noise2d(ctx, x + 1000, y + 1000, 0.03);
        }
    }
}

char get_biome_char(float height, float temp, float moisture) {
    if(height < 0.3) return '~';  // Ocean
    if(height < 0.4) return ',';  // Beach
    
    if(temp < 0.2) {             // Cold biomes
        if(moisture < 0.3) return '.';  // Tundra
        return '*';                     // Snow
    }
    
    if(temp < 0.4) {             // Cool biomes
        if(moisture < 0.3) return 'o';  // Grassland
        return 'T';                     // Taiga
    }
    
    if(temp < 0.7) {             // Temperate biomes
        if(moisture < 0.3) return '-';  // Plains
        if(moisture < 0.6) return 'f';  // Forest
        return 'F';                     // Dense forest
    }
    
    // Hot biomes
    if(moisture < 0.2) return '.';      // Desert
    if(moisture < 0.4) return 's';      // Savanna
    if(moisture < 0.7) return 'r';      // Rainforest
    return 'R';                         // Dense rainforest
}

void print_world(const world_map *map) {
    printf("\nProcedural World Generation:\n");
    printf("===========================\n\n");
    
    for(int y = 0; y < WORLD_SIZE; y += 2) {
        for(int x = 0; x < WORLD_SIZE; x += 2) {
            char biome = get_biome_char(
                map->height[y][x],
                map->temperature[y][x],
                map->moisture[y][x]
            );
            printf("%c", biome);
        }
        printf("\n");
    }
    
    printf("\nLegend:\n");
    printf("~ Ocean   , Beach   . Desert/Tundra   - Plains\n");
    printf("o Grass   f Forest  F Dense Forest    * Snow\n");
    printf("s Savanna r Rain    R Dense Rain      T Taiga\n");
}

int main() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)"worldseed", 9);
    
    world_map map;
    generate_terrain(ctx, &map);
    print_world(&map);
    
    qrng_free(ctx);
    return 0;
}
