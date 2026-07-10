#ifndef WEATHER_SIM_H
#define WEATHER_SIM_H

#include "../../src/quantum_rng/quantum_rng.h"

/*
 * Quantum weather simulation demo.
 *
 * A grid-based toy weather model: moving pressure systems perturb
 * temperature, wind, humidity, cloud cover and precipitation on a
 * 2-D grid, with quantum randomness driving system placement,
 * movement, strength evolution and per-cell fluctuations.
 */

#define GRID_SIZE 50
#define TIME_STEPS 24  // Hours to simulate
#define NUM_PRESSURE_SYSTEMS 5

typedef struct {
    double temperature;    // Celsius
    double pressure;       // hPa
    double humidity;       // Percentage
    double wind_speed;     // m/s
    double wind_dir;       // Degrees
    double precipitation;  // mm/hour
    double cloud_cover;    // Percentage
} cell_t;

typedef struct {
    double x, y;           // Position
    double strength;       // hPa deviation from normal
    double radius;         // Grid cells
    double movement_x;     // Cells per hour
    double movement_y;     // Cells per hour
} pressure_system_t;

typedef struct {
    cell_t grid[GRID_SIZE][GRID_SIZE];
    pressure_system_t pressure_systems[NUM_PRESSURE_SYSTEMS];
    double avg_temperature;
    double avg_pressure;
    double total_precipitation;
    double max_wind_speed;
} weather_grid_t;

/* Initialization */
void init_pressure_system(qrng_ctx *ctx, pressure_system_t *system);
void init_weather(qrng_ctx *ctx, weather_grid_t *weather, double base_temp);

/* Simulation */
void apply_pressure_system(const pressure_system_t *system, int x, int y,
                           cell_t *cell);
void update_weather(qrng_ctx *ctx, weather_grid_t *weather);

/* Reporting */
void print_weather_stats(const weather_grid_t *weather);
void print_pressure_systems(const weather_grid_t *weather);

#endif /* WEATHER_SIM_H */
