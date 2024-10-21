/*
  IO22D08 "8ch Pro Mini PLC"
  https://eletechsup.taobao.com/ (Chinese only)

  I/O board for an Arduino Pro Mini 5V (16MHz)
  - 8 x relay outputs (10A NO/NC outputs) + LED per channel
  - 8 x optically isolated inputs
    - tied high to 5V via 1k
  - 4 x pushbuttons
	  - directly connected to Mini pins 7-10
  - 4 x 9-segment LED display (88:88), handy for time/state info

  - the "PLC" board uses 3x8-bit shift registers to drive the outputs (relays
    and LED display) using only 4 IOs (3 for the registers, 1 for output enable)
    - the shift register driving the relays has its output enable connected to
      the Mini (pin A1); presumably intended as a "shortcut" to disable the
      relays without having to shift in 0's
    - the other two shift registers are for the display
      - displays are also multiplexed (micro must refresh)
  - inputs are passed through to dedicated pins, not multiplexed

  - the Pro Mini's A4,A5 (I2C etc) and A6,A7 (ADC only) are not used by the
    PLC board

  - operating voltage: DC 12V
  	- standby 12mA
  	- w/ LED display: ~48mA
  	- each relay +30mA; e.g. all 8 = +240, ~290mA total

  - simulation: https://wokwi.com/projects/410684313849767937

  - eletechsup apparently provided example code in a tarball distributed via
    MS OneDrive (!), dead as of 2024
    - some discussion here: https://forum.arduino.cc/t/io22d08-control-and-library/
    - this work is derived from https://werner.rothschopf.net/microcontroller/202104_arduino_pro_mini_relayboard_IO22D08_en.htm
      (aka Noiasca?)
      - in turn, that apparently incorporates work from http://www.canton-electronics.com
        (offline) and/or eletechsup

  - summary of changes in this work:
    - documented / RE'd the operation of the shift registers that drive the
      relays and display
    - Arduino style guide for naming (i.e. camelCase)
    - major refactoring for managing display and relays via shift registers
    - added a "self test" at power on to verify the display is working as
      expected


- errata in previous works
  - the PLC schematic labelling leaves a _lot_ to be desired
  - button labels (K1-K4) are reversed on the board silk screen / labels
    - i.e. what's marked as "K4" on the board is connected to pin 7 on the micro
  - code provided by eletechsup (?) refers to the display both as common anode
    and common cathode; it's actually common anode
  - the code from werner.rothschopf.net references IC label that do not match
    the schematic in this repo
  - these issues only impact the display digit mappings / constants, all comes
    out in the wash


- see display.md for details on the 7-segment display and how the _characters
  constants are calculated

*/


#include "Arduino.h"
#include "IO22D08.h"

IO22D08::IO22D08() {}

void IO22D08::begin() {
  pinMode(_latchPin, OUTPUT);
  pinMode(_clockPin, OUTPUT);
  pinMode(_dataPin, OUTPUT);

  // board has pullups; even then leave this to the button library
  for (auto &i : inputPins) pinMode(i, INPUT_PULLUP);
  for (auto &i : buttonPins) pinMode(i, INPUT_PULLUP);

  disableRelays(); // start off, off
  pinMode(_relayOEpin, OUTPUT);
}

void IO22D08::_updateDigitSelect(size_t n)
{
  // set the relevant digit select bit (common anode)
  // - needs to be done after every buffer update
  // - in keeping with the button sequencing, digit 1 is the left-most digit,
  //   4 the right-most
  _displayBuffer[n] |= _digitSelect[n];
}

// write a given character to a specific digit of the display
// - n: digit position (0..numDisplayDigits-1)
// - c: character (index into _characters[])
void IO22D08::_updateDigit(size_t n, uint8_t c)
{
  _displayBuffer[n] =  _characters[c];
}

void IO22D08::_updateColon()
{
  // mix in the colon (DP segments on digits 1,2) as required
  if (_displayColon)
  {
    _displayBuffer[1] &= _dpSegment;
    _displayBuffer[2] &= _dpSegment;
  }
  else
  {
    _displayBuffer[1] |= ~_dpSegment;
    _displayBuffer[2] |= ~_dpSegment;
  }
  _updateDigitSelect(1);
  _updateDigitSelect(2);
}

void IO22D08::displayCharacter(size_t n, uint8_t c)
{
  _updateDigit(n, c);
  _updateDigitSelect(n);
  _updateColon();
}

void IO22D08::displayNumber(uint16_t number)
{
  for (size_t n = numDisplayDigits; n-- > 0;)
  {
    _updateDigit(n, number % 10);
    _updateDigitSelect(n);
    number /= 10;
  }
  _updateColon();
}

void IO22D08::displayMessage(uint8_t m)
{
  for (size_t n = 0; n < numDisplayDigits; n++)
  {
    _updateDigit(n, _displayMessages[m][n]);
    _updateDigitSelect(n);
  }
  _updateColon();
}

void IO22D08::setColon(bool state)
{
    _displayColon = state;
    _updateColon();
}

void IO22D08::toggleColon()
{
    _displayColon ^= true;
    _updateColon();
}


void IO22D08::refreshDisplayAndRelays()
{
  // shift out the entire display: each digit preceded by the relay register
  for (auto &d : _displayBuffer)
  {
    digitalWrite(_latchPin, LOW);
    // - shiftOut() only accepts a byte at a time
    shiftOut(_dataPin, _clockPin, MSBFIRST, lowByte(d));  // U4
    shiftOut(_dataPin, _clockPin, MSBFIRST, highByte(d)); // U3
    shiftOut(_dataPin, _clockPin, MSBFIRST, _relayBuffer);  // U5
    digitalWrite(_latchPin, HIGH);
  }
}


// the PLC board connects the relay shift register's output enable (OE)
// to _relayOEpin; when disabled (high impedance) the ULN2803 transistor
// array that actually drives the relay coils will turn off all relays
// - this is quicker than having to shift in 0's to the relay SR, and
//   also allows the relays to be disabled and re-enabled back to their
//   prior state
void IO22D08::enableRelays() {
  digitalWrite(_relayOEpin, LOW);
}
void IO22D08::disableRelays() {
  digitalWrite(_relayOEpin, HIGH);
}

// the relays are managed en-masse: as a set via the shift-register, not
// individually by dedicated output pins; hence the use of octet-wide operations
// here instead of separate bit*() ops
//
// set relays given a mask to select the desired relay(s) and the desired
// state for the selected relays
// mask bits are the relays to be changed
// state bits are on/off
// e.g. relaySet(0x02, 0xFF) will turn on relay 2
// e.g. relaySet(0x02, 0x02) will also turn on relay 2
// e.g. relaySet(0x02, true) will _not_ turn on relay 2 (true == 0x01)
// e.g. relaySet(0x02, 0x00) will turn off relay 2
// e.g. relaySet(0x02, 0xF1) will also turn off relay 2, but not in a clear way
// e.g. relaySet(0x0F, 0xFF) will set relays 4-1 on
// e.g. relaySet(0xAA, 0x00) will set the even numbered relays off
// e.g. relaySet(0x00, 0xXX) will make no changes (no relays selected)
// e.g. relaySet(0xFF, 0x00) will turn off all relays
// e.g. relaySet(R_ALL, RELAY_OFF) will turn off all relays
// e.g. relaySet(R1+R3+R6, RELAY_ON) will turn on relays 1, 3 and 6

uint8_t IO22D08::relayNumToMask(uint8_t relayNum)
{
  // relays are mapped to SR outputs as 76543218
  // i.e. relay numbers 8,1-7 => bits 0,1-7
  if (relayNum >= 8) relayNum = 0;
  return (1<<relayNum);
}

void IO22D08::relaySet(uint8_t mask, uint8_t state)
{
  // 1) _clear_ the bits in the _relayBuffer that are to be changed (i.e.
  //    indicated in the mask): _relayBuffer & ~mask
  // 2) set the bits that are to be set (first masking off state to remove
  //    any extraneous bits that we shouldn't be paying attention to)
  _relayBuffer = (_relayBuffer & ~mask) | (state & mask);
}

// set state of a specific relay number/ID
void IO22D08::relaySetN(uint8_t relayNum, bool state)
{
  relaySet(relayNumToMask(relayNum), state ? RELAY_ON : RELAY_OFF);
}

uint8_t IO22D08::relayGet()
{
  return _relayBuffer;
}

// get state of a specific relay number/ID
bool IO22D08::relayGetN(uint8_t relayNum)
{
  return _relayBuffer & relayNumToMask(relayNum);
}