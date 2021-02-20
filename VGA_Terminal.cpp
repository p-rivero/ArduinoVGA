// **********************************************************************************
// ***** ARDUINO NANO 40x25 ANSI DUMP (VT-100) TERMINAL by Carsten Herting 2020 *****
// **********************************************************************************
/* Adapted by P. Rivero
   What has been changed:
   - Translated some comments into English
   - Minor clarity improvements and code optimizations
*/

// LICENSE: MIT License (see link in the repo)
// Firmware Version 1.1 - 18.02.2021
// set Arduino Nano FuseA from default 0xFF to 0xBF (enabling output of 16MHz CLKO on pin 8)
// Pins 0-7:  Port D, used to write data to the parallel load of 74165, used to read data from 74174 input register
// Pin  8:    Arduino CLK out (16MHz)
// Pin  9:    74173 /OE
// Pin  10:   VSYNC (timer1)
// Pin  11:   74165 PE (timer2)
// Pin  12:   HSync "by hand" inside ISR
// Pin  13:   74173 MR

#include <avr/io.h>
#include <avr/interrupt.h>

typedef uint8_t byte;

volatile int vLine;         // current horizontal video line
volatile byte reg[128];     // Queue of data to be written into VRAM
volatile byte regout = 0;   // Index of current output position in queue
volatile byte regin = 0;    // Index of current input position in queue
volatile int mFrame = 0;    // 1/60s frame counter for cursor blinking
byte mEscValid = 0;         // Number of valid characters in mEscBuffer[]  
byte mEscBuffer[5];
byte mRow = 8;              // cursor position in terminal window
byte mCol = 0;              // cursor position in terminal window
byte oldc = ' ';            // stores char a cursor position

// GCC doesn't want to initialize a byte matrix with null-terminated strings, so we're stuck with this monstrosity
byte vram[25][40] = { 
    '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*',
    '*', ' ', ' ', 'A', 'T', 'M', 'e', 'g', 'a', '3', '2', '8', 'P', ' ', 'V', 'G', 'A', ' ', 'A', 'N', 'S', 'I', ' ', 'T', 'E', 'R', 'M', 'I', 'N', 'A', 'L', ' ', 'v', '1', '.', '0', '6', ' ', ' ', '*',
    '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*',
    '*', ' ', '-', ' ', '3', '2', '0', 'x', '4', '8', '0', ' ', 'p', 'i', 'x', 'e', 'l', 's', ' ', '/', ' ', '6', '0', 'H', 'z', ' ', 'r', 'e', 'f', 'r', 'e', 's', 'h', ' ', 'r', 'a', 't', 'e', ' ', '*',
    '*', ' ', '-', ' ', '4', '0', 'x', '2', '5', ' ', 'c', 'h', 'a', 'r', 'a', 'c', 't', 'e', 'r', 's', ' ', 'V', 'R', 'A', 'M', ' ', 't', 'e', 'x', 't', ' ', 'o', 'u', 't', 'p', 'u', 't', ' ', ' ', '*',
    '*', ' ', '-', ' ', 'f', 'u', 'l', 'l', ' ', '8', 'x', '8', ' ', 'p', 'i', 'x', 'e', 'l', ' ', 'A', 'S', 'C', 'I', 'I', ' ', 'c', 'h', 'a', 'r', 'a', 'c', 't', 'e', 'r', ' ', 's', 'e', 't', ' ', '*',
    '*', ' ', '-', ' ', 'p', 'r', 'o', 'c', 'e', 's', 's', 'i', 'n', 'g', ' ', 'A', 'N', 'S', 'I', ' ', 'e', 's', 'c', 'a', 'p', 'e', ' ', 's', 'e', 'q', 'u', 'e', 'n', 'c', 'e', 's', ' ', ' ', ' ', '*',
    '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
};

// my improved charset line data starting with character 32 (SPACE)
const byte charset[8][96] = {
// Turn off line wrap on your text editor!
//   ' '   '!'   '"'   '#'   '$'   '%'   '&'   '''   '('   ')'   '*'   '+'   ','   '-'   '.'   '/'   '0'   '1'   '2'   '3'   '4'   '5'   '6'   '7'   '8'   '9'   ':'   ';'   '<'   '='   '>'   '?'   '@'   'A'   'B'   'C'   'D'   'E'   'F'   'G'   'H'   'I'   'J'   'K'   'L'   'M'   'N'   'O'   'P'   'Q'   'R'   'S'   'T'   'U'   'V'   'W'   'X'   'Y'   'Z'   '['   '\'   ']'   '^'   '_'   '`'   'a'   'b'   'c'   'd'   'e'   'f'   'g'   'h'   'i'   'j'   'k'   'l'   'm'   'n'   'o'   'p'   'q'   'r'   's'   't'   'u'   'v'   'w'   'x'   'y'   'z'   '{'   '|'   '}'   '~'   '■'
    0x00, 0x18, 0x66, 0x66, 0x18, 0x62, 0x3C, 0x06, 0x0C, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x18, 0x3C, 0x3C, 0x06, 0x7E, 0x3C, 0x7E, 0x3C, 0x3C, 0x00, 0x00, 0x0E, 0x00, 0x70, 0x3C, 0x3C, 0x18, 0x7C, 0x3C, 0x78, 0x7E, 0x7E, 0x3C, 0x66, 0x3C, 0x1E, 0x66, 0x60, 0x63, 0x66, 0x3C, 0x7C, 0x3C, 0x7C, 0x3C, 0x7E, 0x66, 0x66, 0x63, 0x66, 0x66, 0x7E, 0x3C, 0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x18, 0x70, 0x00, 0xFF,
    0x00, 0x18, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x0C, 0x18, 0x18, 0x66, 0x18, 0x00, 0x00, 0x00, 0x03, 0x66, 0x18, 0x66, 0x66, 0x0E, 0x60, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x18, 0x00, 0x18, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x6C, 0x60, 0x60, 0x66, 0x66, 0x18, 0x0C, 0x6C, 0x60, 0x77, 0x76, 0x66, 0x66, 0x66, 0x66, 0x66, 0x18, 0x66, 0x66, 0x63, 0x66, 0x66, 0x06, 0x30, 0x60, 0x0C, 0x18, 0x00, 0x66, 0x00, 0x60, 0x00, 0x06, 0x00, 0x0E, 0x00, 0x60, 0x18, 0x06, 0x60, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0xFF,
    0x00, 0x18, 0x66, 0xFF, 0x60, 0x0C, 0x3C, 0x18, 0x30, 0x0C, 0x3C, 0x18, 0x00, 0x00, 0x00, 0x06, 0x6E, 0x38, 0x06, 0x06, 0x1E, 0x7C, 0x60, 0x0C, 0x66, 0x66, 0x18, 0x18, 0x30, 0x7E, 0x0C, 0x06, 0x6E, 0x66, 0x66, 0x60, 0x66, 0x60, 0x60, 0x60, 0x66, 0x18, 0x0C, 0x78, 0x60, 0x7F, 0x7E, 0x66, 0x66, 0x66, 0x66, 0x60, 0x18, 0x66, 0x66, 0x63, 0x3C, 0x66, 0x0C, 0x30, 0x30, 0x0C, 0x3C, 0x00, 0x6E, 0x3C, 0x60, 0x3C, 0x06, 0x3C, 0x18, 0x3E, 0x60, 0x00, 0x00, 0x60, 0x18, 0x66, 0x7C, 0x3C, 0x7C, 0x3E, 0x7C, 0x3E, 0x7E, 0x66, 0x66, 0x63, 0x66, 0x66, 0x7E, 0x18, 0x18, 0x18, 0x00, 0xFF,
    0x00, 0x18, 0x00, 0x66, 0x3C, 0x18, 0x38, 0x00, 0x30, 0x0C, 0xFF, 0x7E, 0x00, 0x7E, 0x00, 0x0C, 0x76, 0x18, 0x0C, 0x1C, 0x66, 0x06, 0x7C, 0x18, 0x3C, 0x3E, 0x00, 0x00, 0x60, 0x00, 0x06, 0x0C, 0x6E, 0x7E, 0x7C, 0x60, 0x66, 0x78, 0x78, 0x6E, 0x7E, 0x18, 0x0C, 0x70, 0x60, 0x6B, 0x7E, 0x66, 0x7C, 0x66, 0x7C, 0x3C, 0x18, 0x66, 0x66, 0x6B, 0x18, 0x3C, 0x18, 0x30, 0x18, 0x0C, 0x7E, 0x00, 0x6E, 0x06, 0x7C, 0x60, 0x3E, 0x66, 0x3E, 0x66, 0x7C, 0x38, 0x06, 0x6C, 0x18, 0x7F, 0x66, 0x66, 0x66, 0x66, 0x66, 0x60, 0x18, 0x66, 0x66, 0x6B, 0x3C, 0x66, 0x0C, 0x70, 0x18, 0x0E, 0x3B, 0xFF,
    0x00, 0x00, 0x00, 0xFF, 0x06, 0x30, 0x67, 0x00, 0x30, 0x0C, 0x3C, 0x18, 0x00, 0x00, 0x00, 0x18, 0x66, 0x18, 0x30, 0x06, 0x7F, 0x06, 0x66, 0x18, 0x66, 0x06, 0x00, 0x00, 0x30, 0x7E, 0x0C, 0x18, 0x60, 0x66, 0x66, 0x60, 0x66, 0x60, 0x60, 0x66, 0x66, 0x18, 0x0C, 0x78, 0x60, 0x63, 0x6E, 0x66, 0x60, 0x66, 0x78, 0x06, 0x18, 0x66, 0x66, 0x7F, 0x3C, 0x18, 0x30, 0x30, 0x0C, 0x0C, 0x18, 0x00, 0x60, 0x3E, 0x66, 0x60, 0x66, 0x7E, 0x18, 0x66, 0x66, 0x18, 0x06, 0x78, 0x18, 0x7F, 0x66, 0x66, 0x66, 0x66, 0x60, 0x3C, 0x18, 0x66, 0x66, 0x7F, 0x18, 0x66, 0x18, 0x18, 0x18, 0x18, 0x6E, 0xFF,
    0x00, 0x00, 0x00, 0x66, 0x7C, 0x66, 0x66, 0x00, 0x18, 0x18, 0x66, 0x18, 0x18, 0x00, 0x18, 0x30, 0x66, 0x18, 0x60, 0x66, 0x06, 0x66, 0x66, 0x18, 0x66, 0x66, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00, 0x66, 0x66, 0x66, 0x66, 0x6C, 0x60, 0x60, 0x66, 0x66, 0x18, 0x6C, 0x6C, 0x60, 0x63, 0x66, 0x66, 0x60, 0x3C, 0x6C, 0x66, 0x18, 0x66, 0x3C, 0x77, 0x66, 0x18, 0x60, 0x30, 0x06, 0x0C, 0x18, 0x00, 0x66, 0x66, 0x66, 0x60, 0x66, 0x60, 0x18, 0x3E, 0x66, 0x18, 0x06, 0x6C, 0x18, 0x6B, 0x66, 0x66, 0x7C, 0x3E, 0x60, 0x06, 0x18, 0x66, 0x3C, 0x3E, 0x3C, 0x3E, 0x30, 0x18, 0x18, 0x18, 0x00, 0xFF,
    0x00, 0x18, 0x00, 0x66, 0x18, 0x46, 0x3F, 0x00, 0x0C, 0x30, 0x00, 0x00, 0x18, 0x00, 0x18, 0x60, 0x3C, 0x7E, 0x7E, 0x3C, 0x06, 0x3C, 0x3C, 0x18, 0x3C, 0x3C, 0x00, 0x18, 0x0E, 0x00, 0x70, 0x18, 0x3C, 0x66, 0x7C, 0x3C, 0x78, 0x7E, 0x60, 0x3C, 0x66, 0x3C, 0x38, 0x66, 0x7E, 0x63, 0x66, 0x3C, 0x60, 0x0E, 0x66, 0x3C, 0x18, 0x3C, 0x18, 0x63, 0x66, 0x18, 0x7E, 0x3C, 0x03, 0x3C, 0x18, 0x00, 0x3C, 0x3E, 0x7C, 0x3C, 0x3E, 0x3C, 0x18, 0x06, 0x66, 0x3C, 0x06, 0x66, 0x3C, 0x63, 0x66, 0x3C, 0x60, 0x06, 0x60, 0x7C, 0x0E, 0x3E, 0x18, 0x36, 0x66, 0x0C, 0x7E, 0x0E, 0x18, 0x70, 0x00, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x18, 0x00, 0x00, 0xFF,
};

void setup() {
    cli();                  // disable interrupts before messing around with timer registers (same as noInterrupts();)

    DDRB  = 0b00111111;     // pin8: CLKO, pin 9: 74173 /OE, pin 10: VSYNC (timer1), pin 11: 74165 PE (timer2), pin 12: HSync "by hand" inside ISR, pin 13: 74173 MR
    PORTB = 0b00010010;     // MR=0, HSYNC=1, /OE=1
    GTCCR = 0b10000011;     // set TSM, PSRSYNC und PSRASY to correlate all 3 timers

    // *****************************
    // ***** Timer0: VGA HSYNC *****
    // *****************************
    TCNT0  = 6;
    TCCR0A = (1<<WGM01) | (0<<WGM00);               // mode 2: Clear Timer on Compare Match (CTC)
    TCCR0B = (0<<WGM02) | (0<<CS02) | (1<<CS01) | (0<<CS00); // x8 prescaler -> 0.5µs
    OCR0A  = 63;                                    // compare match register A (TOP) -> 32µs
    TIMSK0 = (1<<OCIE0A);                           // Output Compare Match A Interrupt Enable (not working: TOIE1 with ISR TIMER0_TOIE1_vect because it is already defined by timing functions)

    // *****************************
    // ***** Timer1: VGA VSYNC *****
    // *****************************
    TCNT1  = 0;
    TCCR1A = (1<<COM1B1) | (1<<COM1B0) | (1<<WGM11) | (1<<WGM10);         // mode 15 (Fast PWM), set OC1B on Compare Match, clear OC1B at BOTTOM, controlling OC1B pin 10
    TCCR1B = (1<<WGM13) | (1<<WGM12) | (1<<CS12) | (0<<CS11) | (1<<CS10); // x1024 prescaler -> 64µs
    OCR1A  = 259;                                   // compare match register A (TOP) -> 16.64ms
    OCR1B  = 0;                                     // compare match register B -> 64µs
    TIMSK1 = (1<<TOIE1);                            // enable timer overflow interrupt setting vLines = 0

    // *************************************************
    // ***** Timer2: 74165 Parallel Load Enable PE *****
    // *************************************************
    TCNT2  = 0;
    TCCR2A = (1<<COM2A1) | (1<<COM2A0) | (1<<WGM21) | (1<<WGM20); // mode 7: Fast PWM, COM2A0=0: normal port HIGH, COM2A0=1: Toggle OC2A pin 11 on Compare Match
    TCCR2B = (1<<WGM22) | (0<<CS22) | (0<<CS21) | (1<<CS20) ;     // set x0 prescaler -> 62.5ns;
    OCR2A  = 7;                                     // compare match register A (TOP) -> 250ns
    TIMSK2 = 0;                                     // no interrupts here

    GTCCR = 0;                                      // clear TSM => all timers start synchronously
    sei(); // (same as interrupts();)
}

// timer overflow interrupt sets back vLine at the start of a frame
ISR(TIMER1_OVF_vect) {
    vLine = 0;
    mFrame++;
}

ISR(TIMER0_COMPA_vect) {                // HSYNC generation and drawing of a scanline
    if (TCNT2 & 1) PORTB = 0b00010010;  // MR=0, HSYNC=1, /OE=1: Canceling out interrupt jitter using the fast timer (if branching, this code takes a cycle longer)
    PORTB = 0b00000000;                 // MR=0, HSYNC=0, /OE=0: Start the HSYNC pulse & activate 74173 output
    DDRD  = 0b00000000;                 // check the 74173 input register for new data, switch to D = input
    __builtin_avr_delay_cycles(1);      // IMPORTANT: Wait for the 74173 outputs to stabilize
    byte a = PIND;                      // read twice to capture possible misreads
    byte b = PIND;
    byte c = regin;
    
    if(a != 0) {                        // was anything !=0 read?
        if(a == b) {                    // was the reading valid?
            PORTB = 0b00100010;         // MR=1, HSYNC=0, /OE=1: Pull register reset and output
            reg[c++] = a;               // Store new value in the buffer
            c &= 0b1111111;
            regin = c;
        } else __builtin_avr_delay_cycles(13);  // ensure same amount of cycles on all branches 4/5/9   
    } else __builtin_avr_delay_cycles(15);      // ensure same amount of cycles on all branches          
    
    PORTB = 0b00000010;                         // MR=0, HSYNC=0, /OE=1: Disable register output

    DDRD  = 0b11111111;                         // switch port D to output
    PORTD = 0;                                  // prevent trash data from being fed into the shift register
    vLine++;                                    // use the time during the pulse to init pointers
    byte* vrow = vram[byte((vLine-36)>>4)];     // pointer to the vram row 0...24 to display
    const byte* cset = charset[byte((vLine-36)>>1) & 0b00000111] - 32; // pointer to the charset line 0..7 to use starting @ character 32
    PORTB = 0b00010010;                         // MR=0, HSYNC=1, /OE=1: End of HSYNC pulse

    if (vLine >= 36 && vLine < 436)             // skip 2 lines (VSYNC pulse) + 33 lines (vertical back porch)
    {  
        TCCR2A &= ~(1<<COM2A1);                 // enable OC2A toggling pin 11
        PORTD = cset[*vrow++];                  // start feeding data
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        PORTD = cset[*vrow++];
        __builtin_avr_delay_cycles(1);
        TCCR2A |= (1<<COM2A1);      // stop any further parallel load PE (disabling OC2A toggling pin 11)
        PORTD = 0;
    }
}


void Scroll() { 
    for (int r = 0; r < 24; r++)
        for (int c = 0; c < 40; c++)
            vram[r][c] = vram[r+1][c];  // move all rows one step up

    for (int c = 0; c < 40; c++)
        vram[24][c] = ' ';              // fill lowest line with SPACES
}

// processes a character (accepts some VT52/GEMDOS ESC sequences, control chars, normal chars)
void ProcessChar(byte inbyte) {
    mFrame = 25;
    if (mEscValid > 4) mEscValid = 0;   // delete invalid ESC sequence and process character normally
    if (inbyte == 27) {     // Start of an ESC sequence
        mEscValid = 1;
        return;
    }
    if (mEscValid > 0) {     // An ESC sequence has been initiated
        if (mEscValid < 2) { // The second character MUST be '['
            if (inbyte == '[') mEscValid++;
            else mEscValid = 0;

        } else {            // ESC + '[' has been recieved correctly
            mEscBuffer[mEscValid++] = inbyte;   // add character to the ESC sequence

            switch(inbyte) {    // The length of the ESC sequence must be checked on each case
            case 'A':   // move cursor up
            {
                byte amount;
                if (mEscValid == 5) amount = 10 * (mEscBuffer[2] - '0') + mEscBuffer[3] - '0';
                if (mEscValid == 4) amount = mEscBuffer[2] - '0';
                if (mEscValid == 3) amount = 1;
                
                for(byte i = 0; i < amount; i++) if (mRow > 0) mRow--; // TODO: try if using [ if (amount < mRow) mRow -= amount; else mRow = 0; ] works
                mEscValid = 0;
                break;
            }
            case 'B':   // move cursor down
            {
                byte amount;
                if (mEscValid == 5) amount = 10 * (mEscBuffer[2] - '0') + mEscBuffer[3] - '0';
                if (mEscValid == 4) amount = mEscBuffer[2] - '0';
                if (mEscValid == 3) amount = 1;
                for(byte i=0; i<amount; i++) if (mRow < 24) mRow++; // TODO: try if using [ if (mRow + amount < 24) mRow += amount; else mRow = 24; ] works
                
                mEscValid = 0;
                break;
            }
            case 'C':   // move cursor right
            {
                byte amount;
                if (mEscValid == 5) amount = 10 * (mEscBuffer[2] - '0') + mEscBuffer[3] - '0';
                if (mEscValid == 4) amount = mEscBuffer[2] - '0';
                if (mEscValid == 3) amount = 1;
                
                // TODO: ANSI standard specifies that if cursor DOES NOT CHANGE LINE and it stays on rightmost column.
                // todo: Implement like A i B types (40 columns)
                for (byte i = 0; i < amount; i++) {
                    if (mCol < 39) mCol++;
                    else if (mRow < 24) {
                        mCol = 0;
                        mRow++;
                    }
                }
                
                mEscValid = 0;
                break;
            }
            case 'D':   // move cursor left
            {
                byte amount;
                if (mEscValid == 5) amount = 10 * (mEscBuffer[2] - '0') + mEscBuffer[3] - '0';
                if (mEscValid == 4) amount = mEscBuffer[2] - '0';
                if (mEscValid == 3) amount = 1;
                // TODO: ANSI standard specifies that if cursor DOES NOT CHANGE LINE and it stays on leftmost column.
                // todo: Implement like A i B types (40 columns)
                for (byte i = 0; i < amount; i++) {
                    if (mCol > 0) mCol--;
                    else if (mRow > 0) {
                        mCol = 39;
                        mRow--;
                    }
                }
                mEscValid = 0;
                break;
            }
            case 'H':   // move cursor to upper left corner // TODO: Make sure 'f' type works and remove this one
                mRow = 0;
                mCol = 0;
                mEscValid = 0;
                break;
                
            case 'J':   // clear SCREEN from cursor onwards
                for (byte c = mCol; c < 40; c++)
                    vram[mRow][c] = ' ';
                for (byte r = mRow + 1; r < 25; r++)
                    for(byte c = 0; c < 40; c++)
                        vram[r][c] = ' ';
                mEscValid = 0;
                break;
                
            case 'K':   // clear LINE from cursor onwards
                for (byte c = mCol; c < 40; c++)
                    vram[mRow][c] = ' ';
                mEscValid = 0;
                break;
            
            // New escape codes:
            
            case 'f':   
            // case 'H':   // move cursor to specified coordinates. Format Esc[Line;ColumnH or Esc[;H
                mRow = 0;
                mCol = 0;
                
                if (mEscValid > 3) {    // Some arguments were provided
                    byte ArgNum = 2;    // Points to first argument character
                    
                    while (mEscBuffer[ArgNum] != ';') { // While argument isn't ';' read row number
                        mRow = mRow*10 + (mEscBuffer[ArgNum]) - '0';
                        ArgNum++;
                    }
                    ArgNum++; // Skip ';'
                    while (ArgNum < mEscValid) {    // While arguments remain (argument isn't 'f' or 'H'), read column
                        mCol = mCol*10 + (mEscBuffer[ArgNum]) - '0';
                        ArgNum++;
                    }
                    
                    // Check bounds (mRow and mCol can't be negative)
                    if (mRow > 24) mRow = 24;
                    if (mCol > 40) mCol = 40;
                }
                
                mEscValid = 0;
                break;
            
            // End of new escape codes
            
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': case ';':
                break;  // Number (or separator) has been added to the buffer. Don't do anything until the sequence is finished
                
            default:    // Illegal ESC code. Ignore it.
                mEscValid = 0;
                break;
            }
        }

    } else {    // An ESC sequence has NOT been initiated, process character normally
        switch(inbyte) {
        case '\n':  // Move to a new line
            mCol = 0;
            if (mRow < 24) mRow++;
            else Scroll();
            break;
            
        case '\r':  // Carriage return: move to the beginning of line
            mCol = 0;
            break;
            
        case '\t':  // Tab: Advance 4 positions // TODO: move to next multiple of 4
            if (mCol < 35) mCol += 4; else mCol = 39;
            break;
            
        case 8:     // Backspace: Remove 1 character (back)
            if (mCol > 0)
                vram[mRow][--mCol] = ' ';   // Remove from the same line
            else if (mRow > 0) {        // If not on the first line, remove from previous line
                mRow--;
                vram[mRow][39] = ' ';
                mCol = 39;
            }
            break;
            
        default: // No special character detected.
            if (inbyte >= ' ') { // Check if it's printable
                vram[mRow][mCol++] = inbyte;    // Just write the character to vram
                if (mCol > 39) {
                    mCol = 0;
                    if (mRow < 24) mRow++;
                    else Scroll();
                }
            }
            break;
        }
    }
}


void loop() { 
    // Todo: add ability to toggle cursor blinking
    if ((mFrame & 63) > 31) vram[mRow][mCol] = 127;
    else vram[mRow][mCol] = oldc;   // cursor blinking

    if (regout != regin) {              // was a character received
        vram[mRow][mCol] = oldc;        // restore character beneath the cursor BEFORE a possible scrolling happens
        do {
            ProcessChar(reg[regout]);   // process this character
            regout++;
            regout &= 0b1111111;
        } while (regout != regin);
        
        oldc = vram[mRow][mCol];    
    }
}

// IMPORTANT: Enforce simplest main() loop possible
int main() {
    setup();
    while(true) loop();
}
