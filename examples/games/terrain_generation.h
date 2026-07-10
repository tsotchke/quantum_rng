#ifndef TERRAIN_GENERATION_H
#define TERRAIN_GENERATION_H

#include "../../src/quantum_rng/quantum_rng.h"

/**
 * @file terrain_generation.h
 * @brief Quantum-noise terrain generation for games
 *
 * Generates a wrapping (toroidal) terrain map from multi-octave quantum
 * noise: heightmap with ridged mountain overlay, climate fields
 * (moisture/temperature), downhill-carved rivers, cave density, and a
 * biome classification. All randomness comes from the quantum RNG.
 */

#define MAP_SIZE 256
#define MAX_HEIGHT 1000.0f

typedef enum {
    BIOME_OCEAN,
    BIOME_BEACH,
    BIOME_DESERT,
    BIOME_GRASSLAND,
    BIOME_FOREST,
    BIOME_RAINFOREST,
    BIOME_TUNDRA,
    BIOME_SNOW,
    BIOME_MOUNTAIN,
    NUM_BIOMES
} biome_type;

typedef struct {
    float height;          /**< Elevation in [0, MAX_HEIGHT] (pre-erosion) */
    float moisture;        /**< Moisture in ~[0, 1] (rivers add extra) */
    float temperature;     /**< Temperature; decreases with elevation */
    float river_distance;  /**< 0 on a river tile, RIVER_NONE otherwise */
    float cave_density;    /**< Cave noise density in ~[0, 1] */
    biome_type biome;      /**< Classified biome */
} terrain_cell_t;

/** Sentinel for terrain_cell_t.river_distance: no river on this tile. */
#define RIVER_NONE 1.0e30f

typedef struct {
    terrain_cell_t cells[MAP_SIZE][MAP_SIZE];
    float sea_level;
    float mountain_level;
    int num_rivers;
    float erosion_factor;
} terrain_map_t;

// Quantum noise primitives
float quantum_noise2d(qrng_ctx *ctx, int x, int y, float frequency);
float quantum_ridged_noise(qrng_ctx *ctx, int x, int y, float frequency);

// Generation stages (generate_terrain runs all of them in order)
void generate_base_terrain(terrain_map_t *terrain, qrng_ctx *ctx);
void generate_rivers(terrain_map_t *terrain, qrng_ctx *ctx);
void generate_caves(terrain_map_t *terrain, qrng_ctx *ctx);
void determine_biomes(terrain_map_t *terrain);
void generate_terrain(terrain_map_t *terrain, qrng_ctx *ctx);

// Game-integration queries (coordinates wrap around the map edges)
float get_height(const terrain_map_t *terrain, int x, int y);
float get_slope(const terrain_map_t *terrain, int x, int y);
int is_buildable(const terrain_map_t *terrain, int x, int y);
biome_type get_biome(const terrain_map_t *terrain, int x, int y);
float get_moisture(const terrain_map_t *terrain, int x, int y);
float get_temperature(const terrain_map_t *terrain, int x, int y);
int is_water(const terrain_map_t *terrain, int x, int y);

#endif // TERRAIN_GENERATION_H
