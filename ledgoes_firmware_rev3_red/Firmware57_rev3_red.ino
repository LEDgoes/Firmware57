/**********************************************************************
 * Modular Scrolling LED Matrix - LED Board Firmware
 * (C) 2011-2014 Stephen Wylie
 *
 * REVISION 3.0
 * Standardized which CPU output pins lead to which cathodes/anodes.
 *
 **********************************************************************/

int procID;
int rowActive[7] = {6, 11, 9, 10, 3, 4, 13};
int colActive[5] = {5, 7, 8, 2, 12};
int portBmask = 0b10001;
int portDmask = 0b10100100;

// if colData[col] & rowNums[row] != 0, then activate pin # rowActive[row]
// colActive stores which pin # to activate for a particular column
// Port masks: set columns HIGH & rows LOW for when we're preparing to show another column

int incomingByte;      // a variable to read incoming serial data into
int colData[5];        // column contents
// rowNums represents the bit for each row
int rowNums[7] = {1, 2, 4, 8, 16, 32, 64};
int colWriter = 0;     // column to rewrite
int activeCol = 0;     // active column
int temp;              // multipurpose variable for actually containing the outputs
boolean activeProc;    // is this the active processor or not 
const int numReadings = 100;

int readings[numReadings];  // the readings from the analog input
int runs = 0;               // the index of the current reading
long total = 0;             // the running total
int analogIn = A7;          // Set the Analog input read pin (not on DIP version)
int val = 0;                // raw value from the auto-addressing pin
boolean cmdMode = false;    // variable to find out if we are to enter command mode

void clearReads() {
  // initialize all the readings to 0: 
  for (int thisReading = 0; thisReading < numReadings; thisReading++)
    readings[thisReading] = 0; 
  return;
}

void setup() {
  
  // show power
  digitalWrite(colActive[0], HIGH);
  digitalWrite(rowActive[0], HIGH);
  digitalWrite(colActive[0], LOW);
  // initialize serial communication:
  Serial.begin(9600);
  // initialize the LED pins as outputs:
  for (int i = 1; i < 14; i++)
    pinMode(i, OUTPUT);
  // Output a F-like shape to show all rows & columns are working
  colData[0] = 127;      // first column of test pattern
  colData[1] = 64;       // second column of test pattern
  colData[2] = 64;       // third column of test pattern
  colData[3] = 64;       // fourth column of test pattern
  colData[4] = 64;       // fifth column of test pattern
  activeProc = false;    // this processor isn't active now
  delay(1000);           // stall for 1 second
  // initialize all the readings to 0: 
  clearReads();

}

void loop() {
  
  if (runs > numReadings) {
    // see if there's incoming serial data:
    if (Serial.available() > 0) {
      // read the oldest byte in the serial buffer:
      incomingByte = Serial.read();
      // if it's the reserved bit, start changing the letter:
      if (incomingByte == procID) {    // if the reserved bit = this proc's ID,
        colWriter = 0;                 // reset column writer to the first column (2 in this case)
        activeProc = true;             // mark this as the active processor
      } else if ((incomingByte & 0x80) == 0x80) {  // otherwise if bit 8 is set (indicating a proc ID that's not us),
        activeProc = false;            // don't reset the column values on this processor 
      } else {                         // if the incoming byte is < 0x80, it's a letter
        if (colWriter < 5 && activeProc) {         // if this proc is active & the column being written is < 5,
          colData[colWriter] = incomingByte;       // change the column of this proc to be the incoming byte
          colWriter++;      
        }
      }
    }
    
    /*****************************************
    * Pin | Pin  |
    * No. | Name | Destination (Green Pin/Red Pin)
    * 13  | PB5  | Row 1 (17/18)
    * 12  | PB4  | Col 5 (15/16)
    * 11  | PB3  | Row 6 (13/14)
    * 10  | PB2  | Row 4 (11/12)
    *  9  | PB1  | Row 5 (9/10)
    *  8  | PB0  | Col 3 (7/8)
    *  7  | PD7  | Col 2 (5/6)
    *  6  | PD6  | Row 7 (3/4)
    *  5  | PD5  | Col 1 (1/2)
    *  4  | PD4  | Row 2 (27/28)
    *  3  | PD3  | Row 3 (23/24)
    *  2  | PD2  | Col 4 (19/20)
    *  1  | PD1  | Serial TX
    *  0  | PD0  | Serial RX
    //******************************************/
  
    PORTB = portBmask;      // rewrite all the column pins HIGH so they're turned off
    PORTD = portDmask;      // rewrite all the row pins LOW so they're turned off

    // do all the changes to the row pins
    for (int i = 0; i < 7; i++) {
      if ((colData[activeCol] & rowNums[i]) == rowNums[i]) {
        digitalWrite(rowActive[i], HIGH);
      }
    }
    // set up the column to be active (LOW) again
    digitalWrite(colActive[activeCol], LOW);
    // take some time to let the LEDs come up
    delay(1);
    // increment activeCol but keep within the bounds of 0-4
    activeCol++;
    activeCol %= 5;
  } else if (runs < numReadings) { 
    // subtract the last reading:
    total= total - readings[runs];         
    // read from the sensor:  
    readings[runs] = analogRead(analogIn); 
    // add the reading to the total:
    total= total + readings[runs];       
    // advance to the next position in the array:                      
    // calculate the average:
    val = total / numReadings;
    runs++;
    delay(1);
  } else if (runs == numReadings) {
    procID = floor(val / 16); // find the "not-quite" board index
    procID = 63 - procID;  // flip the number around so lowest voltage drop comes first
    procID += 128;         // Proc IDs should always begin with 0x80
    // Output the chip ID onto the test pattern in this manner:
    // X  X  X  X  X
    // X  o  o  o  o
    // X  o  o  o  o
    // X  o  o  o  o
    // X  o  o  o  o
    // X  o  o 32 16
    // X  8  4  2  1
    if ((procID & 0b1) == 0b1)
      colData[4] |= 1;
    if ((procID & 0b10) == 0b10)
      colData[3] |= 1;
    if ((procID & 0b100) == 0b100)
      colData[2] |= 1;
    if ((procID & 0b1000) == 0b1000)
      colData[1] |= 1;
    if ((procID & 0b10000) == 0b10000)
      colData[4] |= 2;
    if ((procID & 0b100000) == 0b100000)
      colData[3] |= 2;
    if ((procID & 0b1000000) == 0b1000000)
      colData[2] |= 2;
    if ((procID & 0b10000000) == 0b10000000)
      colData[1] |= 2;
    runs++;
    // Output the raw A7 pin reading onto the test pattern in this manner:
    // X    X   X   X   X
    // X    o  1k 512 256 
    // X  128  64  32  16
    // X    8   4   2   1
    // X    o   o   o   o
    // X    o   o   o   o
    // X    o   o   o   o
    int pos = 1;
    int writeCol = 4;
    int writeRow = 8;
    while (pos <= 1024) {
      if ((val & pos) == pos)
        colData[writeCol] |= writeRow;
      pos <<= 1;
      writeCol--;
      if (writeCol == 0) {
        writeCol = 4;
        writeRow <<= 1;
      }
    }
  }
}