#include <EEPROM.h>
#include "cowpi.h"


// Layout of Matrix Keypad
//        1 2 3 A
//        4 5 6 B
//        7 8 9 C
//        * 0 # D
// This array holds the values we want each keypad button to correspond to
const uint8_t keys[4][4] = {
  {1, 2, 3, 0xA},
  {4, 5, 6, 0xB},
  {7, 8, 9, 0xC},
  {0xE, 0, 0xF, 0xD}
};

// Seven Segment Display mapping between segments and bits
// Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
//  DP   A    B    C    D    E    F    G
// This array holds the bit patterns to display each hexadecimal numeral
const uint8_t sevenSegments[16] = {
0b01111110, // 0
0b00110000, // 1
0b01101101, // 2
0b01111001, // 3
0b00110011, // 4
0b01011011, // 5
0b01011111, //6
0b01110000, //7
0b01111111, //8
0b01110011, //9
0b01110111, //A
0b00011111, //b
0b00001101, //c
0b00111101, //d
0b01001111, //E
0b01000111, //F
};

/* Memory-mapped I/O */
cowpi_ioPortRegisters *ioPorts;     // an array of I/O ports
cowpi_spiRegisters *spi;            // a pointer to the single set of SPI registers

void setup() {
  Serial.begin(9600);
    cowpi_setup(SPI | MAX7219);
  ioPorts = (cowpi_ioPortRegisters *) 0x23;
  spi = (cowpi_spiRegisters *)(cowpi_IObase + 0x2C);
  setupTimer();
}

void loop() {
  ;
}

volatile int count = 0;
ISR(TIMER1_COMPA_vect){
  // any vars declared should be as volatile
  // if need to reset timer, write 0 to timer1's counter field
  Serial.print(count++);
}

void setupTimer() {
/*
 * comparison value = 3225 = 16,000,000 / (20*256)
 * using 256 prescaler
 */
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A = 3124;
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS12 bits for 256 prescaler
  TCCR1B |= (1 << CS12);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
}
