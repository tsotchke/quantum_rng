#include "../../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <string.h>

#define MAX_ITEMS 100
#define MAX_AFFIXES 5
#define MAX_NAME_LEN 64

typedef enum {
    RARITY_COMMON,
    RARITY_UNCOMMON,
    RARITY_RARE,
    RARITY_EPIC,
    RARITY_LEGENDARY,
    NUM_RARITIES
} item_rarity;

typedef enum {
    SLOT_WEAPON,
    SLOT_ARMOR,
    SLOT_ACCESSORY,
    NUM_SLOTS
} equipment_slot;

typedef struct {
    char name[MAX_NAME_LEN];
    double damage_bonus;
    double defense_bonus;
    double health_bonus;
    double crit_chance;
    double crit_multiplier;
} affix_t;

typedef struct {
    char name[MAX_NAME_LEN];
    item_rarity rarity;
    equipment_slot slot;
    double base_value;
    int num_affixes;
    affix_t affixes[MAX_AFFIXES];
    double item_level;
} item_t;

// Item prefixes and suffixes for procedural name generation
const char *prefixes[] = {
    "Ancient", "Quantum", "Mystic", "Void", "Celestial",
    "Dragon", "Phoenix", "Storm", "Shadow", "Light"
};

const char *bases[] = {
    "Sword", "Shield", "Armor", "Ring", "Amulet",
    "Blade", "Plate", "Guard", "Band", "Charm"
};

const char *suffixes[] = {
    "of Power", "of Fortune", "of the Whale", "of Time",
    "of Destiny", "of the Void", "of Ages", "of Glory"
};

// Rarity weights and colors (for display)
const double rarity_weights[] = {0.60, 0.25, 0.10, 0.04, 0.01};
const char *rarity_names[] = {"Common", "Uncommon", "Rare", "Epic", "Legendary"};

// Affix pool
const affix_t affix_pool[] = {
    {"of Strength", 5.0, 0.0, 0.0, 0.0, 0.0},
    {"of Defense", 0.0, 5.0, 0.0, 0.0, 0.0},
    {"of Vitality", 0.0, 0.0, 10.0, 0.0, 0.0},
    {"of Precision", 2.0, 0.0, 0.0, 5.0, 0.0},
    {"of Impact", 0.0, 0.0, 0.0, 0.0, 0.5},
    {"of the Bear", 2.0, 2.0, 5.0, 0.0, 0.0},
    {"of the Eagle", 3.0, 0.0, 0.0, 3.0, 0.3},
    {"of the Whale", 0.0, 3.0, 15.0, 0.0, 0.0},
    {"of the Snake", 0.0, 0.0, 0.0, 7.0, 0.4},
    {"of the Dragon", 4.0, 4.0, 4.0, 4.0, 0.4}
};

// Generate item rarity using quantum randomness
item_rarity generate_rarity(qrng_ctx *ctx) {
    double roll = qrng_double(ctx);
    double cumulative = 0;
    
    for(int i = 0; i < NUM_RARITIES; i++) {
        cumulative += rarity_weights[i];
        if(roll < cumulative) return i;
    }
    
    return RARITY_COMMON;
}

// Generate random affixes for an item
void generate_affixes(qrng_ctx *ctx, item_t *item) {
    int num_affixes = 1 + item->rarity;  // More affixes for higher rarities
    if(num_affixes > MAX_AFFIXES) num_affixes = MAX_AFFIXES;
    
    // Use quantum randomness to select unique affixes
    for(int i = 0; i < num_affixes; i++) {
        int affix_index;
        int attempts = 0;
        do {
            affix_index = qrng_uint64(ctx) % (sizeof(affix_pool) / sizeof(affix_pool[0]));
            
            // Check for duplicates
            int duplicate = 0;
            for(int j = 0; j < i; j++) {
                if(strcmp(item->affixes[j].name, affix_pool[affix_index].name) == 0) {
                    duplicate = 1;
                    break;
                }
            }
            
            if(!duplicate) {
                item->affixes[i] = affix_pool[affix_index];
                break;
            }
        } while(++attempts < 100);  // Prevent infinite loops
    }
    
    item->num_affixes = num_affixes;
}

// Generate item name using quantum randomness
void generate_item_name(qrng_ctx *ctx, item_t *item) {
    const char *prefix = prefixes[qrng_uint64(ctx) % (sizeof(prefixes) / sizeof(prefixes[0]))];
    const char *base = bases[qrng_uint64(ctx) % (sizeof(bases) / sizeof(bases[0]))];
    const char *suffix = suffixes[qrng_uint64(ctx) % (sizeof(suffixes) / sizeof(suffixes[0]))];
    
    if(item->rarity >= RARITY_RARE) {
        snprintf(item->name, MAX_NAME_LEN, "%s %s %s", prefix, base, suffix);
    } else {
        snprintf(item->name, MAX_NAME_LEN, "%s %s", base, suffix);
    }
}

// Generate complete random item
item_t generate_item(qrng_ctx *ctx, double item_level) {
    item_t item = {0};
    item.item_level = item_level;
    
    // Generate basic properties
    item.rarity = generate_rarity(ctx);
    item.slot = qrng_uint64(ctx) % NUM_SLOTS;
    
    // Base value scales with item level and rarity
    item.base_value = item_level * (1.0 + item.rarity * 0.5);
    
    // Generate name and affixes
    generate_item_name(ctx, &item);
    generate_affixes(ctx, &item);
    
    return item;
}

// Critical hit system
typedef struct {
    double base_chance;
    double bonus_chance;
    double base_multiplier;
    double bonus_multiplier;
} crit_stats_t;

// Calculate critical hit using quantum randomness
double calculate_damage(qrng_ctx *ctx, double base_damage, crit_stats_t *stats) {
    // Use quantum randomness for crit determination
    double crit_roll = qrng_double(ctx);
    double total_crit_chance = stats->base_chance + stats->bonus_chance;
    
    if(crit_roll < total_crit_chance) {
        // Critical hit! Use quantum randomness for damage variation
        double multiplier = stats->base_multiplier + stats->bonus_multiplier;
        double variation = 0.9 + 0.2 * qrng_double(ctx);  // ±10% variation
        return base_damage * multiplier * variation;
    }
    
    // Normal hit with small quantum variation
    return base_damage * (0.95 + 0.1 * qrng_double(ctx));
}

// Print item details
void print_item(const item_t *item) {
    printf("\n%s\n", item->name);
    printf("Level %d %s %s\n", 
           (int)item->item_level, 
           rarity_names[item->rarity],
           bases[item->slot]);
    printf("Base Value: %.1f\n", item->base_value);
    
    printf("Affixes:\n");
    for(int i = 0; i < item->num_affixes; i++) {
        printf("- %s\n", item->affixes[i].name);
        if(item->affixes[i].damage_bonus > 0)
            printf("  +%.1f%% Damage\n", item->affixes[i].damage_bonus);
        if(item->affixes[i].defense_bonus > 0)
            printf("  +%.1f%% Defense\n", item->affixes[i].defense_bonus);
        if(item->affixes[i].health_bonus > 0)
            printf("  +%.1f%% Health\n", item->affixes[i].health_bonus);
        if(item->affixes[i].crit_chance > 0)
            printf("  +%.1f%% Crit Chance\n", item->affixes[i].crit_chance);
        if(item->affixes[i].crit_multiplier > 0)
            printf("  +%.1fx Crit Multiplier\n", item->affixes[i].crit_multiplier);
    }
    printf("\n");
}

// Demonstrate loot system
void demonstrate_loot_system() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)"loot", 4);
    
    printf("Quantum Loot System Demo\n");
    printf("=======================\n\n");
    
    // Generate items at different levels
    printf("Low Level Items (Level 1-10):\n");
    for(int i = 0; i < 3; i++) {
        double level = 1.0 + qrng_double(ctx) * 9.0;
        item_t item = generate_item(ctx, level);
        print_item(&item);
    }
    
    printf("\nMid Level Items (Level 40-50):\n");
    for(int i = 0; i < 3; i++) {
        double level = 40.0 + qrng_double(ctx) * 10.0;
        item_t item = generate_item(ctx, level);
        print_item(&item);
    }
    
    printf("\nHigh Level Items (Level 90-100):\n");
    for(int i = 0; i < 3; i++) {
        double level = 90.0 + qrng_double(ctx) * 10.0;
        item_t item = generate_item(ctx, level);
        print_item(&item);
    }
    
    // Demonstrate critical hit system
    printf("\nCritical Hit System Demo\n");
    printf("=======================\n");
    
    crit_stats_t crit_stats = {
        .base_chance = 0.05,      // 5% base crit chance
        .bonus_chance = 0.15,     // +15% from gear
        .base_multiplier = 1.5,   // 150% base crit damage
        .bonus_multiplier = 0.8    // +80% from gear
    };
    
    double base_damage = 100.0;
    printf("\nBase Damage: %.1f\n", base_damage);
    printf("Crit Chance: %.1f%%\n", (crit_stats.base_chance + crit_stats.bonus_chance) * 100);
    printf("Crit Multiplier: %.1fx\n", crit_stats.base_multiplier + crit_stats.bonus_multiplier);
    
    printf("\nSample Attacks:\n");
    for(int i = 0; i < 10; i++) {
        double damage = calculate_damage(ctx, base_damage, &crit_stats);
        printf("Attack %d: %.1f damage", i + 1, damage);
        if(damage > base_damage * 1.1) printf(" (CRITICAL HIT!)");
        printf("\n");
    }
    
    qrng_free(ctx);
}

int main() {
    demonstrate_loot_system();
    return 0;
}
