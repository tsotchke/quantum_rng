# Quantum Dice Musical

# Compile the musical dice
gcc -Wall -Wextra -O3 -I. -o build/quantum_dice_musical \
    examples/games/quantum_dice_musical.c \
    examples/games/quantum_dice.c \
    -Lbuild -lquantumrng \
    `pkg-config --cflags --libs gtk+-3.0` -lm

# Run it
LD_LIBRARY_PATH=build ./build/quantum_dice_musical

Usage:

- Select dice type (d4-d20)
- Adjust speed and volume
- Click PLAY to start auto-rolling
- Listen to the quantum musical sequence!
- Stop anytime to change settings


This creates a mesmerizing quantum music generator where each dice roll produces a unique tone, creating evolving musical sequences! 🎵🎲🌌