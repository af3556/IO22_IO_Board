/* examples/IO22D08Timers/IO22D08Timers.ino

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
- the external inputs IN1-IN8 switch the corresponding relay and have AceButton
  behaviours: momentary on/off on short-pulse (press/release), latch on for a
  long-pulse (long-press), and start a timer on double-pulse (double-click)
- in all cases, the display shows the time remaining on the timer that is next
  to expire; e.g. if three timers a, b, c are running with 16, 3 and 5 seconds
  remaining then the display will show b's time remaining, then switch to
  displaying c's, then finally a's
  - when no timers are running, the display is blanked except for the flashing
    colon (toggled every 0.5s)
- the main loop runs at about 3.3ms (300Hz)
*/

#include "IO22_IO_Board.h"

#include <AceButton.h>
using namespace ace_button;

IO22D08 io22d08;  // create an instance of the relay board

// AceButton is used to handle both the buttons K1-K4 and the inputs IN2-IN8
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

  public:
    RelayTimer() {}

    void setTimeout(uint8_t rm, uint16_t t)
    {
      _relayMask = rm;
      _timeout = t * 1000UL;
    }

    void start()
    {
      Serial.print(F("T("));
      Serial.print(_relayMask);
      Serial.println(F("):ON"));
      _previousMillis = millis();
      _isActive = true;
      // if _relayMask is 0, will be a no-op
      io22d08.relaySet(_relayMask, io22d08.RELAY_ON);
    }

    void stop()
    {
      Serial.print(F("T("));
      Serial.print(_relayMask);
      Serial.println(F("):OFF"));
      _isActive = false;
      io22d08.relaySet(_relayMask, io22d08.RELAY_OFF);
    }

    void tick()
    {
      if (_isActive)
        if (millis() - _previousMillis > _timeout) stop();
    }

    uint16_t getTimeRemaining()
    {
      if (_isActive)
        return _timeout - (millis() - _previousMillis);
      return -1;  // maxtime
    }
};

const size_t numRelayTimers = io22d08.numRelays;  // for this demo, as many timers as relays
RelayTimer relayTimers[numRelayTimers];

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
      // buttonIDs are not necessarily aligned with timer indices
      relayTimers[(buttonId-1)%numRelayTimers].start();  // button N => timer N-1
      break;
  }
}

// handler for the digital inputs
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
      io22d08.relaySetN(inputNum, !io22d08.relayIsOn(inputNum));
      break;
    case AceButton::kEventDoubleClicked:
      // buttonIDs are not necessarily aligned with timer indices
      relayTimers[(inputNum-1)%numRelayTimers].start();  // input N => timer N-1
      break;
  }
}

void setup() {
  Serial.begin(9600);
  io22d08.begin();
  io22d08.displayMessage(io22d08.MESSAGE_BLANK);  // clear the display
  io22d08.enableRelays();

  Serial.println(F("\nIO22D08"));

  Serial.print(F("init buttons: K1-K4 "));
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

  Serial.print(F("init digital inputs: IN1-IN8 "));
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
    // button numbers/IDs count from 1, and are normally-high (active low)
    inputs[i].init(&inputConfig, io22d08.inputPins[i], HIGH, i+1);
    Serial.print(i+1);
  }
  Serial.println(F("✔️"));

  // set some demo timer values
  // update numRelayTimers to reflect the number of timers being used
  Serial.print(F("set relay timers: "));
  size_t i = 0;
  relayTimers[i++].setTimeout(io22d08.RELAY1, 4);
  relayTimers[i++].setTimeout(io22d08.RELAY2, 6);
  relayTimers[i++].setTimeout(io22d08.RELAY3, 8);
  relayTimers[i++].setTimeout(io22d08.RELAY4, 10);
  relayTimers[i++].setTimeout(io22d08.RELAY5, 12);
  relayTimers[i++].setTimeout(io22d08.RELAY6, 16);
  relayTimers[i++].setTimeout(io22d08.RELAY7, 20);
  relayTimers[i++].setTimeout(io22d08.RELAY8, 30);
  Serial.print(i);
  Serial.print("/");
  Serial.print(numRelayTimers);
  Serial.println(F("✔️"));

  pinMode(A4, OUTPUT);  // loop() interval measurement
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

void loop()
{
  static unsigned long previousMillis[] = {0, 0};
  const uint8_t PREVIOUS_MILLIS_COLON = 0;    // colon update
  const uint8_t PREVIOUS_MILLIS_DISPLAY = 1;  // display update
  unsigned long currentMillis;
  currentMillis = millis();

  // colon flash is asynchronous to timer and display updates
  if (currentMillis - previousMillis[PREVIOUS_MILLIS_COLON] > 500)
  {
    previousMillis[PREVIOUS_MILLIS_COLON] = currentMillis;
    io22d08.toggleColon();
  }

  // update display
  if (currentMillis - previousMillis[PREVIOUS_MILLIS_DISPLAY] > 250)
  {
    previousMillis[PREVIOUS_MILLIS_DISPLAY] = currentMillis;
    // display the (active) timer that is expiring next (i.e. lowest delta)
    io22d08.displayMessage(io22d08.MESSAGE_BLANK);  // clear the display
    uint16_t mtr = UINT16_MAX;
    mtr = _getMinTimeRemaining(mtr, relayTimers, numRelayTimers);
    if (mtr != UINT16_MAX)
    {
      io22d08.displayNumber(mtr/1000UL + 1); // +1: crude ceil()
    }
  }

  for (auto & b : buttons) b.check();
  for (auto & i : inputs) i.check();
  for (auto & t : relayTimers) t.tick();

  io22d08.refreshDisplayAndRelays();
  digitalWrite(A4, digitalRead(A4)==HIGH?LOW:HIGH); // to measure loop time
}
