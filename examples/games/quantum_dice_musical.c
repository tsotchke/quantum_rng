#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include "quantum_dice.h"
#include "../../src/quantum_rng/quantum_rng.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *dice_combo;
    GtkWidget *play_button;
    GtkWidget *stop_button;
    GtkWidget *speed_scale;
    GtkWidget *volume_scale;
    GtkWidget *current_roll_label;
    GtkWidget *current_note_label;
    GtkWidget *history_text;
    GtkTextBuffer *history_buffer;
    GtkWidget *dark_mode_toggle;
    
    quantum_dice_t *dice;
    qrng_ctx *qrng_ctx;
    guint timer_id;
    int roll_count;
    gboolean is_playing;
    gboolean dark_mode;
    double speed_seconds;
    double volume;
} MusicalDiceApp;

// Musical note frequencies (Hz) - chromatic scale starting from C4
static const double note_frequencies[] = {
    261.63, // C4 (1)
    277.18, // C#4 (2)
    293.66, // D4 (3)
    311.13, // D#4 (4)
    329.63, // E4 (5)
    349.23, // F4 (6)
    369.99, // F#4 (7)
    392.00, // G4 (8)
    415.30, // G#4 (9)
    440.00, // A4 (10)
    466.16, // A#4 (11)
    493.88, // B4 (12)
    523.25, // C5 (13)
    554.37, // C#5 (14)
    587.33, // D5 (15)
    622.25, // D#5 (16)
    659.25, // E5 (17)
    698.46, // F5 (18)
    739.99, // F#5 (19)
    783.99  // G5 (20)
};

static const char *note_names[] = {
    "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4",
    "A#4", "B4", "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5"
};

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
    {"d20 (20-sided)", 20}
};

// Play a tone using system beep or aplay
static void play_tone(double frequency, double duration, double volume) {
    // Method 1: Try using speaker-test (most reliable)
    char command[256];
    int freq_int = (int)frequency;
    int duration_ms = (int)(duration * 1000);
    
    // Generate a simple sine wave tone using speaker-test
    snprintf(command, sizeof(command), 
             "timeout 0.5s speaker-test -t sine -f %d >/dev/null 2>&1 &", 
             freq_int);
    
    if (system(command) != 0) {
        // Method 2: Try using pactl (PulseAudio)
        snprintf(command, sizeof(command),
                 "pactl upload-sample /dev/zero tone && "
                 "pactl play-sample tone >/dev/null 2>&1 &");
        
        if (system(command) != 0) {
            // Method 3: Simple beep fallback
            printf("\a"); // System beep
            fflush(stdout);
        }
    }
    
    // Alternative: Create a sine wave and play with aplay
    // This is more complex but gives better control
    (void)duration; // Suppress unused warning for now
    (void)volume;   // Suppress unused warning for now
}

// Map dice roll to musical note
static int roll_to_note_index(int roll, int max_sides) {
    // Map roll to available notes (max 20 notes)
    int max_notes = sizeof(note_frequencies) / sizeof(note_frequencies[0]);
    
    if (max_sides <= max_notes) {
        return roll - 1; // Direct mapping for smaller dice
    } else {
        // Scale larger dice to available notes
        return ((roll - 1) * max_notes) / max_sides;
    }
}

// Auto-roll timer callback
static gboolean auto_roll_callback(gpointer user_data) {
    MusicalDiceApp *app = (MusicalDiceApp *)user_data;
    
    if (!app->is_playing || !app->dice) {
        return FALSE; // Stop timer
    }
    
    // Roll the dice
    int result = quantum_dice_roll(app->dice);
    int sides = quantum_dice_sides(app->dice);
    app->roll_count++;
    
    // Map to musical note
    int note_index = roll_to_note_index(result, sides);
    double frequency = note_frequencies[note_index];
    const char *note_name = note_names[note_index];
    
    // Play the tone
    play_tone(frequency, 0.3, app->volume);
    
    // Update display
    char roll_text[100];
    const char *emoji = app->dark_mode ? "🌌" : "🎲";
    snprintf(roll_text, sizeof(roll_text), 
             "%s Roll #%d: %d %s", emoji, app->roll_count, result, emoji);
    gtk_label_set_text(GTK_LABEL(app->current_roll_label), roll_text);
    
    char note_text[100];
    snprintf(note_text, sizeof(note_text), 
             "🎵 %s (%.1f Hz) 🎵", note_name, frequency);
    gtk_label_set_text(GTK_LABEL(app->current_note_label), note_text);
    
    // Add to history
    char history_text[200];
    const char *prefix = app->dark_mode ? "🌟" : "🎯";
    snprintf(history_text, sizeof(history_text), 
             "%s #%d: %d → %s (%.1fHz)\n", 
             prefix, app->roll_count, result, note_name, frequency);
    
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(app->history_buffer, &end);
    gtk_text_buffer_insert(app->history_buffer, &end, history_text, -1);
    
    // Auto-scroll to bottom
    GtkTextView *text_view = GTK_TEXT_VIEW(app->history_text);
    gtk_text_view_scroll_to_iter(text_view, &end, 0.0, FALSE, 0.0, 0.0);
    
    return TRUE; // Continue timer
}

// Start auto-rolling
static void on_play_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    MusicalDiceApp *app = (MusicalDiceApp *)user_data;
    
    if (!app->dice) {
        gtk_label_set_text(GTK_LABEL(app->current_roll_label), 
                          "⚠️ Please select a dice type first! ⚠️");
        return;
    }
    
    if (app->is_playing) {
        return; // Already playing
    }
    
    app->is_playing = TRUE;
    app->speed_seconds = gtk_range_get_value(GTK_RANGE(app->speed_scale));
    app->volume = gtk_range_get_value(GTK_RANGE(app->volume_scale));
    
    // Start timer
    guint interval_ms = (guint)(app->speed_seconds * 1000);
    app->timer_id = g_timeout_add(interval_ms, auto_roll_callback, app);
    
    // Update UI
    gtk_widget_set_sensitive(app->play_button, FALSE);
    gtk_widget_set_sensitive(app->stop_button, TRUE);
    gtk_widget_set_sensitive(app->speed_scale, FALSE);
    
    const char *emoji = app->dark_mode ? "🌌" : "🎵";
    char text[100];
    snprintf(text, sizeof(text), "%s Musical Dice Rolling... %s", emoji, emoji);
    gtk_label_set_text(GTK_LABEL(app->current_roll_label), text);
}

// Stop auto-rolling
static void on_stop_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    MusicalDiceApp *app = (MusicalDiceApp *)user_data;
    
    if (!app->is_playing) {
        return; // Not playing
    }
    
    app->is_playing = FALSE;
    
    // Stop timer
    if (app->timer_id > 0) {
        g_source_remove(app->timer_id);
        app->timer_id = 0;
    }
    
    // Update UI
    gtk_widget_set_sensitive(app->play_button, TRUE);
    gtk_widget_set_sensitive(app->stop_button, FALSE);
    gtk_widget_set_sensitive(app->speed_scale, TRUE);
    
    const char *emoji = app->dark_mode ? "🌌" : "🎲";
    char text[100];
    snprintf(text, sizeof(text), "%s Stopped - Ready to play %s", emoji, emoji);
    gtk_label_set_text(GTK_LABEL(app->current_roll_label), text);
    gtk_label_set_text(GTK_LABEL(app->current_note_label), "🎵 --- 🎵");
}

// Speed scale changed
static void on_speed_changed(GtkRange *range, gpointer user_data) {
    MusicalDiceApp *app = (MusicalDiceApp *)user_data;
    app->speed_seconds = gtk_range_get_value(range);
    
    // If currently playing, restart timer with new speed
    if (app->is_playing) {
        g_source_remove(app->timer_id);
        guint interval_ms = (guint)(app->speed_seconds * 1000);
        app->timer_id = g_timeout_add(interval_ms, auto_roll_callback, app);
    }
}

// Volume scale changed
static void on_volume_changed(GtkRange *range, gpointer user_data) {
    MusicalDiceApp *app = (MusicalDiceApp *)user_data;
    app->volume = gtk_range_get_value(range);
}

// Dice selection changed
static void on_dice_changed(GtkComboBox *combo, gpointer user_data) {
    MusicalDiceApp *app = (MusicalDiceApp *)user_data;
    int active = gtk_combo_box_get_active(combo);
    
    if (active >= 0 && active < 6) {
        // Stop if playing
        if (app->is_playing) {
            on_stop_clicked(NULL, app);
        }
        
        // Free old dice
        if (app->dice) {
            quantum_dice_free(app->dice);
        }
        
        // Create new dice
        app->dice = quantum_dice_create(app->qrng_ctx, dice_types[active].sides);
        
        // Update display
        char text[150];
        const char *emoji = app->dark_mode ? "🌌" : "🎲";
        snprintf(text, sizeof(text), 
                "%s Ready to play musical %s %s", 
                emoji, dice_types[active].name, emoji);
        gtk_label_set_text(GTK_LABEL(app->current_roll_label), text);
    }
}

// Clear history
static void on_clear_history(GtkButton *button, gpointer user_data) {
    (void)button;
    MusicalDiceApp *app = (MusicalDiceApp *)user_data;
    gtk_text_buffer_set_text(app->history_buffer, "", -1);
    app->roll_count = 0;
    
    const char *emoji = app->dark_mode ? "🌌" : "✨";
    char text[100];
    snprintf(text, sizeof(text), "%s History cleared %s", emoji, emoji);
    gtk_label_set_text(GTK_LABEL(app->current_roll_label), text);
}

// Dark mode toggle (simplified CSS)
static void on_dark_mode_toggled(GtkToggleButton *toggle, gpointer user_data) {
    MusicalDiceApp *app = (MusicalDiceApp *)user_data;
    app->dark_mode = gtk_toggle_button_get_active(toggle);
    
    // Simple theme switching
    GtkSettings *settings = gtk_settings_get_default();
    g_object_set(settings, 
                 "gtk-application-prefer-dark-theme", app->dark_mode,
                 NULL);
    
    // Update toggle button icon
    const char *icon = app->dark_mode ? "☀️" : "🌙";
    gtk_button_set_label(GTK_BUTTON(toggle), icon);
}

static void create_gui(MusicalDiceApp *app) {
    // Main window
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "🎵 Quantum Musical Dice 🎵");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 500, 650);
    gtk_container_set_border_width(GTK_CONTAINER(app->window), 15);
    
    // Main vertical box
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(app->window), vbox);
    
    // Header
    GtkWidget *header_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(vbox), header_hbox, FALSE, FALSE, 0);
    
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), 
                        "<big><b>🎵 Quantum Musical Dice 🎵</b></big>\n"
                        "<i>Auto-rolling dice with musical tones</i>");
    gtk_label_set_justify(GTK_LABEL(title), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(header_hbox), title, TRUE, TRUE, 0);
    
    // Dark mode toggle
    app->dark_mode_toggle = gtk_toggle_button_new_with_label("🌙");
    gtk_widget_set_size_request(app->dark_mode_toggle, 40, 40);
    gtk_widget_set_tooltip_text(app->dark_mode_toggle, "Toggle Dark/Light Mode");
    gtk_box_pack_end(GTK_BOX(header_hbox), app->dark_mode_toggle, FALSE, FALSE, 0);
    
    // Dice selection
    GtkWidget *dice_frame = gtk_frame_new("Dice Type");
    gtk_box_pack_start(GTK_BOX(vbox), dice_frame, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(dice_frame), 8);
    
    app->dice_combo = gtk_combo_box_text_new();
    for (int i = 0; i < 6; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->dice_combo), 
                                      dice_types[i].name);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->dice_combo), 1); // Default to d6
    gtk_container_add(GTK_CONTAINER(dice_frame), app->dice_combo);
    
    // Controls
    GtkWidget *controls_frame = gtk_frame_new("Controls");
    gtk_box_pack_start(GTK_BOX(vbox), controls_frame, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(controls_frame), 8);
    
    GtkWidget *controls_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(controls_grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(controls_grid), 8);
    gtk_container_add(GTK_CONTAINER(controls_frame), controls_grid);
    
    // Play/Stop buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app->play_button = gtk_button_new_with_label("▶️ PLAY");
    app->stop_button = gtk_button_new_with_label("⏹️ STOP");
    gtk_widget_set_sensitive(app->stop_button, FALSE);
    gtk_box_pack_start(GTK_BOX(button_box), app->play_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), app->stop_button, TRUE, TRUE, 0);
    gtk_grid_attach(GTK_GRID(controls_grid), button_box, 0, 0, 2, 1);
    
    // Speed control
    GtkWidget *speed_label = gtk_label_new("Speed (seconds):");
    gtk_grid_attach(GTK_GRID(controls_grid), speed_label, 0, 1, 1, 1);
    app->speed_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.1, 3.0, 0.1);
    gtk_range_set_value(GTK_RANGE(app->speed_scale), 1.0);
    gtk_scale_set_digits(GTK_SCALE(app->speed_scale), 1);
    gtk_grid_attach(GTK_GRID(controls_grid), app->speed_scale, 1, 1, 1, 1);
    
    // Volume control
    GtkWidget *volume_label = gtk_label_new("Volume:");
    gtk_grid_attach(GTK_GRID(controls_grid), volume_label, 0, 2, 1, 1);
    app->volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.1);
    gtk_range_set_value(GTK_RANGE(app->volume_scale), 0.5);
    gtk_scale_set_digits(GTK_SCALE(app->volume_scale), 1);
    gtk_grid_attach(GTK_GRID(controls_grid), app->volume_scale, 1, 2, 1, 1);
    
    // Current status
    GtkWidget *status_frame = gtk_frame_new("Current Status");
    gtk_box_pack_start(GTK_BOX(vbox), status_frame, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(status_frame), 8);
    
    GtkWidget *status_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(status_frame), status_vbox);
    
    app->current_roll_label = gtk_label_new("🎲 Select dice and click PLAY 🎲");
    gtk_box_pack_start(GTK_BOX(status_vbox), app->current_roll_label, FALSE, FALSE, 0);
    
    app->current_note_label = gtk_label_new("🎵 --- 🎵");
    gtk_box_pack_start(GTK_BOX(status_vbox), app->current_note_label, FALSE, FALSE, 0);
    
    // History
    GtkWidget *history_frame = gtk_frame_new("Musical Roll History");
    gtk_box_pack_start(GTK_BOX(vbox), history_frame, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(history_frame), 8);
    
    GtkWidget *history_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(history_frame), history_vbox);
    
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, -1, 200);
    gtk_box_pack_start(GTK_BOX(history_vbox), scrolled, TRUE, TRUE, 0);
    
    app->history_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->history_text), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app->history_text), FALSE);
    app->history_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->history_text));
    gtk_container_add(GTK_CONTAINER(scrolled), app->history_text);
    
    GtkWidget *clear_button = gtk_button_new_with_label("🗑️ Clear History");
    gtk_box_pack_start(GTK_BOX(history_vbox), clear_button, FALSE, FALSE, 0);
    
    // Connect signals
    g_signal_connect(app->dice_combo, "changed", G_CALLBACK(on_dice_changed), app);
    g_signal_connect(app->play_button, "clicked", G_CALLBACK(on_play_clicked), app);
    g_signal_connect(app->stop_button, "clicked", G_CALLBACK(on_stop_clicked), app);
    g_signal_connect(app->speed_scale, "value-changed", G_CALLBACK(on_speed_changed), app);
    g_signal_connect(app->volume_scale, "value-changed", G_CALLBACK(on_volume_changed), app);
    g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_history), app);
    g_signal_connect(app->dark_mode_toggle, "toggled", G_CALLBACK(on_dark_mode_toggled), app);
    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Initialize
    app->dark_mode = FALSE;
    app->is_playing = FALSE;
    app->speed_seconds = 1.0;
    app->volume = 0.5;
    on_dice_changed(GTK_COMBO_BOX(app->dice_combo), app);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    MusicalDiceApp app = {0};
    
    // Initialize quantum RNG
    qrng_error result = qrng_init(&app.qrng_ctx, NULL, 0);
    if (result != QRNG_SUCCESS || !app.qrng_ctx) {
        g_print("Failed to initialize Quantum RNG!\n");
        return 1;
    }
    
    create_gui(&app);
    gtk_widget_show_all(app.window);
    gtk_main();
    
    // Cleanup
    if (app.is_playing && app.timer_id > 0) {
        g_source_remove(app.timer_id);
    }
    if (app.dice) {
        quantum_dice_free(app.dice);
    }
    qrng_free(app.qrng_ctx);
    
    return 0;
}