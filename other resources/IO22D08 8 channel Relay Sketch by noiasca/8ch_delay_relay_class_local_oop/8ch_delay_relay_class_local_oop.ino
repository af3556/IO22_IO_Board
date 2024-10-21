/*
  IO22D08
  DC 12V 8 Channel Pro mini PLC Board Relay Shield Module
  for Arduino Multifunction Delay Timer Switch Board

  Hardware:
  4 bit Common Cathode Digital Tube Module (two shift registers)
  8 relays (one shift register)
  8 optocoupler
  4 discrete input keys (to GND)

  ---Segment Display Screen----
  --A--
  F---B
  --G--
  E---C
  --D--
   __   __   __   __
  |__| |__|:|__| |__|
  |__|.|__|.|__|.|__|.
  --------------------

  available on Aliexpress: https://s.click.aliexpress.com/e/_A0tJEK

  some code parts based on the work of
  cantone-electonics
  http://www.canton-electronics.com

  this version

  by noiasca
  2022-08-28 OOP - external timer (2260/135)
  2021-04-01 OOP - inheritance (2340/122)
  2021-03-31 initial version (2368/126)
  1999-99-99 OEM Version (2820/101)
*/

//Pin connected to latch of Digital Tube Module
// ST Store
// de: Der Wechsel von Low auf High kopiert den Inhalt des Shift Registers in das Ausgaberegister bzw. Speicherregister
const uint8_t latchPin = A2;
//Pin connected to clock of Digital Tube Module
// SH clock Shift Clock Pin
//de: Übernahme des Data Signals in das eigentliche Schieberegister
const uint8_t clockPin = A3;
//Pin connected to data of Digital Tube Module
const uint8_t dataPin = 13;
//Pin connected to 595_OE of Digital Tube Module
// Output Enable to activate outputs Q0 – Q7  - first device: Relay IC
const uint8_t OE_595 = A1;
// A4 - unused - not connected - I2C SDA - can be used for an additional LCD display
// A5 - unused - not connected - I2C SCL - can be used for an additional LCD display
// A6 - unused - not connected - can be used as analog input only
// A7 - unused - not connected - can be used as analog input only
const uint8_t optoInPin[] {2, 3, 4, 5, 6, A0, 12, 11};     // the input GPIO's with optocoupler - LOW active
const uint8_t keyInPin[] {7, 8, 9, 10};                    // the input GPIO's with momentary buttons - LOW active
const uint8_t noOfOptoIn = sizeof(optoInPin);              // calculate the number of opto inputs
const uint8_t noOfKeyIn = sizeof(keyInPin);                // calculate the number of discrete input keys
const uint8_t noOfRelay = 8;                               // relays on board driven connected to shift registers
byte key_value;                                            // the last pressed key

// the base class implements the basic functionality
// which should be the same for all sketches with this hardware:
// begin()     init the hardware
// setNumber() send an integer to the internal display buffer
// pinWrite()  switch on/off a relay
// update()    refresh the display
// tick()      keep internals running
class IO22D08 {
  protected:
    uint8_t dat_buf[4];                // the display buffer - reduced to 4 as we only have 4 digits on this board
    uint8_t relay_port;                // we need to keep track of the 8 relays in this variable
    uint8_t com_num;                   // Digital Tube Common - actual digit to be shown
    uint32_t previousMillis = 0;       // time keeping for periodic calls

    // low level HW access to shift registers
    // including mapping of pins
    void update()
    {
      static const uint8_t TUBE_NUM[4] = {0xfe, 0xfd, 0xfb, 0xf7}; // Tuble bit number - the mapping to commons
      // currently only the first 10 characters (=numbers) are used, but I keep the definitions
      //        NO.:0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22 23 24 25 26 27 28*/
      // Character :0,1,2,3,4,5,6,7,8,9,A, b, C, c, d, E, F, H, h, L, n, N, o, P, r, t, U, -,  ,*/
      const uint8_t TUBE_SEG[29] =
      { 0xc0, 0xf9, 0xa4, 0xb0, 0x99, 0x92, 0x82, 0xf8, 0x80, 0x90, // 0 .. 9
        0x88, 0x83, 0xc6, 0xa7, 0xa1, 0x86, 0x8e, 0x89, 0x8b, 0xc7, // A, b, C, c, d, E, F, H, h, L,
        0xab, 0xc8, 0xa3, 0x8c, 0xaf, 0x87, 0xc1, 0xbf, 0xff        // n, N, o, P, r, t, U, -,  ,
      };
      uint8_t tube_dat;                                    // Common Cathode Digital Tube, bit negated - sum of all segments to be activated
      uint8_t bit_num;                                     // digital tube common gemappt auf tuble bit number
      uint8_t display_l, display_h, relay_dat;             // three databytes of payload to be shifted to the shift registers
      if (com_num < 3) com_num ++; else com_num = 0;       // next digit
      uint8_t dat = dat_buf[com_num];                      // Data to be displayed
      tube_dat = TUBE_SEG[dat];                            // Common Cathode Digital Tube, bit negated - sum of all segments
      bit_num = ~TUBE_NUM[com_num];                        // digital tube common gemappt auf tuble bit number
      display_l  = ((tube_dat & 0x10) >> 3);     //Q4   <-D1 -3    SEG_E
      display_l |= ((bit_num  & 0x01) << 2);     //DIGI0<-D2 +2
      display_l |= ((tube_dat & 0x08) >> 0);     //Q3   <-D3 0     SEG_D
      display_l |= ((tube_dat & 0x01) << 4);     //Q0   <-D4 -4    SEG_A
      display_l |= ((tube_dat & 0x80) >> 2);     //Q7   <-D5 -2    SEG_DP - Colon - only on digit 1 ?
      display_l |= ((tube_dat & 0x20) << 1);     //Q5   <-D6 1     SEG_F
      display_l |= ((tube_dat & 0x04) << 5);     //Q2   <-D7 5     SEG_C
      // output U3-D0 is not connected,
      // on the schematic the outputs of the shiftregisters are internally marked with Q, here we use U3-D to refeer to the latched output)
      display_h  = ((bit_num  & 0x02) >> 0);     //DIGI1<-D1 0
      display_h |= ((bit_num  & 0x04) >> 0);     //DIGI2<-D2 0
      display_h |= ((tube_dat & 0x40) >> 3);     //Q6   <-D3 -3    SEG_G
      display_h |= ((tube_dat & 0x02) << 3);     //Q1   <-D4 3     SEG_B
      display_h |= ((bit_num  & 0x08) << 2);     //DIGI3<-D5 2
      // Outputs U4-D0, U4-D6 and U4-D7 are not connected

      relay_dat = ((relay_port & 0x7f) << 1);    // map Pinout 74HC595 to ULN2803: 81234567
      relay_dat = relay_dat | ((relay_port & 0x80) >> 7);

      //ground latchPin and hold low for as long as you are transmitting
      digitalWrite(latchPin, LOW);
      // as the shift registers are daisy chained we need to shift out to all three 74HC595
      // hence, one single class for the display AND the relays ...
      // de: das ist natürlich ein Käse dass wir hier einen gemischten Zugriff auf das Display und die Relais machen müssen
      shiftOut(dataPin, clockPin, MSBFIRST, display_h);    // data for U3 - display
      shiftOut(dataPin, clockPin, MSBFIRST, display_l);    // data for U4 - display
      shiftOut(dataPin, clockPin, MSBFIRST, relay_dat);    // data for U5 - Relay
      //return the latch pin high to signal chip that it no longer needs to listen for information
      digitalWrite(latchPin, HIGH);
    }

  public:
    IO22D08() {}

    void begin() {
      digitalWrite(OE_595, LOW);       // Enable Pin of first 74HC595
      pinMode(latchPin, OUTPUT);
      pinMode(clockPin, OUTPUT);
      pinMode(dataPin, OUTPUT);
      pinMode(OE_595, OUTPUT);
    }

    // fills the internal buffer for the digital outputs (relays)
    void pinWrite(uint8_t pin, uint8_t mode)
    {
      //Serial.print (F("pinWrite ")); Serial.print (pin); mode == LOW ? Serial.println(F(" LOW")) :  Serial.println(F(" HIGH"));
      // pin am ersten shiftregister ein oder ausschalten
      if (mode == LOW)
        bitClear(relay_port, pin);
      else
        bitSet(relay_port, pin);
      update();    // optional: call the shiftout process (but will be done some milliseconds later anyway)
    }

    // this is a first simple "print number" method
    // right alligned, unused digits with zeros
    // should be reworked for a nicer print/write
    void setNumber(int display_dat)
    {
      dat_buf[0] = display_dat / 1000;
      display_dat = display_dat % 1000;
      dat_buf[1] = display_dat / 100;
      display_dat = display_dat % 100;
      dat_buf[2] = display_dat / 10;
      dat_buf[3] = display_dat % 10;
    }

    // this method should be called in loop as often as possible
    // it will refresh the multiplex display
    void tick() {
      uint32_t currentMillis = millis();
      if (currentMillis - previousMillis > 1)    // each two milliseconds gives a stable display on pro Mini 8MHz
      {
        update();
        previousMillis = currentMillis;
      }
    }
};

IO22D08 io22d08;  // create an instance of the relay board

// the timer class does the time management for one relay
// Each timer needs to be linked to one relay
// in this implementation we just link them by the array index
// timer[0] is for the first relay on the board, timer[7] is the last "Relais 8"
class Timer
{
  protected:
    const byte id;                     // the associated relay on the board
    static byte nextId;                // one static counter for the total number of timer
    bool isActive;                     // is timer of this relay running
    uint32_t previousMillis;           // start time of relay

  public:
    Timer() : id(nextId++) {}          // associate each Timer with an relay on the IO22D08 board
        
    uint16_t delayTime;                // delay time of this relay - I'm to lazy to write a setter, therefore public

    void startTimer() {                // start the timer and activate the output
      previousMillis = millis();
      //Serial.print (F("Timer startet ")); Serial.println(id);
      isActive = true;
      io22d08.pinWrite(id, HIGH);
    }

    void tick() {                      // a do/run method for all timers
      uint32_t currentMillis = millis();

      // 01 check if there is something to do for the relay timer:
      if (isActive)                    // check for switch off
      {
        if (currentMillis - previousMillis > delayTime * 1000UL)
        {
          isActive = false;
          io22d08.pinWrite(id, LOW);
        }
      }
    }

    void show()
    {
      // 02 update the output buffer
      if (isActive && id == key_value)
        io22d08.setNumber(delayTime - (millis() - previousMillis) / 1000UL);   // calculate remaining time
      else
        io22d08.setNumber(delayTime);  // just show programmed delay time
    }
};
byte Timer::nextId = 0;                // initialization of a static member needs to be outside of the class definition

Timer timer[noOfRelay];                // create [several] instances of timers one for each relay on board

void inputRead()
{
  for (size_t i = 0; i < noOfOptoIn; i++)
  {
    if (digitalRead(optoInPin[i]) == LOW)
    {
      //Serial.print (F("opto ")); Serial.println(i);
      timer[i].startTimer();           // activate pin on board
    }
  }
  for (size_t i = 0; i < noOfKeyIn; i++)
  {
    if (digitalRead(keyInPin[i]) == LOW)
    {
      //Serial.print (F("key ")); Serial.println(i);
      key_value = i;                   // set the last pressed key
    }
  }
}

void displayUpdate(uint32_t currentMillis = millis())      // update the display if needed
{
  static uint32_t previousMillis = 0;
  if (currentMillis - previousMillis > 500)
  {
    timer[key_value].show();
    previousMillis = currentMillis;
  }
}

void setup() {
  //Serial.begin(9600);                  // slow for 8Mhz Pro Mini
  //Serial.println("\nIO22D08 board");
  for (auto &i : optoInPin) pinMode(i, INPUT_PULLUP);      // init all optocoupler
  for (auto &i : keyInPin) pinMode(i, INPUT_PULLUP);       // init all discrete input keys
  io22d08.begin();                                         // prepare the board hardware
  // set some default values
  timer[0].delayTime = 16;   // 1-9999 seconds, modify the number change the delay time
  timer[1].delayTime = 2;
  timer[2].delayTime = 3;
  timer[3].delayTime = 4;
  timer[4].delayTime = 5;
  timer[5].delayTime = 6;
  timer[6].delayTime = 7;
  timer[7].delayTime = 8;  
}

void loop() {
  inputRead();                         // handle input pins
  displayUpdate();                     // send new data to display
  io22d08.tick();                      // handle the board hardware
  for (auto & i : timer) i.tick();     // timekeeping to check if relais need to be changed
}
