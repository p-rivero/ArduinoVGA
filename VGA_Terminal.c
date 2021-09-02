// **********************************************************************************
// ***** ARDUINO NANO 40x25 ANSI DUMP (VT-100) TERMINAL by Carsten Herting 2020 *****
// **********************************************************************************
/* Adapted by P. Rivero
   What has been changed:
   - Translated some comments into English
   - Changed the behavior of ESC codes ESC[C and ESC[D to follow the ANSI standard
   - Minor clarity improvements and code optimizations
   - Added 6-bit color output
*/

// LICENSE: MIT License (see link in the repo)
// Firmware Version 1.11 - 22.02.2021
// set Arduino Nano FuseA from default 0xFF to 0xBF (enabling output of 16MHz CLKO on pin 8)
// Pins 0-7:  Port D, used to write data to the parallel load of 74165, used to read data from 74174 input register
// Pin  8:    Arduino CLK out (16MHz)
// Pin  9:    74173 /OE
// Pin  10:   VSYNC (timer1, every 1/60s)
// Pin  11:   74165 PE (timer2)
// Pin  12:   HSYNC (inside ISR, every 32us)
// Pin  13:   74173 MR
// Pins 14-19: Port C, VGA color

// Using Arduino Nano or Uno
#ifndef __AVR_ATmega328P__
#define __AVR_ATmega328P__ 
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/builtins.h>   // __builtin_avr_delay_cycles()

typedef uint8_t byte;

#define BUFFER_SIZE 128

#define ROWS 25
#define COLS 40

#define STARTING_ROW 2
#define STARTING_COL 0

// If this is not defined (commented out), new inserted lines will be white
// If this is defined, they will copy the color of their upper row
#define COPY_LAST_COLOR

// Color definitions
#define RED             0b110000
#define LIGHTRED        0b110101
#define LLIGHTRED       0b111010
#define DARKRED         0b100000
#define DDARKRED        0b010000

#define GREEN           0b001100
#define LIGHTGREEN      0b011101
#define LLIGHTGREEN     0b101110
#define DARKGREEN       0b001000
#define DDARKGREEN      0b000100

#define BLUE            0b000011
#define LIGHTBLUE       0b010111
#define LLIGHTBLUE      0b101011
#define DARKBLUE        0b000010
#define DDARKBLUE       0b000001

#define CYAN            0b001111
#define LIGHTCYAN       0b011111
#define LLIGHTCYAN      0b101111
#define DARKCYAN        0b001010
#define DDARKCYAN       0b000101

#define YELLOW          0b111100
#define LIGHTYELLOW     0b111101
#define LLIGHTYELLOW    0b111110
#define DARKYELLOW      0b101000
#define DDARKYELLOW     0b010100

#define MAGENTA         0b110011
#define LIGHTMAGENTA    0b110111
#define LLIGHTMAGENTA   0b111011
#define DARKMAGENTA     0b100010
#define DDARKMAGENTA    0b010001

#define ORANGE          0b110100
#define LIGHTORANGE     0b111001
#define DARKORANGE      0b100100

#define LIME            0b101100
#define LIGHTLIME       0b101101
#define DARKLIME        0b011000

#define MINT            0b001110
#define LIGHTMINT       0b011110
#define DARKMINT        0b001001

#define SKY             0b000111
#define LIGHTSKY        0b011011
#define DARKSKY         0b000110

#define PURPLE          0b100011
#define LIGHTPURPLE     0b100111
#define DARKPURPLE      0b010010

#define PINK            0b110001
#define LIGHTPINK       0b110110
#define DARKPINK        0b100001

#define BLACK           0b000000
#define GREY            0b010101
#define LIGHTGREY       0b101010
#define WHITE           0b111111

volatile int vLine;             // current horizontal video line
volatile byte reg[BUFFER_SIZE]; // Queue of data to be written into VRAM
volatile byte regout = 0;       // Index of current output position in queue
volatile byte regin = 0;        // Index of current input position in queue
volatile byte mFrame = 0;       // 1/60s frame counter for cursor blinking
byte blinkEn = 1;               // Blinking enabled
enum {FIRST_BYTE = 0, SET_COLOR_SCREEN, SET_COLOR_LINE};
byte nextByte = FIRST_BYTE;
byte mRow = STARTING_ROW;       // cursor position in terminal window
byte mCol = STARTING_COL;       // cursor position in terminal window
byte old_mRow = 0, old_mCol = 0;

byte vram[ROWS][COLS] = {
    "VGA TERMINAL OK                         ",
    "Waiting for clear command from CPU...   ",
    [2 ... 9] =
    "                                        ",
    [10 ... ROWS-1] =
    "****************************************"
};
byte oldc;   // stores char a cursor position

// Contains the current color for each row
byte cram[ROWS] = { [0 ... 9] = WHITE, RED, ORANGE, 0b111000, YELLOW, LIME, GREEN, MINT, CYAN, SKY, BLUE, 0b010011, PURPLE, MAGENTA, 0b110010, PINK };


// my improved charset line data starting with character 32 (SPACE)
#define STARTING_CHAR ' '
const byte charset[8][128-STARTING_CHAR] = {
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

inline void setup() {
    // Initialize variables
    oldc = vram[STARTING_ROW][STARTING_COL];
    
    cli();                  // disable interrupts before messing around with timer registers (same as noInterrupts();)
    DDRC  = WHITE;          // VGA color (use white color as a mask of all valid bits)
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
    
    if (a != 0) {                   // was anything !=0 read?
        if (a == b) {               // was the reading valid?
            PORTB = 0b00100010;     // MR=1, HSYNC=0, /OE=1: Pull register reset and output
            reg[c++] = a;           // Store new value in the buffer
            c &= (BUFFER_SIZE-1);   // BUFFER_SIZE-1 is the bit mask that keeps c in the range [0, BUFFER_SIZE)
            regin = c;
        } else __builtin_avr_delay_cycles(13);  // ensure same amount of cycles on all branches 4/5/9   
    } else __builtin_avr_delay_cycles(15);      // ensure same amount of cycles on all branches          
    
    PORTB = 0b00000010;                         // MR=0, HSYNC=0, /OE=1: Disable register output
    byte lin = ((vLine++) >> 1) - 38;           // skip 2 lines (VSYNC pulse) + vertical back porch
    byte row = lin >> 3;                        // calculate the character row

    DDRD  = 0b11111111;                         // switch port D to output
    PORTD = 0;                                  // prevent trash data from being fed into the shift register
    byte* vrow = vram[row];                     // pointer to the vram row 0...24 to display
    const byte* cset = charset[lin & 0b00000111] - STARTING_CHAR; // pointer to the charset line 0..7 to use
    PORTB = 0b00010010;                         // MR=0, HSYNC=1, /OE=1: End of HSYNC pulse

    PORTC = cram[row];                          // set current row color

    if (lin < ROWS*8)                           // skip 2 lines (VSYNC pulse) + 33 lines (vertical back porch)
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


inline byte is_bit_set(byte data, byte bit_num) {
    return (data & (1 << bit_num));
}
inline void set_color(byte row, byte color) {
    // The white color works as a mask of all valid bits
    cram[row] = color & WHITE;
}
inline void clear_line(byte row) {
    for (int c = 0; c < COLS; c++)
        vram[row][c] = ' ';
}

void Scroll() { 
    for (int r = 0; r < ROWS-1; r++)
        for (int c = 0; c < COLS; c++)
            vram[r][c] = vram[r+1][c];  // move all rows one step up

    clear_line(ROWS-1); // fill lowest line with SPACES

    for (int r = 0; r < ROWS-1; r++)
        cram[r] = cram[r+1];            // Also scroll the line color

    #ifndef COPY_LAST_COLOR
        cram[ROWS-1] = WHITE;           // Set lowest line color to White
    #endif
}

// processes a character (see VGA terminal docs)
void ProcessChar(byte inbyte) {
    mFrame = 25;
    
    // Second byte of a 2-byte sequence
    if (nextByte != FIRST_BYTE) {
        if (nextByte == SET_COLOR_LINE) {
            set_color(mRow, inbyte & 0x3F);
        }
        else if (nextByte == SET_COLOR_SCREEN) {
            for (int i = 0; i < ROWS; i++)
                set_color(i, inbyte & 0x3F);
        }
        nextByte = FIRST_BYTE;
        return;
    }
    
    if (is_bit_set(inbyte, 7)) {    // COMMAND
        if (is_bit_set(inbyte, 6)) {
            // Move to column
            byte new_col = inbyte & 0x3F;
            if (new_col < COLS) mCol = new_col;
        }
        else if (is_bit_set(inbyte, 5)) {
            // Move to line/row
            byte new_row = inbyte & 0x1F;
            if (new_row < ROWS) mRow = new_row;
        }
        // 0b1001XXXX (bit 4) -> Unused
        else if (is_bit_set(inbyte, 3)) {
            if (is_bit_set(inbyte, 0)) {
                // Set color for line
                nextByte = SET_COLOR_LINE;
            }
            else {
                // Set color for screen
                nextByte = SET_COLOR_SCREEN;
            }
        }
        else if (is_bit_set(inbyte, 2)) {
            if (is_bit_set(inbyte, 1)) {
                if (is_bit_set(inbyte, 0)) {
                    // Clear line
                    clear_line(mRow);
                }
                else {
                    // Clear screen
                    for (int i = 0; i < ROWS; i++) clear_line(i);
                }
            }
            else {
                if (is_bit_set(inbyte, 0)) {
                    // Restore cursor pos
                    mRow = old_mRow;
                    mCol = old_mCol;
                }
                else {
                    // Save cursor pos
                    old_mRow = mRow;
                    old_mCol = mCol;
                }
            }
        }
        else if (is_bit_set(inbyte, 1)) {
            // Turn on/off cursor blink
            blinkEn = is_bit_set(inbyte, 0);
        }
        else if (is_bit_set(inbyte, 0)) {
            // Reset (set cursor to top-left, clear screen, set color of screen to white)
            mRow = mCol = 0;
            for (int i = 0; i < ROWS; i++) {
                clear_line(i);
                set_color(i, WHITE);
            }
        }
    }
    else {  // ASCII CHAR
        switch (inbyte) {
            case '\b':  // Backspace: Remove 1 character (move cursor left)
                if (mCol > 0) vram[mRow][--mCol] = ' '; // Backspace doesn't change line
                break;
            
            case 0x7F:  // Delete: Remove 1 character (move cursor right)
                vram[mRow][mCol] = ' ';
                if (mCol < COLS-1) mCol++; // Del doesn't change line
                break;
            
            case '\t':  // Tab: move to next multiple of 4
                mCol &= 0xFC;
                if (mCol < COLS-5) mCol += 4;
                else mCol = COLS-1;
                break;
            
            case '\n':  // Move to a new line
                mCol = 0;
                // Also do '\v'
            
            case '\v':  // Vertical tab: LF without CR
                if (mRow < ROWS-1) {
                    #ifdef COPY_LAST_COLOR
                        cram[mRow+1] = cram[mRow];  // Propagate color to new line
                    #endif
                    mRow++;
                }
                else Scroll();
                break;
                
            case '\f':  // Form feed: insert a page break
                mRow = ROWS-1;
                mCol = 0;
                for (int i = 0; i < ROWS; i++) {
                    clear_line(i);
                    set_color(i, WHITE);
                }
                break;
            
            case '\r':  // Carriage return: move to the beginning of line
                mCol = 0;
                break;
                
            case 0x1C:  // Move cursor left
                if (mCol > 0) mCol--;
                break;
            
            case 0x1D:  // Move cursor right
                if (mCol < COLS-1) mCol++;
                break;
            
            case 0x1E:  // Move cursor down
                if (mRow < ROWS-1) mRow++;
                break;
            
            case 0x1F:  // Move cursor up
                if (mRow > 0) mRow--;
                break;
            
            default: // No special character detected.
                if (inbyte < STARTING_CHAR) return; // Check if it's printable
                vram[mRow][mCol++] = inbyte;    // Just write the character to vram
                if (mCol >= COLS) {
                    mCol = 0;
                    if (mRow < ROWS-1) {
                        #ifdef COPY_LAST_COLOR
                            cram[mRow+1] = cram[mRow];  // Propagate color to new line
                        #endif
                        mRow++;
                    }
                    else Scroll();
                }
                break;
        }
    }
}


inline void loop() { 
    if (TCNT2 & 1) __builtin_avr_delay_cycles(3);  // Canceling out interrupt jitter using the fast timer

    if (blinkEn) {
        if ((mFrame & 63) > 31) vram[mRow][mCol] = 127;
        else vram[mRow][mCol] = oldc;   // cursor blinking
    }

    if (regout != regin) {              // was a character received
        vram[mRow][mCol] = oldc;        // restore character beneath the cursor BEFORE a possible scrolling happens
        do {
            ProcessChar(reg[regout]);   // process this character
            regout++;
            regout &= (BUFFER_SIZE-1);  // BUFFER_SIZE-1 is the bit mask that keeps c in the range [0, BUFFER_SIZE)
        } while (regout != regin);
        
        oldc = vram[mRow][mCol];    
    }
}

// IMPORTANT: Enforce simplest main() loop possible
int main() {
    setup();
    while(1) loop();
}
