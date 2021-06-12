# Arduino VGA Terminal

Use an Arduino Nano as a terminal for your homebrew computer
- VGA video output of 320x480 pixels at 60 Hz 
- 25 lines of text with 40 text characters each, on an 8x8 pixel font

Adapted from https://github.com/slu4coder/ArduinoVGA


The terminal supports 6-bit color output (2 bits per RGB channel), for a total of 64 colors.

**Guide of available colors:**

![Colors](https://github.com/p-rivero/ArduinoVGA/blob/main/IMG/6bit_colors.jpg?raw=true)

> **Example:** Calling `SetColor(LIGHTLIME)` will set the current line color to `#aaff55`, and calling `SetColor(DDARKBLUE)` will set the current line color to `#000055`.

## How to compile:

- Set Arduino Nano FuseA from default 0xFF to 0xBF. [See original video](https://youtu.be/Id3VYybrcws?t=269).
- Install `gcc-avr` using `sudo apt install gcc-avr`
- Install `avrdude`.
    - If you are using native Linux, use `sudo apt install avrdude`
    - If you are using Windows WSL2, it *cannot* access COM ports. [Download WinAVR](https://sourceforge.net/projects/winavr/) and call `avrdude` from `CMD`.
- Compile using the makefile

You could also use the Arduino IDE, but in my case it caused extreme flickering and instablilty.


## Required hardware:

- The ICs used in [the original video series](https://youtu.be/jUoR5ICCepA)
- 2 x 74HC08
- 3 x 680 Ohm resistor
- 3 x 1 KOhm resistor

Connect the 6 color bits to pins A0-A5, as indicated in the schematic:

![Schematic](https://github.com/p-rivero/ArduinoVGA/blob/main/IMG/Schematic.jpg?raw=true)


## Demo:

![Demo](https://github.com/p-rivero/ArduinoVGA/blob/main/IMG/demo_6bit.jpg?raw=true)
