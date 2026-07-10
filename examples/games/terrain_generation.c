#include "terrain_generation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Quantum noise functions
float quantum_noise2d(qrng_ctx *ctx, int x, int y, float frequency) {
    double dx = x * frequency;
    double dy = y * frequency;
    
    uint64_t hash = qrng_uint64(ctx);
    double rx = (double)(hash & 0xFFFFFFFF) / UINT32_MAX;
    double ry = (double)(hash >> 32) / UINT32_MAX;
    
    return (float)(cos(dx * rx + dy * ry) * 0.5 + 0.5);
}

float quantum_ridged_noise(qrng_ctx *ctx, int x, int y, float frequency) {
    return 1.0f - fabsf(quantum_noise2d(ctx, x, y, frequency) * 2.0f - 1.0f);
}

// Terrain generation functions
void generate_base_terrain(terrain_map_t *terrain, qrng_ctx *ctx) {
    // Multi-octave quantum noise for heightmap
    for (int y = 0; y < MAP_SIZE; y++) {
        for (int x = 0; x < MAP_SIZE; x++) {
            float amplitude = 1.0f;
            float frequency = 0.01f;
            float height = 0;
            float height_norm = 0;

            // Regular noise for base terrain
            for (int octave = 0; octave < 6; octave++) {
                height += quantum_noise2d(ctx, x, y, frequency) * amplitude;
                height_norm += amplitude;
                amplitude *= 0.5f;
                frequency *= 2.0f;
            }
            height /= height_norm;  // Normalize octave sum to [0, 1]

            // Ridged noise for mountains
            amplitude = 0.5f;
            frequency = 0.02f;
            float mountain = 0;
            float mountain_norm = 0;
            for (int octave = 0; octave < 4; octave++) {
                mountain += quantum_ridged_noise(ctx, x, y, frequency) * amplitude;
                mountain_norm += amplitude;
                amplitude *= 0.5f;
                frequency *= 2.0f;
            }
            mountain /= mountain_norm;  // Normalize to [0, 1]

            // Combine height and mountain noise; result stays in [0, MAX_HEIGHT]
            terrain->cells[y][x].height = (height * 0.7f + mountain * 0.3f) * MAX_HEIGHT;

            // Generate climate data
            terrain->cells[y][x].moisture = quantum_noise2d(ctx, x, y, 0.005f);
            terrain->cells[y][x].temperature =
                quantum_noise2d(ctx, x + 1000, y + 1000, 0.005f) -
                (terrain->cells[y][x].height / MAX_HEIGHT) * 0.5f; // Temperature decreases with height

            // No river here yet; generate_rivers() marks river tiles with 0.
            // (Leaving this uninitialized made is_water() read garbage.)
            terrain->cells[y][x].river_distance = RIVER_NONE;
            terrain->cells[y][x].cave_density = 0.0f;
        }
    }
}

void generate_rivers(terrain_map_t *terrain, qrng_ctx *ctx) {
    // Start rivers from high points
    for (int i = 0; i < terrain->num_rivers; i++) {
        int x = qrng_uint64(ctx) % MAP_SIZE;
        int y = qrng_uint64(ctx) % MAP_SIZE;
        
        // Find high point in local area
        float max_height = terrain->cells[y][x].height;
        int start_x = x, start_y = y;
        
        for (int dx = -10; dx <= 10; dx++) {
            for (int dy = -10; dy <= 10; dy++) {
                int nx = (x + dx + MAP_SIZE) % MAP_SIZE;
                int ny = (y + dy + MAP_SIZE) % MAP_SIZE;
                if (terrain->cells[ny][nx].height > max_height) {
                    max_height = terrain->cells[ny][nx].height;
                    start_x = nx;
                    start_y = ny;
                }
            }
        }
        
        // Carve river path
        x = start_x;
        y = start_y;
        float river_strength = 1.0f;
        
        while (river_strength > 0.1f && terrain->cells[y][x].height > terrain->sea_level) {
            // Mark river path
            terrain->cells[y][x].river_distance = 0;
            terrain->cells[y][x].moisture += river_strength * 0.2f;
            
            // Find lowest neighbor
            int lowest_x = x, lowest_y = y;
            float lowest_height = terrain->cells[y][x].height;
            
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = (x + dx + MAP_SIZE) % MAP_SIZE;
                    int ny = (y + dy + MAP_SIZE) % MAP_SIZE;
                    
                    if (terrain->cells[ny][nx].height < lowest_height) {
                        lowest_height = terrain->cells[ny][nx].height;
                        lowest_x = nx;
                        lowest_y = ny;
                    }
                }
            }
            
            if (lowest_x == x && lowest_y == y) break;
            
            // Erode river path
            float erosion = river_strength * terrain->erosion_factor;
            terrain->cells[y][x].height -= erosion;
            
            x = lowest_x;
            y = lowest_y;
            river_strength *= 0.99f;
        }
    }
}

void generate_caves(terrain_map_t *terrain, qrng_ctx *ctx) {
    // Generate 3D noise for cave systems
    for (int y = 0; y < MAP_SIZE; y++) {
        for (int x = 0; x < MAP_SIZE; x++) {
            float cave = 0;
            float amplitude = 1.0f;
            float frequency = 0.05f;
            
            for (int octave = 0; octave < 4; octave++) {
                cave += quantum_noise2d(ctx, x, y + 2000, frequency) * amplitude;
                amplitude *= 0.5f;
                frequency *= 2.0f;
            }
            
            terrain->cells[y][x].cave_density = cave;
        }
    }
}

void determine_biomes(terrain_map_t *terrain) {
    for (int y = 0; y < MAP_SIZE; y++) {
        for (int x = 0; x < MAP_SIZE; x++) {
            terrain_cell_t *cell = &terrain->cells[y][x];
            float height = cell->height;
            float moisture = cell->moisture;
            float temperature = cell->temperature;
            
            if (height < terrain->sea_level) {
                cell->biome = BIOME_OCEAN;
            }
            else if (height < terrain->sea_level + 10.0f) {
                cell->biome = BIOME_BEACH;
            }
            else if (height > terrain->mountain_level) {
                cell->biome = BIOME_MOUNTAIN;
            }
            else if (temperature < 0.2f) {
                cell->biome = BIOME_SNOW;
            }
            else if (temperature < 0.4f) {
                cell->biome = BIOME_TUNDRA;
            }
            else if (moisture < 0.2f) {
                cell->biome = BIOME_DESERT;
            }
            else if (moisture < 0.4f) {
                cell->biome = BIOME_GRASSLAND;
            }
            else if (moisture < 0.6f) {
                cell->biome = BIOME_FOREST;
            }
            else {
                cell->biome = BIOME_RAINFOREST;
            }
        }
    }
}

// Main generation function
void generate_terrain(terrain_map_t *terrain, qrng_ctx *ctx) {
    // Initialize parameters
    terrain->sea_level = MAX_HEIGHT * 0.4f;
    terrain->mountain_level = MAX_HEIGHT * 0.8f;
    terrain->num_rivers = 5;
    terrain->erosion_factor = 0.1f;
    
    // Generate base terrain and climate
    generate_base_terrain(terrain, ctx);
    
    // Add features
    generate_rivers(terrain, ctx);
    generate_caves(terrain, ctx);
    
    // Determine biomes
    determine_biomes(terrain);
}

// Utility functions for game integration
float get_height(const terrain_map_t *terrain, int x, int y) {
    x = (x + MAP_SIZE) % MAP_SIZE;
    y = (y + MAP_SIZE) % MAP_SIZE;
    return terrain->cells[y][x].height;
}

float get_slope(const terrain_map_t *terrain, int x, int y) {
    float center = get_height(terrain, x, y);
    float max_slope = 0;
    
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            float slope = fabsf(get_height(terrain, x + dx, y + dy) - center);
            if (slope > max_slope) max_slope = slope;
        }
    }
    
    return max_slope;
}

int is_buildable(const terrain_map_t *terrain, int x, int y) {
    terrain_cell_t cell = terrain->cells[(y + MAP_SIZE) % MAP_SIZE]
                                        [(x + MAP_SIZE) % MAP_SIZE];
    return cell.height > terrain->sea_level && 
           cell.height < terrain->mountain_level &&
           cell.cave_density < 0.5f &&
           get_slope(terrain, x, y) < 30.0f;
}

biome_type get_biome(const terrain_map_t *terrain, int x, int y) {
    x = (x + MAP_SIZE) % MAP_SIZE;
    y = (y + MAP_SIZE) % MAP_SIZE;
    return terrain->cells[y][x].biome;
}

float get_moisture(const terrain_map_t *terrain, int x, int y) {
    x = (x + MAP_SIZE) % MAP_SIZE;
    y = (y + MAP_SIZE) % MAP_SIZE;
    return terrain->cells[y][x].moisture;
}

float get_temperature(const terrain_map_t *terrain, int x, int y) {
    x = (x + MAP_SIZE) % MAP_SIZE;
    y = (y + MAP_SIZE) % MAP_SIZE;
    return terrain->cells[y][x].temperature;
}

int is_water(const terrain_map_t *terrain, int x, int y) {
    x = (x + MAP_SIZE) % MAP_SIZE;
    y = (y + MAP_SIZE) % MAP_SIZE;
    return terrain->cells[y][x].height < terrain->sea_level ||
           terrain->cells[y][x].river_distance == 0.0f;
}

// ============================================================================
// DEMO DRIVER
// ============================================================================

static char biome_char(biome_type b) {
    switch (b) {
        case BIOME_OCEAN:      return '~';
        case BIOME_BEACH:      return ',';
        case BIOME_DESERT:     return '.';
        case BIOME_GRASSLAND:  return 'g';
        case BIOME_FOREST:     return 'f';
        case BIOME_RAINFOREST: return 'R';
        case BIOME_TUNDRA:     return 't';
        case BIOME_SNOW:       return '*';
        case BIOME_MOUNTAIN:   return '^';
        default:               return '?';
    }
}

int main(void) {
    printf("Quantum Terrain Generation Demo\n");
    printf("===============================\n\n");

    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx, (uint8_t*)"terrainseed", 11);
    if (err != QRNG_SUCCESS) {
        fprintf(stderr, "Failed to initialize QRNG: %s\n", qrng_error_string(err));
        return 1;
    }

    // ~1.6 MB map: allocate on the heap rather than the stack
    terrain_map_t *terrain = malloc(sizeof(terrain_map_t));
    if (!terrain) {
        fprintf(stderr, "Out of memory\n");
        qrng_free(ctx);
        return 1;
    }

    generate_terrain(terrain, ctx);

    // Render a downsampled biome overview (every 4th cell)
    printf("Biome map (%dx%d, downsampled 4x):\n\n", MAP_SIZE, MAP_SIZE);
    for (int y = 0; y < MAP_SIZE; y += 4) {
        for (int x = 0; x < MAP_SIZE; x += 4) {
            putchar(biome_char(terrain->cells[y][x].biome));
        }
        putchar('\n');
    }
    printf("\nLegend: ~ ocean  , beach  . desert  g grass  f forest\n");
    printf("        R rainforest  t tundra  * snow  ^ mountain\n");

    // Measured statistics (counted from the generated map, not assumed)
    int biome_counts[NUM_BIOMES] = {0};
    int river_tiles = 0, water_tiles = 0, buildable_tiles = 0;
    float min_h = MAX_HEIGHT, max_h = 0.0f;
    double sum_h = 0.0;

    for (int y = 0; y < MAP_SIZE; y++) {
        for (int x = 0; x < MAP_SIZE; x++) {
            const terrain_cell_t *c = &terrain->cells[y][x];
            biome_counts[c->biome]++;
            if (c->river_distance == 0.0f) river_tiles++;
            if (is_water(terrain, x, y)) water_tiles++;
            if (is_buildable(terrain, x, y)) buildable_tiles++;
            if (c->height < min_h) min_h = c->height;
            if (c->height > max_h) max_h = c->height;
            sum_h += c->height;
        }
    }

    const int total = MAP_SIZE * MAP_SIZE;
    static const char *biome_names[NUM_BIOMES] = {
        "Ocean", "Beach", "Desert", "Grassland", "Forest",
        "Rainforest", "Tundra", "Snow", "Mountain"
    };

    printf("\nTerrain statistics (%d cells):\n", total);
    printf("  Height: min %.1f, mean %.1f, max %.1f (sea level %.1f)\n",
           min_h, sum_h / total, max_h, terrain->sea_level);
    printf("  Rivers carved: %d tiles from %d sources\n",
           river_tiles, terrain->num_rivers);
    printf("  Water tiles: %d (%.1f%%)\n", water_tiles, 100.0 * water_tiles / total);
    printf("  Buildable tiles: %d (%.1f%%)\n",
           buildable_tiles, 100.0 * buildable_tiles / total);
    printf("  Biomes:\n");
    for (int b = 0; b < NUM_BIOMES; b++) {
        printf("    %-11s %6d (%.1f%%)\n",
               biome_names[b], biome_counts[b], 100.0 * biome_counts[b] / total);
    }

    free(terrain);
    qrng_free(ctx);

    printf("\nTerrain generation completed successfully.\n");
    return 0;
}
