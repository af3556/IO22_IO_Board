/* examples/IO22D08Timers/IO22D08TimersAndFrequencySwitch.ino

  The IO22D08 is an I/O board for an Arduino Pro Mini; it provides:
  - 8 x relay outputs (10A NO/NC outputs) + LED per channel
  - 8 x optically isolated inputs
  - 4 x pushbuttons
  - 4 x 9-segment LED display (88:88), handy for time/state info

  This example program builds on IO22D08Timers.ino and replaces one of the
digital inputs with a frequency signal; switching RELAY1 based on that input
frequency.
- the external input IN1 switches the R1 relay on/off at an input frequency
  of 20Hz (with some hysteresis)
- inputs IN1 and IN2 are connected to the two external interrupt pins on the
  Mini and are readily adapted to monitoring a frequency input
- the buttons K1-K4, other inputs IN2-IN8, and display are the same as for
  IO22D08Timers

  In addition, a "test mode" is provided that runs an alternate loop() if the
K1 button is held down in setup() when powering on. The test mode cycles various
values through the display and toggles the relay enable control.

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


// frequency controlled switch for IN1 and/or IN2
//  - IN1,2 inputs are connected to the two external interrupt pins on the Mini
//    and thus can readily monitor a frequency input
//    - the other inputs could as well, via a pin change interrupt, but that's
//      a lot more work (and will be slower)
//
// - can measure frequency either by
//    1) counting the number of pulses in a given period
//      - requires longer sample periods as the frequency decreases (or
//        conversely loses resolution)
//    2) measuring the period (e.g. time between rising edges)
//      - loses resolution as the frequency increases
//      - lower noise-immunity / greater impact from spurious pulses
//
// - for this example the expected input frequency range of ~1-120Hz
//   (period: 100-8ms), with a desired 500ms update rate
//   - either frequency measurement approach is viable / should work
//   - at the lower frequency end the resolution of the pulse-counting approach
//     will be relatively poor (i.e. only ~5 pulses per 500ms, so a 20% margin
//     of error) so go with the period measuring approach
//   - with a 500ms update rate there's budget to apply a filter to reduce noise
//     on the frequency measurement; a simple moving average filter works well
//    - https://en.wikipedia.org/wiki/Exponential_smoothing
//    - expressed as the following, where u is the new sample, and x is the
//      smoothed average:
//      x = (1-alpha).x + (alpha).u
//    - with alpha = 1/(2^n), this expression can be implemented via efficient
//      add/subtract and bit shifts (i.e. 1/2^n = >>n)
//      - e.g. https://electronics.stackexchange.com/a/34426/264328
//      - https://forum.arduino.cc/t/implementing-exponential-moving-average-filter/428637/11
//    - at very low frequencies and modest update rates sampling starts
//      becoming a problem (i.e. when the incoming pulse train is at 2Hz, you
//      can't get an update faster than 2Hz); one approach would be to use a
//      multiplier PLL to construct a finer-grained representation of the input
//
// - with the interrupt-based approach to measuring the input signal, if the
//   input signal is disconnected then the ISR won't be called and the frequency
//   measurement will stop being updated, it'll stay 'stuck' at the last reading
//    - if this is a problem for the application, it's easy to deal with via a
//      periodic check outside of the ISR: if the last update was longer than
//      some expected period, then zero it; this logic should also be reflected
//      in the ISR averaging code as well - see PERIOD_MAX below
//    - note as the input signal slows right down to DC, the above logic starts
//      interfering with the real measurements (i.e. Q: when does "low
//      frequency" become "stopped"? A: beyond PERIOD_MAX)
// - some hysteresis is required to prevent chatter

class FreqSwitch
{
  public:
    enum FSState {FS_STOPPED, FS_LOW, FS_HIGH}; // Arduino LOW/HIGH be stompin'

  private:
    // all values in this class are periods (intervals), in us, not frequencies

    volatile unsigned long _previousMicros;

    volatile unsigned long _period;   // last interval
    volatile unsigned long _stopped;  // longer than this == stopped
    volatile unsigned long _lower, _upper; // lower, upper hysteresis thresholds

    const size_t _filterN = 2; // filter alpha = 1/1^n

    FSState _state = FS_STOPPED; // initial state

  public:
    // anything longer than this is considered DC (stopped); making this too
    // long will slow the filter response once the signal starts up again
    const unsigned long PERIOD_MAX = 500*1000UL; // 500ms = 2Hz

    FreqSwitch() {}

    void setThresholds(unsigned long stopped, unsigned long lower, unsigned long upper)
    {
      _stopped = stopped;
      _lower = lower;
      _upper = upper;
    }

    unsigned long getPeriod() { return _period; }
    FSState getState() { return _state; }

    void sample() // this is called by the ISR; keep it short
    {
      // as the signal approaches DC this function will be called at increasing
      // intervals; if the signal is removed it won't get called at all; and thus
      // the period won't get updated
      static unsigned long currentMicros;
      static unsigned long periodN;  // period left-shifted (for averaging)
      currentMicros = micros();
      // have to do the filtering here to ensure we catch 'em all
      // - see above re. this moving average calculation
      periodN += (currentMicros - _previousMicros) - _period;
      _period = periodN >> _filterN;
      _previousMicros = currentMicros;
    }

    int tick()
    {
      // high period == low speed and vice-versa; a high enough period == stopped

      // LOW-HIGH hysteresis:
      // transition state => FS_HIGH when period is now lower than lower threshold
      // transition state => FS_LOW when period is now higher than upper threshold
      // no change in between
      switch (_state)
      {
        case FS_STOPPED:
          if (_period < _stopped) _state = _period > _upper ? FS_LOW : FS_HIGH;
          break;
        case FS_LOW:
          if (_period < _lower) _state = FS_HIGH;
          if (_period > _stopped) _state = FS_STOPPED;
          break;
        case FS_HIGH:
          if (_period > _upper) _state = FS_LOW;
          if (_period > _stopped) _state = FS_STOPPED;
          break;
      }

      // if the ISR hasn't been called "recently" (longer than the _stopped period),
      // force to stopped state
      // - this overrides the above FSM
      if (micros() - _previousMicros > _stopped)
      {
        _state = FS_STOPPED;
        // it might be tempting to force _period to the max here to "help" the
        // sample() filter, but that will be problematic as this function and the
        // ISR are totally asynchronous, manipulating any of the filter components
        // here will give rise to aliasing effects
      }

      return _state;
  }
};

//  - it's easiest to use separate ISRs for each external interrupt
//    - an ISR has limitations on its context (e.g. if part of the class, has to
//      be a static member function: one and only one); if there is only one ISR
//      shared between multiple instances you then have to determine which pin
//      triggered the interrupt; you can try and mess around with reading and
//      keeping track of the states of each pin to figure that out, but there
//      are complexities including race conditions with that approach
FreqSwitch freqSwitch;
void _isr_freq() { freqSwitch.sample(); }

void (*loop_fn)() = loop_main;  // allow switching between main and testmode

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

  Serial.print(F("init frequency input: IN1 (RELAY1) 15,20Hz "));
  pinMode(io22d08.inputPins[0], INPUT);
  attachInterrupt(digitalPinToInterrupt(io22d08.inputPins[0]), _isr_freq, RISING);
  // thresholds are periods in ms, so inverted:
  // - 2Hz = 500ms, 20Hz = 50ms, 15Hz = 66ms
  freqSwitch.setThresholds(500e3, 50e3, 66e3);
  Serial.println(F("✔️"));

  Serial.print(F("init digital inputs: IN2-IN8 "));
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
  for (size_t i = 1; i < io22d08.numInputs; i++)  // start at 1: excl. IN1
  {
    // button numbers/IDs count from 1, and are normally-high (active low)
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
    relayTimers[0].setTimeout(relayMask, 4);
    relayMask = io22d08.RELAY5+io22d08.RELAY6+io22d08.RELAY7+io22d08.RELAY8;
    relayTimers[1].setTimeout(relayMask, 8);
    // start the timers, once (then handover to "manual" control via buttons)
    for (auto & t : relayTimers) t.start();

    loop_fn = loop_testmode;
    return;
  }

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


// testmode
// - check display is working by cycling through some display values
// - also enable/disable the relays to test the relay enable output

// don't left-pad these constants (i.e. not octal)
const uint16_t testmodeNumbers[] = {0, 1234, 8, 80, 800, 8000, 8888};
const size_t numTestmodeNumbers (sizeof(testmodeNumbers)/sizeof(testmodeNumbers[0]));

void loop_testmode()
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis;
  currentMillis = millis();

  if (currentMillis - previousMillis > 1000)
  {
    previousMillis = currentMillis;
    uint8_t i = (currentMillis/1000UL) % numTestmodeNumbers;
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
  for (auto & t : relayTimers) t.tick();

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
  static unsigned long previousMillis[] = {0, 0, 0};
  const uint8_t PREVIOUS_MILLIS_COLON = 0;    // colon update
  const uint8_t PREVIOUS_MILLIS_DISPLAY = 1;  // display update
  const uint8_t PREVIOUS_MILLIS_FREQ = 2;     // freq switch update
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

  // process frequency switch trigger
  if (currentMillis - previousMillis[PREVIOUS_MILLIS_FREQ] > 500)
  {
    previousMillis[PREVIOUS_MILLIS_FREQ] = currentMillis;

    // - this tick could be executed every cycle but we don't need that fast a
    //   response
    // - freq switch state:
    //    - LOW: input signal period < low threshold (i.e. f = high)
    //    - HIGH: input signal period > high threshold (i.e. f = low)
    // - relay state (note; this is a binary mask, not a simple two-state var)
    //    - io22d08.RELAY_OFF: off
    //    - !io22d08.RELAY_OFF: on

    // report current frequency measurement, but only when it changes
    static unsigned long previousPeriod;
    long deltaPeriod;
    deltaPeriod = freqSwitch.getPeriod() - previousPeriod;
    previousPeriod = freqSwitch.getPeriod();
    if (abs(deltaPeriod) > 100)
    {
      Serial.print(F("f="));
      Serial.print(1e6/previousPeriod);
      Serial.print(F("Hz("));
      Serial.print(previousPeriod/1000.0);
      Serial.println(F("ms)"));
    }

    int freqSwitchState = freqSwitch.tick();
    int relayState = io22d08.relayGet(io22d08.RELAY1);
    // in this example, want the relay on at low frequencies, or when stopped
    // if the relay is on and needs to be off, turn it off; ditto the inverse
    switch (freqSwitchState)
    {
      case freqSwitch.FS_STOPPED:
        [[fallthrough]];
      case freqSwitch.FS_LOW:
        if (relayState == io22d08.RELAY_OFF)
        {
          // should be on but is off, turn on
          Serial.println(F("F1<:ON"));
          io22d08.relaySet(io22d08.RELAY1, io22d08.RELAY_ON);
        }
        break;

      case freqSwitch.FS_HIGH:
        if (relayState != io22d08.RELAY_OFF)
        {
          // should be off but is on, turn off
          Serial.println(F("F1>:OFF"));
          io22d08.relaySet(io22d08.RELAY1, io22d08.RELAY_OFF);
        }
        break;
    }
  }

  for (auto & b : buttons) b.check();
  for (auto & i : inputs) i.check();
  for (auto & t : relayTimers) t.tick();

  io22d08.refreshDisplayAndRelays();
}

void loop() {
  loop_fn();
  digitalWrite(A4, digitalRead(A4)==HIGH?LOW:HIGH); // to measure loop time
}
