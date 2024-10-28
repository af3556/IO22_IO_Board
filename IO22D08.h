#ifndef IO22D08_h

#define IO22D08_h

#include "Arduino.h"

class IO22D08
{
  // this class wraps the IO22D08 hardware
  // - TODO: generalise to support the IO22C04 variant?
  //   - the IO22C04 has its four relay outputs directly connected to the
  //     micro; i.e only has two shift registers for the display

  public:
    static const uint8_t numDisplayDigits = 4;
    static const uint8_t numRelays = 8;
    static const uint8_t numInputs = 8;
    static const uint8_t numButtons = 4;

    static const uint8_t numDisplayMessages = 4;
    static const uint8_t MESSAGE_BLANK = 0;
    static const uint8_t MESSAGE_ON = 1;
    static const uint8_t MESSAGE_OFF = 2;
    static const uint8_t MESSAGE_ERR = 3;

    IO22D08();
    void begin();

    void displayNumber(uint16_t n);
    void setColon(bool state);
    void toggleColon();
    void displayCharacter(size_t n, uint8_t c);
    void displayMessage(uint8_t m);

    // relay masks
    static const uint8_t RELAY1 = 1<<1;
    static const uint8_t RELAY2 = 1<<2;
    static const uint8_t RELAY3 = 1<<3;
    static const uint8_t RELAY4 = 1<<4;
    static const uint8_t RELAY5 = 1<<5;
    static const uint8_t RELAY6 = 1<<6;
    static const uint8_t RELAY7 = 1<<7;
    static const uint8_t RELAY8 = 1<<0;
    static const uint8_t RELAYS_ALL = 0xFF;
    static const uint8_t RELAY_ON = 0xFF;
    static const uint8_t RELAY_OFF = 0x00;

    void refreshDisplayAndRelays();
    void enableRelays();
    void disableRelays();

    void relaySet(uint8_t mask, uint8_t state);
    // relayNum = simple numerical sequence, e.g. 3 (meaning RELAY3)
    uint8_t relayNumToMask(uint8_t n);
    void relaySetN(uint8_t relayNum, bool state);
    uint8_t relayGet();
    bool relayIsOn(uint8_t relayNum);

    const uint8_t inputPins[numInputs] {2, 3, 4, 5, 6, A0, 12, 11};    // IN1-8
    const uint8_t buttonPins[numButtons] {7, 8, 9, 10};                // K1-K4/B1-B4

  protected:

    // board connections
    // ref. circuit diagram for labels
    // - latch and clock are shared across the three shift registers
    // - no idea why the board designers didn't use the hardware serial pins (SPI)
    const uint8_t _latchPin = A2;
    const uint8_t _clockPin = A3;
    // - data is shifted out to the first register
    const uint8_t _dataPin = 13;

    // - relay shift register (U5) output enable; active low
    const uint8_t _relayOEpin = A1;

    uint16_t _displayBuffer[numDisplayDigits];  // display shift register buffer (n digits x 16bits ea.)
    uint8_t _relayBuffer = 0;                   // relay shift register buffer
    bool _displayColon = false;                 // enable the display colon

    // constexpr should work? but results in the linker complaining:
    //  "undefined reference to `IO22D08::characters" on dereference
    //  when being used as: digitBuffer = characters[c];
    // - guessing this is due to the AVR's memory architecture (and mixing thereof) ¯\_(ツ)_/¯
    // benefit of constexpr, if it worked, would be not having to specify the size of the array
    // constexpr static uint16_t characters[] =
    // - see display.md for details on the 7-segment display and how the _characters
    // constants are calculated
    const uint16_t _characters[17] =
    {
      0x2008, // 0
      0x7A08, // 1
      0xE000, // 2
      0x6200, // 3
      0x3A00, // 4
      0x2210, // 5
      0x2010, // 6
      0x6A08, // 7
      0x2000, // 8
      0x2200, // 9
      0xFA18, // 10 ' ' (i.e. blank)
      0x2008, // 11 O
      0x7810, // 12 n
      0xA810, // 13 F
      0xA010, // 14 E
      0xF810, // 15 r
      0xF218, // 16 _
    };

    // to enable a digit the appropriate K1-K4 bit needs to be set high
    // - if the display were common cathode these constants would be
    //   pre-inverted to skip the XOR that would otherwise be required
    // - in keeping with the button sequencing, digit 1 is the left-most digit,
    //   4 the right-most
    uint16_t _digitSelect[numDisplayDigits] =
    {
      0x0400, // K1 (left-most)
      0x0002, // K2
      0x0004, // K3
      0x0020, // K4 (right-most)
    };
    // DP = U3:Q5; it'll get "mixed in" to each digit
    // for the IO22D08 board only DP2 and DP3 are connected as 'colon' LEDs
    const uint16_t _dpSegment = 0xDFFF;

    const uint8_t _displayMessages[numDisplayMessages][numDisplayDigits] =
    {
      {10, 10, 10, 10}, // '    '
      {10, 10, 11, 12}, // '  On'
      {10, 11, 13, 13}, // ' OFF'
      {10, 14, 15, 15}, // ' Err'
    };

    void _updateDigit(size_t d, uint8_t c);
    void _updateDigitSelect(size_t n);
    void _updateColon();

};


#endif
