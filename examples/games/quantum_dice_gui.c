#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quantum_dice.h"
#include "../../src/quantum_rng/quantum_rng.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *dice_combo;
    GtkWidget *roll_button;
    GtkWidget *result_label;
    GtkWidget *history_text;
    GtkTextBuffer *history_buffer;
    
    quantum_dice_t *dice;
    qrng_ctx *qrng_ctx;
    int roll_count;
} DiceApp;

// Dice types
typedef struct {
    const char *name;
    int sides;
} DiceType;

static DiceType dice_types[] = {
    {"d4 (4-sided)", 4},
    {"d6 (6-sided)", 6},
    {"d8 (8-sided)", 8},
    {"d10 (10-sided)", 10},
    {"d12 (12-sided)", 12},
    {"d20 (20-sided)", 20},
    {"d100 (100-sided)", 100}
};

static void on_dice_changed(GtkComboBox *combo, gpointer user_data) {
    DiceApp *app = (DiceApp *)user_data;
    int active = gtk_combo_box_get_active(combo);
    
    if (active >= 0 && active < 7) {
        // Free old dice
        if (app->dice) {
            quantum_dice_free(app->dice);
        }
        
        // Create new dice with selected sides
        app->dice = quantum_dice_create(app->qrng_ctx, dice_types[active].sides);
        
        // Update result label
        char text[100];
        snprintf(text, sizeof(text), "Ready to roll %s", dice_types[active].name);
        gtk_label_set_text(GTK_LABEL(app->result_label), text);
    }
}

static void on_roll_clicked(GtkButton *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    DiceApp *app = (DiceApp *)user_data;
    
    if (!app->dice) {
        gtk_label_set_text(GTK_LABEL(app->result_label), "Please select a dice type first!");
        return;
    }
    
    // Roll the dice
    int result = quantum_dice_roll(app->dice);
    int sides = quantum_dice_sides(app->dice);
    app->roll_count++;
    
    // Update result label with animation-like effect
    char result_text[100];
    snprintf(result_text, sizeof(result_text), 
             "🎲 Rolled: %d (d%d) 🎲", result, sides);
    gtk_label_set_text(GTK_LABEL(app->result_label), result_text);
    
    // Add to history
    char history_text[200];
    snprintf(history_text, sizeof(history_text), 
             "Roll #%d: %d (d%d)\n", app->roll_count, result, sides);
    
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(app->history_buffer, &end);
    gtk_text_buffer_insert(app->history_buffer, &end, history_text, -1);
    
    // Auto-scroll to bottom
    GtkTextView *text_view = GTK_TEXT_VIEW(app->history_text);
    gtk_text_view_scroll_to_iter(text_view, &end, 0.0, FALSE, 0.0, 0.0);
}

static void on_clear_history(GtkButton *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    DiceApp *app = (DiceApp *)user_data;
    gtk_text_buffer_set_text(app->history_buffer, "", -1);
    app->roll_count = 0;
    gtk_label_set_text(GTK_LABEL(app->result_label), "History cleared - ready to roll!");
}

static void create_gui(DiceApp *app) {
    // Create main window
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "🌌 Quantum Dice Roller");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 400, 500);
    gtk_container_set_border_width(GTK_CONTAINER(app->window), 10);
    
    // Create main vertical box
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(app->window), vbox);
    
    // Title label
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), 
                        "<big><b>🌌 Quantum Dice Roller</b></big>\n"
                        "<i>Powered by Quantum RNG</i>");
    gtk_label_set_justify(GTK_LABEL(title), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);
    
    // Dice selection
    GtkWidget *dice_frame = gtk_frame_new("Select Dice Type");
    gtk_box_pack_start(GTK_BOX(vbox), dice_frame, FALSE, FALSE, 0);
    
    app->dice_combo = gtk_combo_box_text_new();
    for (int i = 0; i < 7; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->dice_combo), 
                                      dice_types[i].name);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->dice_combo), 1); // Default to d6
    gtk_container_add(GTK_CONTAINER(dice_frame), app->dice_combo);
    
    // Roll button
    app->roll_button = gtk_button_new_with_label("🎲 ROLL DICE 🎲");
    gtk_widget_set_size_request(app->roll_button, -1, 50);
    gtk_box_pack_start(GTK_BOX(vbox), app->roll_button, FALSE, FALSE, 0);
    
    // Result display
    GtkWidget *result_frame = gtk_frame_new("Result");
    gtk_box_pack_start(GTK_BOX(vbox), result_frame, FALSE, FALSE, 0);
    
    app->result_label = gtk_label_new("Select a dice type and click ROLL!");
    gtk_label_set_justify(GTK_LABEL(app->result_label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_size_request(app->result_label, -1, 50);
    gtk_container_add(GTK_CONTAINER(result_frame), app->result_label);
    
    // History section
    GtkWidget *history_frame = gtk_frame_new("Roll History");
    gtk_box_pack_start(GTK_BOX(vbox), history_frame, TRUE, TRUE, 0);
    
    GtkWidget *history_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(history_frame), history_vbox);
    
    // Scrolled window for history
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, -1, 200);
    gtk_box_pack_start(GTK_BOX(history_vbox), scrolled, TRUE, TRUE, 0);
    
    app->history_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->history_text), FALSE);
    app->history_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->history_text));
    gtk_container_add(GTK_CONTAINER(scrolled), app->history_text);
    
    // Clear history button
    GtkWidget *clear_button = gtk_button_new_with_label("Clear History");
    gtk_box_pack_start(GTK_BOX(history_vbox), clear_button, FALSE, FALSE, 0);
    
    // Connect signals
    g_signal_connect(app->dice_combo, "changed", G_CALLBACK(on_dice_changed), app);
    g_signal_connect(app->roll_button, "clicked", G_CALLBACK(on_roll_clicked), app);
    g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_history), app);
    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Initialize with default dice (d6)
    on_dice_changed(GTK_COMBO_BOX(app->dice_combo), app);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    DiceApp app = {0};
    
    // Initialize quantum RNG with proper parameters
    qrng_error result = qrng_init(&app.qrng_ctx, NULL, 0);
    if (result != QRNG_SUCCESS || !app.qrng_ctx) {
        g_print("Failed to initialize Quantum RNG!\n");
        return 1;
    }
    
    // Create GUI
    create_gui(&app);
    
    // Show all widgets
    gtk_widget_show_all(app.window);
    
    // Start GTK main loop
    gtk_main();
    
    // Cleanup
    if (app.dice) {
        quantum_dice_free(app.dice);
    }
    qrng_free(app.qrng_ctx);
    
    return 0;
}