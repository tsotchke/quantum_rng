# Gtk Gui for testing options

# Install GTK3 development libraries
sudo apt update
sudo apt install libgtk-3-dev pkg-config

# Compile the fixed GUI version
gcc -Wall -Wextra -O3 -I. -o build/quantum_dice_gui \
    examples/games/quantum_dice_gui.c \
    examples/games/quantum_dice.c \
    -Lbuild -lquantumrng \
    `pkg-config --cflags --libs gtk+-3.0` -lm


# Run the quantum dice GUI

``` LD_LIBRARY_PATH=build ./build/quantum_dice_gui ```

New Dark Mode Features:

🌙 Dark Mode Toggle

Toggle button in the top-right corner
Instantly switches between light and dark themes
Button label changes: "🌙 Dark Mode" ↔ "☀️ Light Mode"

🎨 Theme Styling

Dark Mode: Deep space theme with dark backgrounds, bright text
Light Mode: Clean, bright interface
Custom CSS for buttons, frames, text areas, and more

✨ Theme-Aware Content

Roll button changes: "🎲 ROLL DICE 🎲" ↔ "🌌 QUANTUM ROLL 🌌"
Different emojis for dark/light themes
History entries styled with theme-appropriate icons

🚀 Enhanced UI

Larger window size (420x550)
Better spacing and padding
More polished button sizes
Improved visual hierarchy

🎨 Enhanced CSS Selectors

!important: Forces color override for button text
button label: Specifically targets button text labels
combobox styling: Comprehensive dropdown styling including hover states
popover: Styles the dropdown menu that appears

🌙 Dark Mode Fixes

Button text: color: #ffffff !important forces white text
Dropdown text: Proper styling for both button and menu items
Frame labels: Frame titles now properly colored
Arrows: Dropdown arrows properly colored

☀️ Light Mode Consistency

Button text: color: #000000 !important forces black text
Complete styling: All elements properly styled for light theme