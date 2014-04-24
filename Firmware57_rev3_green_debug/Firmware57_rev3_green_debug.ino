/**********************************************************************
 * Modular Scrolling LED Matrix - LED Board Firmware
 * (C) 2011-2014 Stephen Wylie & Stacy Wylie
 *
 * REVISION 3.2
 * Standardized which CPU output pins lead to which cathodes/anodes.
 * Added the auto-addressing scheme.
 * Implemented control codes for finer firmware control.
 *
 **********************************************************************/

#include <EEPROM.h>

#define ADDR_BOARD   0
#define ADDR_BAUD    3

int procID = 0;  //Processor ID
int rowActive[7] = {6, 11, 9, 10, 3, 4, 13};
int colActive[5] = {5, 7, 8, 2, 12};
unsigned long supportedBauds[16] = {9600, 14400, 19200, 28800, 38400, 57600, 76800, 
115200, 200000, 250000, 500000, 1000000, 9600, 9600, 9600, 9600};
int portBmask = 0b10001;
int portDmask = 0b10100100;
int currentBaud = 0;
boolean inLoop;

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

int fetchOneSerialByte() {
  boolean waitingForByte = true;   // indicate we will wait for serial data
  while (waitingForByte) {         // until we have made a successful read,
    if (Serial.available() > 0) {  // wait for data to come across the serial line
      return Serial.read();        // return the information just read
      waitingForByte = false;      // do not wait for any more information
    } // end if serial available
  } // end while waiting
}

void runCommand(int cmd) {

  // This routine handles configuration commands that might be sent from the computer
  // in order to effect some sort of change without requiring reprogramming the firmware.
  // One common example would be to reprogram the serial communication rate.
  if (cmd < 0x90) {
    // Various commands dealing with procIDs
    int boardToAffect = fetchOneSerialByte();   // find out which board needs to change
    if (cmd == 0x80) {
      // This will cause the specified panels to show their test patterns
      if (boardToAffect < 0x40 || boardToAffect >= 0x80) {
        // a value < 0x40 means the user wants to see the test patterns on
        // both chips on the specified board (0-63); otherwise just show the 
        // test pattern for that one particular chip
        int tempProcID = (boardToAffect < 0x40) ? (procID & 0x3F) : procID;
        if (tempProcID == boardToAffect) {
          makeTheF();
          showAddrDebug();
        }
      } else {
        // every chip should show their test pattern
        makeTheF();
        showAddrDebug();
      }
    } else if (cmd == 0x81) {
      // This will recalculate the procIDs automatically for all boards
      clearReads();    // initialize all the readings to 0
      // wait a random amount of time before we take the reading
      randomSeed(analogRead(5));
      delay(random(1000));
      // Now next time loop() runs, it'll find the procID based on the electrical value of ADC7 -- actually get
    } else if (cmd == 0x82 && (procID == boardToAffect)) {
      // Increment the procID of the desired board as long as it's not 0xFF already
      if (procID != 0xFF)
        procID++;
      makeTheF();
      showAddrDebug();
    } else if (cmd == 0x83 && (procID == boardToAffect)) {
      // Decrement the procID of the desired board as long as it's not 0x80 already
      if (procID != 0x80)
        procID--;
      makeTheF();
      showAddrDebug();
    } else if (cmd == 0x84 && (procID == boardToAffect)) {
      // Set the procID of the desired board to a particular value, which will be input next
      int newProcID = fetchOneSerialByte();          // value of the processor's new ID
      if (newProcID > 0x7F && newProcID <= 0xFF)     // If the new proc ID is within range,
        procID = newProcID;                          // Commit it as the processor ID
      makeTheF();
      showAddrDebug();
    } else if (cmd == 0x85 && (procID == boardToAffect)) {
      // Save our address (eeprom)
      // This will have each board save their address to the EEPROM
      makeTheF(); // Show the user that you are activating this board
      eepromClearAddr(ADDR_BOARD);
      eepromSave(ADDR_BOARD, procID); //save It
      procID = eepromRead(ADDR_BOARD); //Read the new address to be used
      showAddrDebug(); //Show the Address
    } else if (cmd == 0x86 && (procID == boardToAffect)) {
      //Clear your address
      eepromClearAddr(ADDR_BOARD);
      procID = 0; 
    } else if (cmd == 0x8F) {
      // Clear ALL OF your EEPROM
      eepromClear();
      makeTheF(); // Show that the operation completed
    }
    // end if cmd is in the 0x80s
  } else if (cmd < 0xA0) {
    // Various commands dealing with serial baud rate
    if (cmd < 0x9E) {
      //We will now set and use this baud rate
      serialBaudReset(cmd);
    } else if (cmd == 0x9E) {
      //Store the Desired serial baud rate
      eepromClearAddr(ADDR_BAUD);
      eepromSave(ADDR_BAUD, currentBaud);
      int baudVal = eepromRead(ADDR_BAUD);
      serialBaudReset(baudVal);
    } else if (cmd == 0x9F) {
      //clear out the Serial Baud rate
      eepromClearAddr(ADDR_BAUD); //EEPROM address of Baud
    }
    // end if cmd is in the 0x90s
  }
  return;
}

void makeTheF() {
  // Output a F-like shape to show all rows & columns are working
  colData[0] = 127;      // first column of test pattern
  colData[1] = 64;       // second column of test pattern
  colData[2] = 64;       // third column of test pattern
  colData[3] = 64;       // fourth column of test pattern
  colData[4] = 64;       // fifth column of test pattern
  return;
}

void clearPanel() {
  // Clear the panel so it's blank
  for (int i = 0; i < 5; i++)
    colData[i] = 0;
  return;
}

void clearReads() {
  // initialize all the readings to 0: 
  for (int thisReading = 0; thisReading < numReadings; thisReading++)
    readings[thisReading] = 0;
  // reset all our counters to 0
  // By setting runs < numReadings, we will get loop() to take readings on ADC7
  runs = 0;     // the index of the current reading
  total = 0;    // the running total
  val = 0;      // raw value from the auto-addressing pin
  return;
}

//Serial Baud restart
void serialBaudReset(int serVal) (
   // Codes ranging from 0x90 - 0x9F will change the serial baud rate dynamically on all boards.
    Serial.end();                  // Stop the serial port
    delay(100);                    // Delay 0.1 seconds
    // Output the raw A7 pin reading onto the test pattern in this manner:
    // X    X    X    X    X
    // X    o    o    o    o
    // X 512k 256k 128k  64k
    // X  32k  16k   8k   4k
    // X   2k   1k  512  256 
    // X  128   64   32   16
    // X    8    4    2    1
    clearPanel();
    currentBaud = serVal;
    unsigned long newBaudRate = supportedBauds[serVal & 0xF];
    if (inLoop) {
      for (int i = 19; i >= 0; i--) {
        // Columns go from MSB to LSB as shown above, hence 4 - (i % 4)
        // If val has the (1 << i)th bit set, then shift that "1" value up by (i / 4) + 3 places
        // +3 because we need this to appear 3 rows higher than the bottom
        unsigned long one = (unsigned long) 1;
        unsigned long shift = one << i;
        colData[4 - (i % 4)] |= ((newBaudRate & shift) == shift) << (i / 4);
      }
    }
    Serial.begin(newBaudRate);   // Begin the serial at the specified baud rate
}

//Clears the Entire EEPROM
void eepromClear() {
  //ATmega168 has 512 bytes of EEPROM, ATmega328 has 1024 
  for (int i = 0; i < 512; i++)
    EEPROM.write(i, 0);
}

//Clears specific EEPROM address
void eepromClearAddr(int byteLoc) {
  EEPROM.write(byteLoc, 0);
}

//Saves the desired value (data) to the specified location in EEPROM
void eepromSave(int byteLoc, int data){
  EEPROM.write(byteLoc, data);
}

//Read the value at this address on the EEPROM
int eepromRead(int byteLoc) {
  return EEPROM.read(byteLoc); 
}

// Load the Saved Address
void getSavedBoardAddr() {
  int proc = eepromRead(ADDR_BOARD);
  if (proc != 0)
    procID = proc;
}

void setup() {
  
  inLoop = false;
  // show power
  digitalWrite(colActive[0], HIGH);
  digitalWrite(rowActive[0], HIGH);
  digitalWrite(colActive[0], LOW);
  // serialBaudReset can initialize serial communication too ;)
  serialBaudReset(eepromRead(ADDR_BAUD));
  // initialize the LED pins as outputs:
  for (int i = 1; i < 14; i++)
    pinMode(i, OUTPUT);
  // Output a F-like shape to show all rows & columns are working
  makeTheF();
  getSavedBoardAddr();       // Read any saved address
  activeProc = false;    // this processor isn't active now
  delay(1000);           // stall for 1 second
  // initialize all the readings to 0: 
  clearReads();
  inLoop = true;

}

void loop() {
  
  if (runs > numReadings) {
    // see if there's incoming serial data:
    if (Serial.available() > 0) {
      // read the oldest byte in the serial buffer:
      incomingByte = Serial.read();
      // if it's the reserved bit, start changing the letter:
      if ((incomingByte & 0x80) == 0x80) {     // otherwise if bit 8 is set (indicating a proc ID that's not us),
        activeProc = false;                    // don't reset the column values on this processor 
        if (!cmdMode) {
          if (incomingByte == procID) {        // if the reserved bit = this proc's ID,
            colWriter = 0;                     // reset column writer to the first column (2 in this case)
            activeProc = true;                 // mark this as the active processor
          }
          cmdMode = (incomingByte == 0xFF);   // Address 0xFF could be a valid board or a control signal
        } else {
          runCommand(incomingByte);            // Handle the command we just received
          cmdMode = false;
        }
      } else {                                 // if the incoming byte is < 0x80, it's a letter
        if (colWriter < 5 && activeProc) {     // if this proc is active & the column being written is < 5,
          colData[colWriter] = incomingByte;   // change the column of this proc to be the incoming byte
          colWriter++;  
        }
        cmdMode = false;
      }
    }
    
    // This following portion needs to run regardless of if serial is available
    
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
  } else if (runs == numReadings || procID == 0) {
    // Reset the column data to show the test pattern
    makeTheF();
    if (procID == 0){
      // Calculate the ID of this processor based on the average calculated earlier
      procID = floor(val / 16); // find the "not-quite" board index
      procID = 63 - procID;  // flip the number around so lowest voltage drop comes first
      procID += 128;         // Proc IDs should always begin with 0x80
      procID += 64;          // GREEN ONLY: Add 64 (0x40) to the chip ID
    }
    runs++;                // Go into the main loop after this
    showAddrDebug();       // Show debug output for addressing
  }
}

void showAddrDebug() {
  // Output the chip ID onto the test pattern in this manner:
  // X  X  X  X  X
  // X  o  o  o  o
  // X  o  o  o  o
  // X  o  o  o  o
  // X  o  o  o  o
  // X  o  o 32 16
  // X  8  4  2  1
  for (int i = 7; i >= 0; i--) {
    // Columns go from MSB to LSB as shown above, hence 4 - (i % 4)
    // If procID has the (1 << i)th bit set, then shift that "1" value up by (i / 4) places
    colData[4 - (i % 4)] |= ((procID & (1 << i)) == (1 << i)) << (i / 4);
  }
  // Output the raw A7 pin reading onto the test pattern in this manner:
  // X    X   X   X   X
  // X    o  1k 512 256 
  // X  128  64  32  16
  // X    8   4   2   1
  // X    o   o   o   o
  // X    o   o   o   o
  // X    o   o   o   o
  for (int i = 11; i >= 0; i--) {
    // Columns go from MSB to LSB as shown above, hence 4 - (i % 4)
    // If val has the (1 << i)th bit set, then shift that "1" value up by (i / 4) + 3 places
    // +3 because we need this to appear 3 rows higher than the bottom
    colData[4 - (i % 4)] |= ((val & (1 << i)) == (1 << i)) << ((i / 4) + 3);
  }
}
