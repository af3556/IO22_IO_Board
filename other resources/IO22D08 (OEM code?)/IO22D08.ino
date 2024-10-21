// if Arduno NANO ar attached as MINI  connect in NANO pin D10 to A7 and pin D11 to A6
// remove comment NANO_AS_MINI if NANO is used
//#define NANO_AS_MINI

#define USE_LCD 1

#include "FlexiTimer2.h"

#include "defsAndInputs.h"
#include "lcdAndRelays.h"
#include "xdemo.h"


void setup() {
  Serial.begin(57600);

  initIO();

  if (USE_LCD) {
    clearLCD();
    // if  don't use FlexiTimer remove here
    FlexiTimer2::set(1, 1.0 / 1000, TubeDisplay4Bit); // call every 1ms "ticks"
    FlexiTimer2::start();

    Serial.println(" LCD AND RELAY TESTS ");
  }
  else
  {
    Serial.println(" RELAY TESTS ");
    currentStep = 5;
    i = -1;
  }
}



void loop() {

  // read inputs and keys
  // noise reduction 50ms
  // in array inValues and keysValues stored inputs status : IO_LOW-0 IO_HIGH-1  IO_RISING-2 IO_FALLING-3
  // inputs and keys have reaction(ex pressed) if they connected to ground - negative logic
  // input free - value IO_LOW = 0
  // input start to ground - value IO_RISING = 2
  // input connected to ground  - value IO_HIGH = 1
  // input going to free from ground - value IO_FALLING = 3

  readINPUTS();
  readKEYS();

  // using readed values
  // read IN1
  // byte in1=inValues[0]; // 0- is first IN. 7 - is last (8) input.
  // byte key1=keysValues[0]; // 0- is first KEY. 3 - is last (4) key



  // if don't use flexiTimer uncomment this:
  // if (USE_LCD) TubeDisplay4Bit();

  DEMO(); // see xdemo.h for details

  /* avail functions


      clearLCD(); // clears all chars in LCD

      write a value to segment
      segment 0- is first 3 is last segment
      value
         int digit value from 0 to 45  ex 0,1,2, etc. -> more codes see lcdAndRelays
         char char code for digit ex '0', 'A', 'p';
      dots - optional print : ex in timer. - avail. only in second segment (segment=1)
      setLCDdigit(segment, value, dots);

      // wtite text to LCD
      setLCDtext("TEST");

      // write time to LCD
      hour, minute or minute second
      dots- oprional : between numbers
      setLCDTime(hour, minute,dots );


      // write int to lcd
      // int value - falue from -999 to 9999
      setLCDbyInt(intValue);



    ---Segment Display Screen----
    --A--
    F---B
    --G--
    E---C
    --D--
     __  __   __  __
    |__||__|.|__||__|
    |__||__|'|__||__|

    mask= ~( 0b (DOT)GFEDCBA);


      // write user defined char to specific segment
      mask is a negative which segments are visible
      setCustomChar(segment, mask, dots)



    // write user defined char to specific segment
    byte setCustomChar(segment,  a,  b,  c, d,  e,  f,  g ,  dots )
    return mask ex for future use;

// RELAYS


    setRelay(relayNo); - set relay on  first is 1 last is 8
    resetRelay(relayNo); - reset relay off  first is 1 last is 8

    setRelayAndOffRest(relayNo);  - set relayNo on and reset others

    byte getRelay(relayNo); - returns specific relay state 0 (off)  or 1 (on)

    clearRelays();  - clears all relays - reset to off


   
  */





}
