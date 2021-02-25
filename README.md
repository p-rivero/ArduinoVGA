# Arduino VGA Terminal

Use an Arduino Nano as a terminal for your homebrew computer
- VGA video output of 320x480 pixels at 60 Hz 
- 25 lines of text with 40 text characters each, on an 8x8 pixel font

Adapted from https://github.com/slu4coder/ArduinoVGA

## Legacy branch

The `VGA_Terminal.cpp` program in both the `main` and the `legacy` branches is an adaptation from Slu4's original sketch, with improved stability and readability.
However, the `legacy` branch is compatible with the original ESC codes* **and uses the same ICs and pin connections as in the video**, while the `main` branch will be used for adding new features (so it won't be compatible with programs designed for the original terminal).

\* The only exceptions are the `ESC[C` and `ESC[D` codes, which didn't follow the ANSI standard and have been modified. Programs that relied on that incorrect behaviour will not work.

## How to compile:

- Set Arduino Nano FuseA from default 0xFF to 0xBF. [See original video](https://youtu.be/Id3VYybrcws?t=269).
- Install `gcc-avr` using `sudo apt install gcc-avr`
- Install `avrdude`.
    - If you are using native Linux, use `sudo apt install avrdude`
    - If you are using Windows WSL2, it *cannot* access COM ports. [Download WinAVR](https://sourceforge.net/projects/winavr/) and call `avrdude` from `CMD`.
- Compile using the makefile

You could also use the Arduino IDE, but in my case it caused extreme flickering and instablilty.
