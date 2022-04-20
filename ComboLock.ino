#include <EEPROM.h>
#include "cowpi.h"

#define DEBOUNCE 20u

// function stubs
uint8_t charToHex(char keyPressed);
void clearDisplay();
void updateDisplay();
void displayData(uint8_t address, uint8_t value);
void setupTimer();
void handleButtonAction();
void handleKeypress();


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

//Global Variables
uint8_t keyPressed = 0; // holds the last key pressed on the 4x4 keypad
uint8_t segments[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // holds the values currently displayed on the keypad

void setup() {
  Serial.begin(9600);
    cowpi_setup(SPI | MAX7219);
  ioPorts = (cowpi_ioPortRegisters *) 0x23;
  spi = (cowpi_spiRegisters *)(cowpi_IObase + 0x2C);
  attachInterrupt(digitalPinToInterrupt(2), handleButtonAction , CHANGE );
  attachInterrupt(digitalPinToInterrupt(3), handleKeypress , CHANGE );
  setupTimer();
  clearDisplay();
}

void loop() {
  ;
}

volatile int FLAG = 0;
ISR(TIMER1_COMPA_vect){
  // any vars declared should be as volatile
  // if need to reset timer, write 0 to timer1's counter field
  
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

void handleKeypress(){
  char key
  unsigned long now = millis();
  if(now - lastKeyPress > DEBOUNCE){
    lastKeypadPress = now;
    key = cowpi_getKeypress() ;
    keyPressed = charToHex(key);
  }
}

void displayData(uint8_t address, uint8_t value) {
  // address is MAX7219's register address (1-8 for digits; otherwise see MAX7219 datasheet Table 2)
  // value is the bit pattern to place in the register
  cowpi_sendDataToMax7219(address, value);
}


void clearDisplay(){
  for(int i = 0; i < 8; i++){
    displayData(i+1,0);
  }
  displayData(1, sevenSegments[0]);
}

uint8_t charToHex(char keyPressed){
  uint8_t hexValue;
  switch (keyPressed){
    case '0':
      hexValue = 0;
      break;
    case '1':
      hexValue = 1;
      break;
    case '2':
      hexValue = 2;
      break;
    case '3':
      hexValue = 3;
      break;
    case '4':
      hexValue = 4;
      break;
    case '5':
      hexValue = 5;
      break;
    case '6':
      hexValue = 6;
      break;
    case '7':
      hexValue = 7;
      break;
    case '8':
      hexValue = 8;
      break;
    case '9':
      hexValue = 9;
      break;
    case 'A':
      hexValue = 10;
      break;
    case 'B':
      hexValue = 11;
      break;
    case 'C':
      hexValue = 12;
      break;
    case 'D':
      hexValue = 13;
      break;
    case '*':
      hexValue = 14;
      break;
    case '#':
      hexValue = 15;
      break;   
  }
  return hexValue;
}
