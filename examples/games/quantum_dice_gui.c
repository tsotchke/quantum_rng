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
    GtkWidget *dark_mode_toggle;
    
    quantum_dice_t *dice;
    qrng_ctx *qrng_ctx;
    int roll_count;
    gboolean dark_mode;
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

// Dark mode CSS styles
static const char *dark_mode_css = 
    "window { background-color: #2d2d30; color: #ffffff; }\n"
    "window headerbar { background-color: #2d2d30; color: #ffffff; }\n"
    "headerbar { background-color: #2d2d30; color: #ffffff; }\n"
    "headerbar button { color: #ffffff; }\n"
    "frame { background-color: #3c3c3c; border-color: #555555; }\n"
    "frame > label { color: #ffffff; }\n"
    "button { background: #0e639c; color: #ffffff; border: 1px solid #1177bb; }\n"
    "button * { color: #ffffff; }\n"
    "button:hover { background: #1177bb; }\n"
    "button:active { background: #094771; }\n"
    ".roll-button { background: #d2691e; color: #ffffff; font-weight: bold; }\n"
    ".roll-button:hover { background: #ff8c00; }\n"
    ".roll-button:active { background: #b8860b; }\n"
    ".theme-toggle { background: #404040; }\n"
    ".theme-toggle:hover { background: #505050; }\n"
    "combobox { background-color: #404040; }\n"
    "combobox button { background-color: #404040; color: #ffffff; }\n"
    "combobox button:hover { background-color: #505050; }\n"
    "combobox button * { color: #ffffff; }\n"
    "combobox cellview { color: #ffffff; background-color: #404040; }\n"
    "entry { background-color: #404040; color: #ffffff; border-color: #555555; }\n"
    "textview { background-color: #1e1e1e; color: #ffffff; }\n"
    "textview text { background-color: #1e1e1e; color: #ffffff; }\n"
    "label { color: #ffffff; }\n"
    "scrolledwindow { border-color: #555555; }\n"
    "popover { background-color: #3c3c3c; color: #ffffff; }\n"
    "popover * { color: #ffffff; }\n";

static const char *light_mode_css = 
    "window { background-color: #ffffff; color: #000000; }\n"
    "window headerbar { background-color: #ffffff; color: #000000; }\n"
    "headerbar { background-color: #ffffff; color: #000000; }\n"
    "headerbar button { color: #000000; }\n"
    "frame { background-color: #f5f5f5; border-color: #cccccc; }\n"
    "frame > label { color: #000000; }\n"
    "button { background: #e1e1e1; color: #000000; border: 1px solid #adadad; }\n"
    "button * { color: #000000; }\n"
    "button:hover { background: #e5f1fb; }\n"
    "button:active { background: #cce4f7; }\n"
    ".roll-button { background: #4caf50; color: #ffffff; font-weight: bold; }\n"
    ".roll-button:hover { background: #66bb6a; }\n"
    ".roll-button:active { background: #388e3c; }\n"
    ".theme-toggle { background: #f0f0f0; }\n"
    ".theme-toggle:hover { background: #e0e0e0; }\n"
    "combobox { background-color: #ffffff; }\n"
    "combobox button { background-color: #ffffff; color: #000000; }\n"
    "combobox button * { color: #000000; }\n"
    "combobox cellview { color: #000000; background-color: #ffffff; }\n"
    "entry { background-color: #ffffff; color: #000000; border-color: #cccccc; }\n"
    "textview { background-color: #ffffff; color: #000000; }\n"
    "textview text { background-color: #ffffff; color: #000000; }\n"
    "label { color: #000000; }\n"
    "scrolledwindow { border-color: #cccccc; }\n"
    "popover { background-color: #ffffff; color: #000000; }\n"
    "popover * { color: #000000; }\n";

static GtkCssProvider *css_provider = NULL;

static void apply_theme(DiceApp *app) {
    if (!css_provider) {
        css_provider = gtk_css_provider_new();
    }
    
    const char *css = app->dark_mode ? dark_mode_css : light_mode_css;
    
    GError *error = NULL;
    gtk_css_provider_load_from_data(css_provider, css, -1, &error);
    
    if (error) {
        g_print("Error loading CSS: %s\n", error->message);
        g_error_free(error);
        return;
    }
    
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    // Apply dark theme to GTK settings for window decorations
    GtkSettings *settings = gtk_settings_get_default();
    g_object_set(settings, 
                 "gtk-application-prefer-dark-theme", app->dark_mode,
                 NULL);
    
    // Force redraw
    gtk_widget_queue_draw(app->window);
}

static void on_dark_mode_toggled(GtkToggleButton *toggle, gpointer user_data) {
    DiceApp *app = (DiceApp *)user_data;
    app->dark_mode = gtk_toggle_button_get_active(toggle);
    apply_theme(app);
    
    // Update toggle button icon only
    const char *icon = app->dark_mode ? "☀️" : "🌙";
    gtk_button_set_label(GTK_BUTTON(toggle), icon);
}

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
        
        // Update result label with theme-appropriate emoji
        char text[100];
        const char *emoji = app->dark_mode ? "🌌" : "🎲";
        snprintf(text, sizeof(text), "%s Ready to roll %s %s", 
                emoji, dice_types[active].name, emoji);
        gtk_label_set_text(GTK_LABEL(app->result_label), text);
    }
}

static void on_roll_clicked(GtkButton *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    DiceApp *app = (DiceApp *)user_data;
    
    if (!app->dice) {
        const char *emoji = app->dark_mode ? "⚠️" : "⚠️";
        char text[100];
        snprintf(text, sizeof(text), "%s Please select a dice type first! %s", emoji, emoji);
        gtk_label_set_text(GTK_LABEL(app->result_label), text);
        return;
    }
    
    // Roll the dice
    int result = quantum_dice_roll(app->dice);
    int sides = quantum_dice_sides(app->dice);
    app->roll_count++;
    
    // Update result label with theme-appropriate styling
    char result_text[100];
    const char *emoji = app->dark_mode ? "🌌✨" : "🎲🎯";
    snprintf(result_text, sizeof(result_text), 
             "%s Rolled: %d (d%d) %s", emoji, result, sides, emoji);
    gtk_label_set_text(GTK_LABEL(app->result_label), result_text);
    
    // Add to history with theme styling
    char history_text[200];
    const char *prefix = app->dark_mode ? "🌟" : "🎯";
    snprintf(history_text, sizeof(history_text), 
             "%s Roll #%d: %d (d%d)\n", prefix, app->roll_count, result, sides);
    
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
    
    const char *emoji = app->dark_mode ? "🌌" : "✨";
    char text[100];
    snprintf(text, sizeof(text), "%s History cleared - ready to roll! %s", emoji, emoji);
    gtk_label_set_text(GTK_LABEL(app->result_label), text);
}

static void create_gui(DiceApp *app) {
    // Create main window
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "🌌 Quantum Dice Roller");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 420, 550);
    gtk_container_set_border_width(GTK_CONTAINER(app->window), 15);
    
    // Create main vertical box
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(app->window), vbox);
    
    // Header with title and dark mode toggle
    GtkWidget *header_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(vbox), header_hbox, FALSE, FALSE, 0);
    
    // Title label
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), 
                        "<big><b>🌌 Quantum Dice Roller</b></big>\n"
                        "<i>Powered by Quantum RNG</i>");
    gtk_label_set_justify(GTK_LABEL(title), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(header_hbox), title, TRUE, TRUE, 0);
    
    // Dark mode toggle - small icon button with CSS class
    app->dark_mode_toggle = gtk_toggle_button_new_with_label("🌙");
    gtk_widget_set_size_request(app->dark_mode_toggle, 40, 40);
    gtk_widget_set_tooltip_text(app->dark_mode_toggle, "Toggle Dark/Light Mode");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->dark_mode_toggle), "theme-toggle");
    gtk_box_pack_end(GTK_BOX(header_hbox), app->dark_mode_toggle, FALSE, FALSE, 0);
    
    // Dice selection
    GtkWidget *dice_frame = gtk_frame_new("Select Dice Type");
    gtk_box_pack_start(GTK_BOX(vbox), dice_frame, FALSE, FALSE, 0);
    
    app->dice_combo = gtk_combo_box_text_new();
    for (int i = 0; i < 7; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->dice_combo), 
                                      dice_types[i].name);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->dice_combo), 1); // Default to d6
    gtk_container_set_border_width(GTK_CONTAINER(dice_frame), 8);
    gtk_container_add(GTK_CONTAINER(dice_frame), app->dice_combo);
    
    // Roll button with CSS class for special styling
    app->roll_button = gtk_button_new_with_label("🎲 ROLL DICE 🎲");
    gtk_widget_set_size_request(app->roll_button, -1, 55);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->roll_button), "roll-button");
    gtk_box_pack_start(GTK_BOX(vbox), app->roll_button, FALSE, FALSE, 0);
    
    // Result display
    GtkWidget *result_frame = gtk_frame_new("Result");
    gtk_box_pack_start(GTK_BOX(vbox), result_frame, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(result_frame), 8);
    
    app->result_label = gtk_label_new("🎲 Select a dice type and click ROLL! 🎲");
    gtk_label_set_justify(GTK_LABEL(app->result_label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_size_request(app->result_label, -1, 55);
    gtk_container_add(GTK_CONTAINER(result_frame), app->result_label);
    
    // History section
    GtkWidget *history_frame = gtk_frame_new("Roll History");
    gtk_box_pack_start(GTK_BOX(vbox), history_frame, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(history_frame), 8);
    
    GtkWidget *history_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(history_frame), history_vbox);
    
    // Scrolled window for history
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, -1, 220);
    gtk_box_pack_start(GTK_BOX(history_vbox), scrolled, TRUE, TRUE, 0);
    
    app->history_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->history_text), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app->history_text), FALSE);
    app->history_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->history_text));
    gtk_container_add(GTK_CONTAINER(scrolled), app->history_text);
    
    // Clear history button  
    GtkWidget *clear_button = gtk_button_new_with_label("🗑️ Clear History");
    gtk_box_pack_start(GTK_BOX(history_vbox), clear_button, FALSE, FALSE, 0);
    
    // Connect signals
    g_signal_connect(app->dice_combo, "changed", G_CALLBACK(on_dice_changed), app);
    g_signal_connect(app->roll_button, "clicked", G_CALLBACK(on_roll_clicked), app);
    g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_history), app);
    g_signal_connect(app->dark_mode_toggle, "toggled", G_CALLBACK(on_dark_mode_toggled), app);
    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Initialize theme (light mode by default)
    app->dark_mode = FALSE;
    apply_theme(app);
    
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
    
    if (css_provider) {
        g_object_unref(css_provider);
    }
    
    return 0;
}