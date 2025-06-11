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