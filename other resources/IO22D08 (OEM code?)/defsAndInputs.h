

#define INPUTS 8
#define KEYS 4
#define NOISE_TIME 50


#define IN1 2
#define IN2 3
#define IN3 4
#define IN4 5
#define IN5 6

#define K1 7
#define K2 8
#define K3 9


#ifndef NANO_AS_MINI
// config for mini
#define IN6 A0
#define IN7 12
#define IN8 11

#define K4 10

#define CLOCK_PIN A3
#define LATCH_PIN A2
#define OE_595 A1
#define DATA_PIN 13

#else
// config for nano
#define IN6 A4
#define IN7 A2
#define IN8 A1

#define K4 A0

#define CLOCK_PIN 10
#define LATCH_PIN 11
#define OE_595 A5
#define DATA_PIN A3

#endif

#define IO_LOW 0
#define IO_HIGH 1
#define IO_RISING 2
#define IO_FALLING 3

byte inputs[INPUTS] = {IN1, IN2, IN3, IN4, IN5, IN6, IN7, IN8};
byte keys[KEYS] = {K1, K2, K3, K4};

byte inValues[INPUTS] = {0, 0, 0, 0, 0, 0, 0, 0};
byte oldInValue[INPUTS] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned int inTime[INPUTS] = {0, 0, 0, 0, 0, 0, 0, 0};

byte keysValues[KEYS] = {0, 0, 0, 0};
byte oldKeyValue[KEYS] = {0, 0, 0, 0};
unsigned int keyTime[KEYS] = {0, 0, 0, 0};


void initIO()
{
  for (byte b = 0; b < INPUTS; b++)
    pinMode(inputs[b], INPUT);

  for (byte b = 0; b < KEYS; b++)
    pinMode(keys[b], INPUT_PULLUP);

  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(OE_595, OUTPUT);

}


// read INPUTS negative it - pull to ground is 1. free is 0
void readINPUTS()
{
  for (byte b = 0; b < INPUTS; b++)
  {
    byte in = digitalRead(inputs[b]);

    if (in != oldInValue[b]) inTime[b] = millis();
    byte IN = !in;
    if ( millis() - inTime[b] > NOISE_TIME)
    {

      if (IN != inValues[b] && inValues[b] < 2)
      {
        if (IN == 1) inValues[b] = IO_RISING; else  inValues[b] = IO_FALLING;
      }
      else {
        inValues[b] = IN;
        inTime[b] = 0;
      }
    }

    oldInValue[b] = in;
  }

}


// read keys with noise remove  pull is 1 free is 0
void readKEYS()
{
  for (byte b = 0; b < KEYS; b++)
  {
    byte key = digitalRead(keys[b]);
    if (key != oldKeyValue[b]) keyTime[b] = millis();
    byte KEY = !key;
    if ( millis() - keyTime[b] > NOISE_TIME)
    {
      if (KEY != keysValues[b] && keysValues[b] < 2)
      {
        if (KEY == 1) keysValues[b] = IO_RISING; else  keysValues[b] = IO_FALLING;
      }
      else {
        keysValues[b] = KEY;
        keyTime[b] = 0;
      }
    }
    oldKeyValue[b] = key;
  }

}
