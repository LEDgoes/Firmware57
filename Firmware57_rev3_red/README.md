5x7 Firmware Rev 3, Red
==========

Boards supported:
* 5x7 Production Boards
* 5x7 Rev 3 Prototype Boards

This firmware is to be installed on the right (red) chip on the board types specified above.

___.ino: This file is the human-readable firmware code.  Open this in the Arduino IDE.
___.cpp.hex: (Coming soon) This is the compiled sketch.  You can use AVRdude with an ISP (in-system programmer) or Arduino shield to write this file to the board (which is the compiled LEDgoes firmware only) if the bootloader is already in place.
___.hex: (Coming soon) This is the compiled sketch with bootloader.  You can use AVRdude with an ISP to erase the chip, set the fuses appropriately, and write all firmware including the bootloader.

Technique for using the Arduino IDE:

1. Select the board type "Lilypad Arduino w/ ATmega168" in Tools -> Board.  This should be available on Arduino IDE 1.0.5.
2a. If programming the board using an Arduino shield, select the COM port that the Arduino is plugged into in Tools -> Serial Port.
2b. If programming the board using an ISP, select the appropriate programmer in Tools -> Programmer.

Technique for using the AVRdude command line interface:

Using an Arduino:
> path\to\avrdude �C path\to\avrdude.conf -v -v �p atmega168 �c arduino �P <com port> -b 19200 -D -U flash:w:path/to/firmware.cpp.hex:i

Using a USBtiny:
> path\to\avrdude �C path\to\avrdude.conf -v -v �p atmega168 �c usbtiny -e �U lock:w:0x3F:m �U efuse:w:0x00:m �U hfuse:w:0xdd:m �U lfuse:w:0xe2:m 
> path\to\avrdude �C path\to\avrdude.conf -v -v �p atmega168 �c usbtiny �U flash:w:path\to\firmware.hex:i -Ulock:w:0x0F:m
