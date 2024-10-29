# IO22D08 Arduino Library

The Eletechsup IO22D08 is an I/O board for an Arduino Pro Mini; it provides:

- 8 x relay outputs (10A NO/NC outputs) + LED per channel
- 8 x optically isolated inputs
- 4 x pushbuttons
- 4 x 9-segment LED display (88:88), handy for time/state info

This project contains an Arduino library that can be used to interact with the
IO22D08 hardware and an example sketch that exercises the library.

Eletechsup appear to manufacture a number of related products, including the
IO22C04 (4 relay/input version of the IO22D08) and other boards that use a
Nano and provide an RS485 I/O interface, and/or 4-20mA inputs and so on. This
appears to be their official web site and AliExpress store:

- https://485io.com/expansion-board-c-14/
- https://eletechsupeletechsup.aliexpress.com/

These seem to have the same basic design: inputs directly connected to the
Arduino, outputs (Darlington transistor array such as the ULN2803A) and LED
display driven via a chain of 74HC595D shift registers. In some cases the relays
are driven directly and only the display via shift registers. This library could
likely be readily adapted to work with these other boards.

## Wokwi Emulation

A Wokwi emulation of the significant parts of the board is here: https://wokwi.com/projects/410684313849767937

The following shell pipeline below assembles this project's example into a form suitable for Wokwi:

```shell
cat IO22D08.cpp | sed '/#include "IO22D08.h"/{
s/#include "IO22D08.h"//
r IO22D08.h
}' | ( cat; sed 's/#include "IO22D08.h"//' examples/IO22D08.ino ) | pbcopy
```
