#include <EEPROM.h>
#include "cowpi.h"

#define  DEBOUNCE_KEYPAD 200u
# define DEBOUNCE_BUTTON 20u
#define SINGLE_CLICK_TIME 150u
#define DOUBLE_CLICK_TIME 500u

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

enum mode {LOCKED, UNLOCKED, UNLOCKING, ALARMED, CHANGING, CHANGE_, CONFIRMING, CONFIRM_, BAD_TRY, LOCKING};
/* Memory-mapped I/O */
cowpi_ioPortRegisters *ioPorts;     // an array of I/O ports
cowpi_spiRegisters *spi;            // a pointer to the single set of SPI registers

//Global Variables
uint8_t attempt = 1;
uint8_t cursorLocation = 1;
uint8_t locked;
uint8_t keyPressed = 0; // holds the last key pressed on the 4x4 keypad
unsigned long lastKeypadPress = 0;
uint8_t segments[8] = {0, 0, 1, 0, 0, 1, 0, 0}; // holds the values currently displayed on the keypad
enum mode systemMode;
uint8_t index = 0;
uint16_t combination[3];
uint16_t entry[3];
uint16_t password[3];

//variables for button actions
volatile unsigned long LastLeftAction = 0; // LastLeftAction
volatile unsigned long LastRightAction = 0; // LastRightAction
volatile unsigned long OldLeftPosition = 1; // OldLeftPosition
volatile unsigned long OldRightPosition = 1; // OldRightPosition
volatile unsigned long LastLeftPress = 0; // LastLeftPress
volatile unsigned long LastRightPress = 0; // LastRightPress
volatile unsigned long LastLeftClick = 0; // LastLeftClick
volatile bool DoubleClick = 0;

void setup() {
  Serial.begin(9600);
    cowpi_setup(SPI | MAX7219);
  ioPorts = (cowpi_ioPortRegisters *) 0x23;
  spi = (cowpi_spiRegisters *)(cowpi_IObase + 0x2C);
  attachInterrupt(digitalPinToInterrupt(2), handleButtonAction , CHANGE );
  attachInterrupt(digitalPinToInterrupt(3), handleKeypress , RISING );
  setupTimer();
  updateDisplay();
  //Set combination
  EEPROM.put(0, 136);
  EEPROM.put(1, 136);
  EEPROM.put(2, 136);
  //Set system to locked on start
  systemMode = LOCKED;
  ;
}

void loop() {

  if(systemMode == UNLOCKED){}
  if(systemMode == ALARMED){
    detachInterrupt(digitalPinToInterrupt(2));
    detachInterrupt(digitalPinToInterrupt(3));  
    clearDisplay();
    alert();
  }
  if(systemMode == CHANGING){}
  if(systemMode == CONFIRMING){}
}

volatile long int count = 0;
volatile bool FLAG = 0;
ISR(TIMER1_COMPA_vect){
  // any vars declared should be as volatile
  // if need to reset timer, write 0 to timer1's counter field
//   any vars declared should be as volatile
  // if need to reset timer, write 0 to timer1's counter field
//  if blinkCursor();
  if(systemMode == LOCKED || systemMode == CHANGING){
    if(count == 2){
      count = 0;
      FLAG = !FLAG;
    }
    blinkCursor();
    
  }
  if(systemMode == ALARMED){
    if(FLAG){
      digitalWrite(12, HIGH);
      FLAG = !FLAG;
    }
    else{
      digitalWrite(12, LOW);
      FLAG = !FLAG;
    }
  }
  if(systemMode == BAD_TRY){
    if(count == 4){
      clearDisplay();
      updateDisplay();
      print_segments();
      systemMode = LOCKED;
    }
  }
  if(systemMode == LOCKING){
    if(count == 4){
      clearDisplay();
      clearDigits();
      clearCombination();
      updateDisplay();
      count = 0;
      cursorLocation = 1;
      systemMode = LOCKED;
    }
  }
  if(systemMode == CHANGE_) {
    if(count == 4) {
      clearDisplay();
      clearDigits();
      updateDisplay();
      systemMode = CHANGING;
    }
  }
  if(systemMode == CONFIRM_) {
    if(count == 4) {
      clearDisplay();
      clearDigits();
      updateDisplay();
      systemMode = CONFIRMING;
    }
  }
  if(systemMode == UNLOCKING) {
    if(count == 4) {
      clearDisplay();
      clearDigits();
      updateDisplay();
      systemMode = UNLOCKED;
    }
  }
  
  count++;

}

void setupTimer() {
/*
 * comparison value = 15625 = 16,000,000 / (4*256)
 * using 256 prescaler
 */
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A = 32249;
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS12 bits for 256 prescaler
  TCCR1B |= (1 << CS12);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
}

void handleKeypress(){
  char key;
  unsigned long now = millis();
  uint8_t newKeyPressed;
  if((now - lastKeypadPress > DEBOUNCE_KEYPAD)){ 
    lastKeypadPress = now;
    key = cowpi_getKeypress();
    keyPressed = charToHex(key);
//    newKeyPressed = charToHex(key);
//    Serial.println(keyPressed);
    if (systemMode == LOCKED || CHANGING){
      combinationEntry();
    }
  }
//  if (keyPressed == 0 && newKeyPressed != 0){
//    keyPressed = newKeyPressed;
//  }
//  if(newKeyPressed && !keyPressed){
//    lastKeypadPress = now;
//    Serial.println("keypad pressed");
//    keyPressed = newKeyPressed;
//  }
//  if(!newKeyPressed && keyPressed){
//    lastKeypadPress = now;
//    Serial.println("keypad released");
//    return;
//  }
}

void handleButtonAction() {
  // right button is connected to D9
  // left button is connected to D8
  /*
   * if 0, builder is in decimal
   * if 1, builder is in hex
   */
  unsigned long now = millis();
  uint8_t NewLeftPosition;
  uint8_t NewRightPosition;
  if ((now - LastLeftAction) > DEBOUNCE_BUTTON ){
    NewLeftPosition = digitalRead(8);
//    LastLeftAction = now;
  }else{
    NewLeftPosition = OldLeftPosition;
  }
  if ((now - LastRightAction) > DEBOUNCE_BUTTON){
    NewRightPosition = digitalRead(9);
//    LastRightAction = now;
  }else{
    NewRightPosition = OldRightPosition;
  }

  if (OldLeftPosition == NewLeftPosition && OldRightPosition == NewRightPosition){
    return;
  }else {
    // case 1: Right button pressed
    if (OldRightPosition && !NewRightPosition){
      LastRightPress = now;
      if(systemMode !=  UNLOCKED && systemMode != BAD_TRY){
        moveCursor();
      }
      Serial.print("Right button pressed\n");
    }
    // Case 2: Right Button Released
    if (!OldRightPosition && NewRightPosition){
       Serial.print("Right button released\n");
      if ((LastRightPress + SINGLE_CLICK_TIME) < now){
          OldRightPosition = 0;
      }
    }
    // Case 3: Left Button Pressed
    if (OldLeftPosition && !NewLeftPosition){
      LastLeftPress = now;
      Serial.print("Left button pressed\n");
      if(systemMode == LOCKED){
        checkCombination();
      } else if (systemMode == UNLOCKED) {
        if(digitalRead(A4) && digitalRead(A5)) {
          systemMode = CHANGE_;
          clearDisplay();
          enter();
        }
      } else if (systemMode == CHANGING) {
        if(!digitalRead(A4)) {
          reenter();
          systemMode = CONFIRM_;
          entry[0] = EEPROM.read(0);
          entry[1] = EEPROM.read(1);
          entry[2] = EEPROM.read(2);
        }
      } else if (systemMode == CONFIRMING) {
        if(!digitalRead(A5)) {
          checkCombination();
        }
      }
      if ((LastLeftClick + DOUBLE_CLICK_TIME) > now){
        DoubleClick = true;
        if(!digitalRead(A4) && !digitalRead(A5)){
          clearDisplay();
          systemMode = LOCKING;
          count = 0;
          closed();
        }
      }
    }
    // Case 4: Left Button Released
    if (!OldLeftPosition && NewLeftPosition){
      if (DoubleClick ){
        Serial.print("Left button double clicked and released\n");
        DoubleClick = !DoubleClick; //Turn off DoubleClick flag
      }else if (LastLeftPress + SINGLE_CLICK_TIME > now){
        LastLeftClick = 0;
        Serial.print("Left button release\n");
      }else{
        LastLeftClick = now;
        Serial.print("Left button release\n");
      }
    }
    OldLeftPosition = NewLeftPosition;
    OldRightPosition = NewRightPosition;
  }
}


void combinationEntry(){
//       Serial.println("index on entry");
//     Serial.print(index);
  switch (cursorLocation){
    /*
     * case 1: I am setting this up to work so that when the user enters a third number into 
     * a single combination slot it clears the previous value and puts the third number into
     * the first digit of the combination slot.
     */
    case 1:
      if(index == 0){
        segments[1] = sevenSegments[keyPressed];
        combination[0] = keyPressed;
        index ++;
      }else
      if (index == 1){
        segments[0] = segments[1];
        segments[1] = sevenSegments[keyPressed];
        combination[0] = ((combination[0] * 16) + keyPressed);
        index ++;
      }else
      if (index == 2){
        segments[0] = 0;
        segments[1] = sevenSegments[keyPressed];
        index = 1;
        combination[0] = keyPressed;        
      }
      break;
    case 2:
      if(index == 3){
        segments[4] = sevenSegments[keyPressed];
        combination[1] = keyPressed;
        index ++;
      }else
      if (index == 4){
        segments[3] = segments[4];
        segments[4] = sevenSegments[keyPressed];
        combination[1] = ((combination[1] * 16) + keyPressed);
        index ++;
      }else
      if (index == 5){
        segments[3] = 0;
        segments[4] = sevenSegments[keyPressed];
        index = 4;
        combination[1] = keyPressed;        
      }else
      break;
    case 3: 
      if(index == 6){
        segments[7] = sevenSegments[keyPressed];
        combination[2] = keyPressed;
        index ++;
      }else
      if (index == 7){
        segments[6] = segments[7];
        segments[7] = sevenSegments[keyPressed];
        combination[2] = ((combination[2] * 16) + keyPressed);
        index ++;
      }else
      if (index == 8){
        segments[6] = 0;
        segments[7] = sevenSegments[keyPressed];
        index = 7;
        combination[2] = keyPressed;        
      }
      break; 
      updateDisplay;
  }
  updateDisplay();
//     Serial.println("index on exit");
//     Serial.print(index);
//  Serial.println(combination[0]);
//  Serial.println(combination[1]);
//  Serial.println(combination[2]);

}

void checkCombination(){

  if(systemMode == LOCKED) {

    int first = EEPROM.read(0);
    int second = EEPROM.read(1);
    int third = EEPROM.read(2);

    if(segments[0] == 0 || segments[3] == 0 || segments[6] == 0){
      count = 0;
      error();
    }
    if(attempt == 4){
      systemMode == ALARMED;
    }
    if(first == combination[0] && second  == combination[1] && third == combination[2]){
      systemMode = UNLOCKED;
      clearDisplay();
      labOpen();
    }else{
      Serial.println(combination[0]);
      systemMode == BAD_TRY;
      clearDisplay();
      badTry();
      count = 0;
      attempt++;;
    }
  } else if(systemMode == CHANGING) {
      password[0] = EEPROM.read(0);
      password[1] = EEPROM.read(1);
      password[2] = EEPROM.read(2);
      EEPROM.put(0, combination[0]);
      EEPROM.put(1, combination[2]);
      EEPROM.put(2, combination[3]);
      clearCombination();
  } else if(systemMode == CONFIRMING) {
      int first = EEPROM.read(0);
      int second = EEPROM.read(1);
      int third = EEPROM.read(2);
      //Keep EEPROM the same if combos match
      if(first == entry[0] && second == entry[1] && third == entry[3]) {
        clearDisplay();
        changed();
        systemMode = UNLOCKING;
      } else {
        EEPROM.put(0, password[0]);
        EEPROM.put(1, password[1]);
        EEPROM.put(2, password[2]);
        clearDisplay();
        nochange();
        systemMode = UNLOCKING;
      }
  }
  
}

void displayData(uint8_t address, uint8_t value) {
  // address is MAX7219's register address (1-8 for digits; otherwise see MAX7219 datasheet Table 2)
  // value is the bit pattern to place in the register
  cowpi_spiEnable;
  ioPorts[D8_D13].output &= 0b111011;
  spi->data = address;
  while(!(spi->status &= 0b10000000)) {
    //Do nothing
  }
  spi->data = value;
  while(!(spi->status &= 0b10000000)) {
    //Do nothing
  }
  ioPorts[D8_D13].output |= 0b000100;
  cowpi_spiDisable;
}

uint8_t getKeyPressed() {
  uint8_t keyPressed = 0xFF;
  unsigned long now = millis();
  if (now - lastKeypadPress > DEBOUNCE_KEYPAD) {
    lastKeypadPress = now;
    for(int i = 0; i < 4; i++) {
      ioPorts[D0_D7].output |= 0b11110000;
      if(i == 0) {         
        ioPorts[D0_D7].output &= 0b11100000;
      } else if (i == 1) {
        ioPorts[D0_D7].output &= 0b11010000;
      } else if (i == 2) {
        ioPorts[D0_D7].output &= 0b10110000;
      } else if (i == 3) {
        ioPorts[D0_D7].output &= 0b01110000;
      }
      if(!(ioPorts[A0_A5].input & 0b000001)) {
        keyPressed = keys[i][0];
      } else if (!(ioPorts[A0_A5].input & 0b000010)) {
        keyPressed = keys[i][1];
      } else if (!(ioPorts[A0_A5].input & 0b000100)) {
        keyPressed = keys[i][2];
      } else if (!(ioPorts[A0_A5].input & 0b001000)) {
        keyPressed = keys[i][3];
      }
    }
    ioPorts[D0_D7].output &= 0b00000000;
  }
  return keyPressed;
}


void clearDisplay(){
  displayData(8, 0);
  displayData(7, 0);
  displayData(6, 0);
  displayData(5, 0);
  displayData(4, 0);
  displayData(3, 0);
  displayData(2, 0);
  displayData(1, 0);
}

void updateDisplay(){
  displayData(8, segments[0]);
  displayData(7, segments[1]);
  displayData(6, segments[2]);
  displayData(5, segments[3]);
  displayData(4, segments[4]);
  displayData(3, segments[5]);
  displayData(2, segments[6]);
  displayData(1, segments[7]);
}

void clearDigits(){
  segments[0] = 0;
  segments[1] = 0;
  segments[2] = 1;
  segments[3] = 0;
  segments[4] = 0;
  segments[5] = 1;
  segments[6] = 0;
  segments[7] = 0;
}

void clearCombination(){
  combination[0] = 0;
  combination[1] = 0;
  combination[2] = 0;
  
}

void error(){
  displayData(8, sevenSegments[14]);
  displayData(7, 0b00000101);
  displayData(6, 0b00000101);
  displayData(5, 0b00011101);
  displayData(4, 0b00000101);
  displayData(3, 0);
  displayData(2, 0);
  displayData(1, 0);
}

void badTry(){ //attempt is a number between 0-3, as described in the handout
  displayData(8, 0b00011111); // b
  displayData(7, 0b01110111); // A
  displayData(6, 0b00111101); // d
  displayData(5, 0);         // _
  displayData(4, 0b00001111); // t
  displayData(3, 0b00000101); // r
  displayData(2, 0b00111011); // y
  displayData(1, sevenSegments[attempt]);
}

void labOpen(){
  displayData(8, 0b00001110); // L
  displayData(7, 0b01110111); // A
  displayData(6, 0b00011111); // b
  displayData(5, 0);         // _
  displayData(4, 0b00011101); // o
  displayData(3, 0b01100111); // P
  displayData(2, 0b01001111); // E
  displayData(1, 0b01110110); // n
}

void alert(){
  displayData(8, 0b01110111); // A
  displayData(7, 0b00001110); // L
  displayData(6, 0b01001111); // E
  displayData(5, 0b00000101); // r
  displayData(4, 0b00001111); // t
  displayData(3, 0b10100000); //  !
}

void closed(){
  displayData(8, 0b00001101); // c
  displayData(7, 0b00001110); // L
  displayData(6, 0b00011101); // o
  displayData(5, 0b01011011); // S
  displayData(4, 0b01001111); // E
  displayData(3, 0b00111101); // d
}

void enter() {
  displayData(8, 0b01001111); // E
  displayData(7, 0b01110110); // n
  displayData(6, 0b00001111); // t
  displayData(5, 0b01001111); // E
  displayData(4, 0b00000101); // r
}

void reenter() {
  displayData(8, 0b00000101); // r
  displayData(7, 0b01001111); // E
  displayData(6, 0b00000001); // -
  displayData(5, 0b01001111); // E
  displayData(4, 0b01110110); // n
  displayData(3, 0b00001111); // t
  displayData(2, 0b01001111); // E
  displayData(1, 0b00000101); // r
}

void changed() {
  displayData(8, 0b00001101); // c
  displayData(7, 0b00110110); // H
  displayData(6, 0b01110111); // A
  displayData(5, 0b01110110); // n
  displayData(4, 0b01011110); // G
  displayData(3, 0b01001111); // E
  displayData(2, 0b00111101); // d
}

void nochange() {
  displayData(8, 0b01110110); // n
  displayData(7, 0b00011101); // o
  displayData(6, 0b00001101); // c
  displayData(5, 0b00110110); // H
  displayData(4, 0b01110111); // A
  displayData(3, 0b01110110); // n
  displayData(2, 0b01011110); // G
  displayData(1, 0b01001111); // E
}

void blinkCursor(){
  switch(cursorLocation){
    // we bitwise or the cursor with the current value displayed so that when numbers are shown,
    // the cursor doesn't erase the number just to blink
    case 1:
      if(count < 2 && FLAG){
        displayData(8, 0b10000000 | segments[0]);
        displayData(7, 0b10000000 | segments[1]);
      }else if (count < 2 && !FLAG){
        displayData(8, segments[0]);
        displayData(7, segments[1]);
      }
      break;
    case 2:
      if(count < 2 && FLAG){
        displayData(5, 0b10000000 | segments[3]);
        displayData(4, 0b10000000 | segments[4]);
      }else if ((count < 2 && !FLAG)){
        displayData(5, segments[3]);
        displayData(4, segments[4]);
      }
      break;
    case 3:
      if(count < 2 && FLAG){
        displayData(2, 0b10000000 | segments[6]);
        displayData(1, 0b10000000 | segments[7]);
      }else if ((count < 2 && !FLAG)){
        displayData(2, segments[6]);
        displayData(1, segments[7]);
      }
      break;
  }
}

void moveCursor(){
  count = 0;
  switch (cursorLocation){
    case 1:
      displayData(8, segments[0] & 0b01111111);
      displayData(7, segments[1] & 0b01111111);
      index = 3;
      cursorLocation = 2;
      break;
    case 2:
      displayData(5, segments[3] & 0b01111111);
      displayData(4, segments[4] & 0b01111111);
      cursorLocation = 3;
      index = 6;
      break;
    case 3: 
      displayData(2, segments[6] & 0b01111111);
      displayData(1, segments[7] & 0b01111111);
      cursorLocation = 1;
      index = 0;
      break; 
  }
}

void print_segments(){
  char buff[50];
  snprintf(buff, 50,  "[%d, %d, %d, %d, %d, %d, %d, %d]", 
  segments[0],
  segments[1],
  segments[2],
  segments[3],
  segments[4],
  segments[5],
  segments[6],
  segments[7]);
  Serial.println(buff);
  
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
