/* IO22D08.cpp

  The IO22D08 is an I/O board for an Arduino Pro Mini; it provides:
  - 8 x relay outputs (10A NO/NC outputs) + LED per channel
  - 8 x optically isolated inputs
  - 4 x pushbuttons
  - 4 x 9-segment LED display (88:88), handy for time/state info

  This example program exercises the IO22D08 library that controls the 8 relays
from the 4 onboard buttons and 8 external inputs. Usage/features:
- a short-press on buttons K1-K4 starts a timer for the corresponding relay
  R1-R4
- a long-press on buttons K1-K4 starts a timer for the corresponding relay
  R5-R8
- each of the 8 external inputs switch the corresponding relay directly on/off
  on short-press/release, start a timer on double-click, and latch on for
  a long-press
- in all cases, the display shows the time remaining on the timer that is next
  to expire; e.g. if three timers a, b, c are running with 16, 3 and 5 seconds
  remaining then the display will show b's time remaining, then switch to
  displaying c's, then finally a's
  - when no timers are running, the display is blanked except for the flashing
   colon (toggled every 0.5s)
- the main loop runs to about 3.3ms (300Hz)

  In addition, a "test mode" is provided that runs an alternate loop() if the
K1 button is held down in setup() when powering on. The test mode cycles various
values through the display and toggles the relay enable control.

*/

#include "IO22D08.h"

#include <AceButton.h>
using namespace ace_button;

IO22D08 io22d08;  // create an instance of the relay board

// AceButton is used to handle both the buttons K1-K4 and the inputs IN1-IN8
// - relays (or timers) are switched via button handler callbacks
ButtonConfig buttonConfig;
AceButton buttons[io22d08.numButtons];

ButtonConfig inputConfig;
AceButton inputs[io22d08.numInputs];


// simple timer controlling one or more relays that turns them on with start()
// and, when the elapsed time exceeds the given timeout, off via tick()
// - relayMasks are used to allow multiple relays to be switched together
class RelayTimer
{
  protected:
    uint32_t _previousMillis = 0;
    uint8_t _relayMask = 0;
    uint16_t _timeout = 1000UL;
    bool _isActive = false;

  private:
    RelayTimer(const RelayTimer& that);  // disallow copy constructor
    RelayTimer& operator=(const RelayTimer& that);  // disallow assignment operator

  public:

    RelayTimer() {}

    void setTimeout(uint8_t rm, uint16_t t)
    {
      _relayMask = rm;
      _timeout = t * 1000UL;
    }

    void start()
    {
      _previousMillis = millis();
      _isActive = true;
      // if _relayMask is 0, will be a no-op
      io22d08.relaySet(_relayMask, io22d08.RELAY_ON);
    }

    void stop()
    {
      _isActive = false;
      io22d08.relaySet(_relayMask, io22d08.RELAY_OFF);
    }

    void tick()
    {
      if (_isActive)
        if (millis() - _previousMillis > _timeout) stop();
    }

    uint16_t getTimeRemaining() const
    {
      if (_isActive)
        return _timeout - (millis() - _previousMillis);
      return -1;  // maxtime
    }
};

RelayTimer buttonTimers[io22d08.numRelays];  // for this demo, as many timers as relays
const size_t numButtonTimers (sizeof(buttonTimers)/sizeof(buttonTimers[0]));

RelayTimer inputTimers[io22d08.numRelays];  // for this demo, as many timers as relays
const size_t numInputTimers(sizeof(inputTimers)/sizeof(inputTimers[0]));


// handler for the onboard buttons K1-K4
// - in this example a short-press starts the timer for relays 1-4 and a
//   long-press for relays 5-8
// - the link between buttons and relays (the button ID) is established via the
//   AceButton's init
void buttonHandler(AceButton* button, uint8_t eventType, uint8_t /*buttonState*/)
{
  uint8_t buttonId = button->getId();

  Serial.print(F("B"));
  Serial.print(buttonId);
  Serial.print(F(":"));
  Serial.println(AceButton::eventName(eventType));

  switch (eventType) {
    case AceButton::kEventLongPressed:
      buttonId += 4; // "virtual" buttons 8-5
      [[fallthrough]];
    case AceButton::kEventClicked:
      // buttonIDs may be offset from timer indices; e.g. K3-8 with timers0-5
      buttonTimers[(buttonId-1)%numButtonTimers].start();  // button N => timer N-1
      break;
  }
}

// handler for the digital inputs IN1-IN8
// - in this example, simply switch the corresponding relay directly on/off,
//   on short-press/release; start a timer on "double-click"
// - the link between inputs and relays (the button ID) is established via the
//   AceButton's init
void inputHandler(AceButton* button, uint8_t eventType, uint8_t /*buttonState*/)
{
  uint8_t inputNum = button->getId();
  Serial.print(F("I"));
  Serial.print(inputNum);
  Serial.print(F(":"));
  Serial.println(AceButton::eventName(eventType));

  switch (eventType) {
    case AceButton::kEventPressed:
      io22d08.relaySetN(inputNum, true);
      break;
    case AceButton::kEventReleased:
      io22d08.relaySetN(inputNum, false);
      break;
    case AceButton::kEventClicked:
      io22d08.relaySetN(inputNum, !io22d08.relayGetN(inputNum));
      break;
    case AceButton::kEventDoubleClicked:
      inputTimers[inputNum-1].start();  // input N => timer N-1
      break;
  }
}


void (*loop_fn)() = loop_main;  // allow switching between main and testmode

void setup() {
  Serial.begin(9600);
  io22d08.begin();
  io22d08.displayMessage(io22d08.MESSAGE_BLANK);  // clear the display
  io22d08.enableRelays();

  Serial.println(F("\nIO22D08"));

  Serial.print(F("init buttons: "));
  buttonConfig.setEventHandler(buttonHandler);
  buttonConfig.setFeature(ButtonConfig::kFeatureClick);
  buttonConfig.setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig.setClickDelay(300);
  for (size_t b = 0; b < io22d08.numButtons; b++)
  {
    // button numbers/IDs count from 1, and are normally high (active low)
    buttons[b].init(&buttonConfig, io22d08.buttonPins[b], HIGH, b+1);
    Serial.print(b+1);
  }
  Serial.println(F("✔️"));

  Serial.print(F("init inputs: "));
  // the following provides:
  // - single pulse = relay pulse (stretched to accommodate double-click)
  // - long pulse = relay pulse (i.e. on for as long as input is active)
  // - double-pulse = start the timer
  // - long-press = turn on the relay, leave it on
  inputConfig.setEventHandler(inputHandler);
  inputConfig.setFeature(ButtonConfig::kFeatureClick);
  inputConfig.setFeature(ButtonConfig::kFeatureLongPress);
  inputConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  inputConfig.setFeature(ButtonConfig::kFeatureSuppressAll);
  for (size_t i = 0; i < io22d08.numInputs; i++)
  {
    // button numbers/IDs count from 1, and are normally high (active low)
    inputs[i].init(&inputConfig, io22d08.inputPins[i], HIGH, i+1);
    Serial.print(i+1);
  }
  Serial.println(F("✔️"));


  // go into test mode if K1 is held during boot
  if (buttons[0].isPressedRaw())
  {
    Serial.println(F("entering testmode"));
    // some test timers
    // - relays 1-4 on for 4s; 5-8 on for 8s
    // - note the testmode loop will cycle the relay enables as well
    uint8_t relayMask;
    relayMask = io22d08.RELAY1+io22d08.RELAY2+io22d08.RELAY3+io22d08.RELAY4;
    buttonTimers[0].setTimeout(relayMask, 4);
    relayMask = io22d08.RELAY5+io22d08.RELAY6+io22d08.RELAY7+io22d08.RELAY8;
    buttonTimers[1].setTimeout(relayMask, 8);
    // start the timers, once (then handover to "manual" control via buttons)
    for (auto & t : buttonTimers) t.start();

    loop_fn = loop_testmode;
    return;
  }

  // set some demo timer values
  Serial.print(F("set button timers: "));
  buttonTimers[0].setTimeout(io22d08.RELAY1, 4);
  buttonTimers[1].setTimeout(io22d08.RELAY2, 6);
  buttonTimers[2].setTimeout(io22d08.RELAY3, 8);
  buttonTimers[3].setTimeout(io22d08.RELAY4, 10);
  buttonTimers[4].setTimeout(io22d08.RELAY5, 12);
  buttonTimers[5].setTimeout(io22d08.RELAY6, 16);
  buttonTimers[6].setTimeout(io22d08.RELAY7, 20);
  buttonTimers[7].setTimeout(io22d08.RELAY8, 30);
  Serial.println(F("✔️"));

  Serial.print(F("set input timers: "));
  for (size_t i = 0; i < io22d08.numInputs; i++)
  {
    inputTimers[i].setTimeout(io22d08.relayNumToMask(i+1), 3);
  }
  Serial.println(F("✔️"));
}


// testmode
// - check display is working by cycling through some display values
// - also enable/disable the relays to test the relay enable output

// don't left-pad these constants (i.e. not octal)
const uint16_t testmodeNumbers[] = {0, 1234, 8, 80, 800, 8000, 8888};
const size_t numtestmodeNumbers (sizeof(testmodeNumbers)/sizeof(testmodeNumbers[0]));

void loop_testmode()
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis;
  currentMillis = millis();

  if (currentMillis - previousMillis > 1000)
  {
    previousMillis = currentMillis;
    uint8_t i = (currentMillis/1000UL) % numtestmodeNumbers;
    io22d08.displayNumber(testmodeNumbers[i]);
    io22d08.setColon(i%2 ? false: true);

    // disable the relays for the 0'th display period
    if (i)
    {
      io22d08.enableRelays();
    }
    else
    {
      io22d08.disableRelays();
    }
  }

  for (auto & b : buttons) b.check();
  for (auto & i : inputs) i.check();
  for (auto & t : buttonTimers) t.tick();
  for (auto & t : inputTimers) t.tick();

  // unlike the display, the relay outputs are not multiplexed and don't need
  // continual refreshing; however we want to show the display as well
  io22d08.refreshDisplayAndRelays();
}


uint16_t _getMinTimeRemaining(uint16_t mtr, RelayTimer* timers, size_t nTimers)
{
  for (size_t n = 0; n < nTimers; n++)
  {
    uint16_t tr = timers[n].getTimeRemaining();
    if (tr < mtr) mtr = tr;
  }
  return mtr;
}

void loop_main()
{
  static unsigned long previousMillis[] = {0, 0};
  #define PREVIOUS_MILLIS_COLON 0     // colon update
  #define PREVIOUS_MILLIS_DISPLAY 1   // display update
  unsigned long currentMillis;
  currentMillis = millis();

  // colon flash is asynchronous to timer and display updates
  if (currentMillis - previousMillis[PREVIOUS_MILLIS_COLON] > 500)
  {
    previousMillis[PREVIOUS_MILLIS_COLON] = currentMillis;
    io22d08.toggleColon();
  }

  if (currentMillis - previousMillis[PREVIOUS_MILLIS_DISPLAY] > 100)
  {
    previousMillis[PREVIOUS_MILLIS_DISPLAY] = currentMillis;
    // display the (active) timer that is expiring next (i.e. lowest delta)
    io22d08.displayMessage(io22d08.MESSAGE_BLANK);  // clear the display
    uint16_t mtr = UINT16_MAX;
    mtr = _getMinTimeRemaining(mtr, buttonTimers, numButtonTimers);
    mtr = _getMinTimeRemaining(mtr, inputTimers, numInputTimers);
    if (mtr != UINT16_MAX)
    {
      io22d08.displayNumber(mtr/1000UL + 1); // +1: crude ceil()
    }
  }

  for (auto & b : buttons) b.check();
  for (auto & i : inputs) i.check();
  for (auto & t : buttonTimers) t.tick();
  for (auto & t : inputTimers) t.tick();

  io22d08.refreshDisplayAndRelays();
}

void loop() {
  loop_fn();
}
