
The IO22D08 is an I/O board for an Arduino Pro Mini; it provides:

- 8 x relay outputs (10A NO/NC outputs) + LED per channel
- 8 x optically isolated inputs
- 4 x pushbuttons
- 4 x 9-segment LED display (88:88), handy for time/state info

This project contains an Arduino library that can be used to interact with the
IO22D08 hardware and an example sketch that exercises the library.

## Wokwi Emulation

A Wokwi emulation of the significant parts of the board is here: https://wokwi.com/projects/410684313849767937

The following shell pipeline below assembles this project's example into a form suitable for Wokwi:

```shell
cat IO22D08.cpp | sed '/#include "IO22D08.h"/{
s/#include "IO22D08.h"//
r IO22D08.h
}' | ( cat; sed 's/#include "IO22D08.h"//' examples/IO22D08.ino ) | pbcopy
```
