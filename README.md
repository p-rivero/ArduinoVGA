# Arduino VGA Terminal

Use an Arduino Nano as a terminal for your homebrew computer
- VGA video output of 320x480 pixels at 60 Hz 
- 25 lines of text with 40 text characters each, on an 8x8 pixel font

Adapted from https://github.com/slu4coder/ArduinoVGA

## How to compile:

- Set Arduino Nano FuseA from default 0xFF to 0xBF. [See original video](https://youtu.be/Id3VYybrcws?t=269).
- Install `gcc-avr` using `sudo apt install gcc-avr`
- Install `avrdude`.
    - If you are using native Linux, use `sudo apt install avrdude`
    - If you are using Windows WSL2, it *cannot* access COM ports. [Download WinAVR](https://sourceforge.net/projects/winavr/) and call `avrdude` from `CMD`.
- Compile using the makefile

You could also use the Arduino IDE, but in my case it caused extreme flickering and instablilty.


## Color output:

Now the terminal supports 4-bit color output. See demo below:

![Demo](https://github.com/p-rivero/ArduinoVGA/blob/main/IMG/demo.jpg?raw=true)

> **Note:** Black is also an available color. The line between Light magenta and Light grey is set to black and its text is: `This line is black and you cannot see it`
