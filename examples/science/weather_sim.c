#include "weather_sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Constants for weather simulation
const double STANDARD_PRESSURE = 1013.25;  // hPa
const double TEMPERATURE_RANGE = 40.0;     // ±20°C from average
const double MAX_WIND_SPEED = 30.0;        // m/s
const double MAX_PRECIPITATION = 50.0;     // mm/hour

// Initialize pressure system with quantum randomness
void init_pressure_system(qrng_ctx *ctx, pressure_system_t *system) {
    system->x = qrng_double(ctx) * GRID_SIZE;
    system->y = qrng_double(ctx) * GRID_SIZE;
    system->strength = (qrng_double(ctx) * 2.0 - 1.0) * 30.0;  // ±30 hPa
    system->radius = 5.0 + qrng_double(ctx) * 15.0;  // 5-20 cells
    
    // Random movement vector
    double angle = qrng_double(ctx) * 2.0 * M_PI;
    double speed = 0.5 + qrng_double(ctx) * 1.5;  // 0.5-2.0 cells per hour
    system->movement_x = cos(angle) * speed;
    system->movement_y = sin(angle) * speed;
}

// Initialize weather grid with quantum randomness
void init_weather(qrng_ctx *ctx, weather_grid_t *weather, double base_temp) {
    weather->avg_temperature = base_temp;
    weather->avg_pressure = STANDARD_PRESSURE;
    weather->total_precipitation = 0.0;
    weather->max_wind_speed = 0.0;
    
    // Initialize pressure systems
    for(int i = 0; i < NUM_PRESSURE_SYSTEMS; i++) {
        init_pressure_system(ctx, &weather->pressure_systems[i]);
    }
    
    // Initialize grid with base values
    for(int y = 0; y < GRID_SIZE; y++) {
        for(int x = 0; x < GRID_SIZE; x++) {
            cell_t *cell = &weather->grid[y][x];
            
            // Add some random variation to base temperature
            cell->temperature = base_temp + (qrng_double(ctx) * 2.0 - 1.0) * 5.0;
            cell->pressure = STANDARD_PRESSURE;
            cell->humidity = 30.0 + qrng_double(ctx) * 40.0;
            cell->wind_speed = qrng_double(ctx) * 5.0;
            cell->wind_dir = qrng_double(ctx) * 360.0;
            cell->precipitation = 0.0;
            cell->cloud_cover = qrng_double(ctx) * 30.0;
        }
    }
}

// Calculate pressure system influence on a cell
void apply_pressure_system(const pressure_system_t *system, int x, int y, cell_t *cell) {
    double dx = x - system->x;
    double dy = y - system->y;
    double distance = sqrt(dx*dx + dy*dy);
    
    if(distance < system->radius) {
        double influence = (1.0 - distance/system->radius) * system->strength;
        cell->pressure += influence;
        
        // Wind speed increases with pressure gradient
        double wind_factor = fabs(influence) / 10.0;
        cell->wind_speed += wind_factor;
        
        // Wind direction points around pressure system
        cell->wind_dir = fmod(atan2(dy, dx) * 180.0/M_PI + 360.0, 360.0);
        
        // Humidity and cloud cover affected by pressure
        if(influence < 0) {  // Low pressure: more clouds and humidity
            cell->humidity += wind_factor * 10.0;
            cell->cloud_cover += wind_factor * 15.0;
        } else {  // High pressure: clearer skies
            cell->humidity -= wind_factor * 5.0;
            cell->cloud_cover -= wind_factor * 10.0;
        }
    }
}

// Update weather conditions using quantum randomness
void update_weather(qrng_ctx *ctx, weather_grid_t *weather) {
    // Move pressure systems
    for(int i = 0; i < NUM_PRESSURE_SYSTEMS; i++) {
        pressure_system_t *system = &weather->pressure_systems[i];
        system->x += system->movement_x;
        system->y += system->movement_y;
        
        // Wrap around grid edges
        system->x = fmod(system->x + GRID_SIZE, GRID_SIZE);
        system->y = fmod(system->y + GRID_SIZE, GRID_SIZE);
        
        // Slowly change system strength with quantum randomness
        system->strength += (qrng_double(ctx) * 2.0 - 1.0) * 2.0;
        if(fabs(system->strength) > 30.0) {
            system->strength *= 0.9;  // Decay strong systems
        }
    }
    
    // Update each cell
    for(int y = 0; y < GRID_SIZE; y++) {
        for(int x = 0; x < GRID_SIZE; x++) {
            cell_t *cell = &weather->grid[y][x];
            
            // Reset pressure to standard
            cell->pressure = STANDARD_PRESSURE;
            
            // Apply pressure system influences
            for(int i = 0; i < NUM_PRESSURE_SYSTEMS; i++) {
                apply_pressure_system(&weather->pressure_systems[i], x, y, cell);
            }
            
            // Add quantum randomness to conditions
            cell->temperature += (qrng_double(ctx) * 2.0 - 1.0) * 0.5;
            cell->wind_speed += (qrng_double(ctx) * 2.0 - 1.0) * 0.5;
            cell->humidity += (qrng_double(ctx) * 2.0 - 1.0) * 2.0;

            // Moisture cycle: without an evaporation source the humidity
            // decays monotonically and rain can never occur. Relax humidity
            // toward a 55% baseline, let high humidity condense into cloud
            // cover, and dissipate clouds slowly in dry air.
            cell->humidity += (55.0 - cell->humidity) * 0.05;
            if(cell->humidity > 60.0) {
                cell->cloud_cover += (cell->humidity - 60.0) * 0.15;
            } else {
                cell->cloud_cover *= 0.95;
            }

            // Constrain values to realistic ranges
            cell->humidity = fmax(0.0, fmin(100.0, cell->humidity));
            cell->wind_speed = fmax(0.0, fmin(MAX_WIND_SPEED, cell->wind_speed));
            cell->cloud_cover = fmax(0.0, fmin(100.0, cell->cloud_cover));

            // Calculate precipitation based on humidity and cloud cover
            if(cell->humidity > 80.0 && cell->cloud_cover > 60.0) {
                double precip_chance = (cell->humidity - 80.0) * (cell->cloud_cover - 60.0) / 1600.0;
                if(qrng_double(ctx) < precip_chance) {
                    cell->precipitation = qrng_double(ctx) * MAX_PRECIPITATION * precip_chance;
                    // Rain removes moisture and thins the cloud deck
                    cell->humidity -= cell->precipitation * 0.5;
                    cell->cloud_cover -= cell->precipitation * 1.0;
                    cell->humidity = fmax(0.0, cell->humidity);
                    cell->cloud_cover = fmax(0.0, cell->cloud_cover);
                } else {
                    cell->precipitation = 0.0;
                }
            } else {
                cell->precipitation = 0.0;
            }
            
            // Update statistics
            weather->total_precipitation += cell->precipitation;
            weather->max_wind_speed = fmax(weather->max_wind_speed, cell->wind_speed);
        }
    }
}

// Print weather statistics
void print_weather_stats(const weather_grid_t *weather) {
    double total_temp = 0.0;
    double total_pressure = 0.0;
    double total_humidity = 0.0;
    double total_cloud = 0.0;
    int rainy_cells = 0;
    
    for(int y = 0; y < GRID_SIZE; y++) {
        for(int x = 0; x < GRID_SIZE; x++) {
            const cell_t *cell = &weather->grid[y][x];
            total_temp += cell->temperature;
            total_pressure += cell->pressure;
            total_humidity += cell->humidity;
            total_cloud += cell->cloud_cover;
            if(cell->precipitation > 0.0) rainy_cells++;
        }
    }
    
    int total_cells = GRID_SIZE * GRID_SIZE;
    printf("\nWeather Statistics:\n");
    printf("Average Temperature: %.1f°C\n", total_temp / total_cells);
    printf("Average Pressure: %.1f hPa\n", total_pressure / total_cells);
    printf("Average Humidity: %.1f%%\n", total_humidity / total_cells);
    printf("Average Cloud Cover: %.1f%%\n", total_cloud / total_cells);
    printf("Areas with Rain: %.1f%%\n", (double)rainy_cells / total_cells * 100.0);
    printf("Accumulated Precipitation: %.1f mm avg per cell\n",
           weather->total_precipitation / total_cells);
    printf("Maximum Wind Speed: %.1f m/s\n", weather->max_wind_speed);
}

// Print pressure system information
void print_pressure_systems(const weather_grid_t *weather) {
    printf("\nPressure Systems:\n");
    for(int i = 0; i < NUM_PRESSURE_SYSTEMS; i++) {
        const pressure_system_t *system = &weather->pressure_systems[i];
        printf("System %d: Position (%.1f, %.1f), Strength %.1f hPa, ",
               i + 1, system->x, system->y, system->strength);
        printf("Movement (%.1f, %.1f)\n", system->movement_x, system->movement_y);
    }
}

int main(void) {
    qrng_ctx *ctx;
    if (qrng_init(&ctx, (uint8_t*)"weather", 7) != QRNG_SUCCESS) {
        fprintf(stderr, "Error: failed to initialize quantum RNG\n");
        return 1;
    }
    
    printf("Quantum Weather Simulation\n");
    printf("=========================\n");
    
    // Initialize with average temperature of 15°C
    weather_grid_t weather;
    init_weather(ctx, &weather, 15.0);
    
    // Run simulation
    printf("\nSimulating %d hours of weather...\n", TIME_STEPS);
    for(int hour = 0; hour < TIME_STEPS; hour++) {
        update_weather(ctx, &weather);
        
        if(hour % 6 == 0) {  // Print stats every 6 hours
            printf("\nHour %d:\n", hour);
            print_weather_stats(&weather);
            print_pressure_systems(&weather);
        }
    }
    
    printf("\nFinal Weather Conditions:\n");
    print_weather_stats(&weather);
    print_pressure_systems(&weather);
    
    qrng_free(ctx);
    return 0;
}
