# Quantum Dice Musical


# Install audio tools on Debian
sudo apt update

# Install ALSA utilities (for aplay and speaker-test)
sudo apt install alsa-utils

# Install SoX (Swiss Army knife of audio)
sudo apt install sox

# Install PulseAudio utilities
sudo apt install pulseaudio-utils

# Optional: Install FFmpeg for more audio options
sudo apt install ffmpeg


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