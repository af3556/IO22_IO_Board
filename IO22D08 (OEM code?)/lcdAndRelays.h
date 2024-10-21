byte dat = 0;
bool dot = false;
byte com_num = 0;
byte relay_port = 0;
byte dat_buf[4] = {41, 41, 41, 41};
bool dot_buf[4] = {0, 0, 0, 0};
//                    0    1     2     3     4     5     6     7     8     9    10    11    12    13    14    15    16    17    18    19    20    21    22    23    24    25    26    27    28    29    30    31    32    33    34    35    36    37    38    39    40    41      42     43    44      45
//                    0    1     2     3     4     5     6     7     8     9     A     b     C     d     E     F     G     H     i     j     k     l     m     n     o     p     q     r     S     t     u     v     w     x      y    z     [     /     ]     -     =   space , custom1 cus2  cus3   cus4
byte  TUBE_SEG[] = {0xc0, 0xf9, 0xa4, 0xb0, 0x99, 0x92, 0x82, 0xf8, 0x80, 0x90, 0x88, 0x83, 0xc6, 0xa1, 0x86, 0x8e, 0x90, 0x89, 0xFB, 0xF1, 0x8f, 0xC7, 0xA9, 0xab, 0xa3, 0x8c, 0x98, 0xaf, 0x12, 0x87, 0xe3, 0xc1, 0x81, 0x89, 0x91, 0xa4, 0xC6, 0xAD, 0xF0, 0xbf, 0xb7, 0xff , 0xff , 0xff , 0xff , 0xff }; //Common anode Digital Tube Character Gallery

byte TUBE_NUM[8] = {0xfe, 0xff, 0xfd, 0xff, 0xfb, 0xff, 0xf7, 0xff}; //Tuble bit number

void TubeDisplayOneBit()
{
  byte tube_dat, bit_num;
  byte display_l, display_h, relay_dat;

  tube_dat = TUBE_SEG[dat]; //Common Cathode Digital Tube,bit negated
  if (dot)  tube_dat &= 127;
  bit_num = ~TUBE_NUM[com_num];

  display_l  = ((tube_dat & 0x10) >> 3); //Q4   <-D1 -3
  display_l |= ((bit_num & 0x01) << 2); //DIGI0<-D2 +2
  display_l |= ((tube_dat & 0x08) >> 0); //Q3   <-D3 0
  display_l |= ((tube_dat & 0x01) << 4); //Q0   <-D4 -4
  display_l |= ((tube_dat & 0x80) >> 2); //Q7   <-D5 -2
  display_l |= ((tube_dat & 0x20) << 1); //Q5   <-D6 1
  display_l |= ((tube_dat & 0x04) << 5); //Q2   <-D7 5

  display_h  = ((bit_num & 0x02) >> 0); //DIGI1<-D1 0
  display_h |= ((bit_num & 0x04) >> 0); //DIGI2<-D2 0
  display_h |= ((tube_dat  & 0x40) >> 3); //Q6   <-D3 -3
  display_h |= ((tube_dat  & 0x02) << 3); //Q1   <-D4 3
  display_h |= ((bit_num & 0x08) << 2); //DIGI3<-D5 2

  relay_dat = ((relay_port & 0x7f) << 1);
  relay_dat = relay_dat | ((relay_port & 0x80) >> 7);

  //ground latchPin and hold low for as long as you are transmitting
  digitalWrite(LATCH_PIN, LOW);

  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, display_h);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, display_l);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, relay_dat);

  //return the latch pin high to signal chip that it
  //no longer needs to listen for information
  digitalWrite(LATCH_PIN, HIGH);

}



void TubeDisplay4Bit(void)
{
  if (com_num < 7) com_num ++; else com_num = 0;
  dat = dat_buf[com_num / 2];
  dot = dot_buf[com_num / 2];
  // dat = dat_buf[com_num];
  TubeDisplayOneBit();
}

void clearLCD()
{
  for (byte b = 0; b < 4; b++) {
    dat_buf[b] = 41;
    dot_buf[b] = 0;
  }
}


void setLCDdigit(byte segment, int value, bool dots = false)
{
  dat_buf[segment] = (byte)value;
  dot_buf[segment] = dots;
}


void setLCDdigit(byte segment, char ch_value, bool dots = false)
{
  byte b = (byte)ch_value;
  if (b >= 48 && b <= 57) b = b - 48; // 0-9 digits
  else if (b >= 65 && b <= 93) b = b - 55; // A-Z [ / ] chars
  else if (b >= 97 && b <= 122) b = b - 87; // a-z  chars
  else if (b == 45) b = 39; // minus
  else if (b == 61) b = 40; // =
  else if (b == ' ') b = 41; // space

  dat_buf[segment] = b;
  dot_buf[segment] = dots;
}

void setLCDtext(char c[] , bool dots = false)
{

  byte len = strlen(c);
  byte start = 0;
  if (len < 4) start = 4 - len;
  for (byte b = 0; b < start; b++) setLCDdigit(b, 41, dots);
  for (byte b = start; b < 4; b++) setLCDdigit(b, c[b - start], dots);

}

void setLCDTime(byte hour, byte minute, bool dots = true)
{
  byte b1 = hour / 10;
  byte b2 = hour % 10;
  byte b3 = minute / 10;
  byte b4 = minute % 10;
  setLCDdigit(0, (byte)b1);
  setLCDdigit(1, (byte)b2);
  setLCDdigit(2, (byte)b3);
  setLCDdigit(3, (byte)b4);
  dot_buf[1] = dots;
}


void setLCDbyInt(int i)
{
  bool minus = false;
  if (i < 0) {
    minus = true;
    i = -i;
  }
  int b1 = i / 1000;
  int b2 = (i - (b1 * 1000)) / 100;
  int b3 = (i - ((b1 * 1000) + b2 * 100)) / 10;
  int b4 = i % 10;
  if (b1 < 0) b1 = 0;
  if (b2 < 0) b2 = 0;
  if (b3 < 0) b3 = 0;
  if (minus)  b1 = 39;
  if (i >= 0 && i < 10) b3 = 41;
  if (i >= 0 && i < 100) b2 = 41;
  if (!minus && i >= 0 && i < 1000) b1 = 41;
  setLCDdigit(0, (byte)b1);
  setLCDdigit(1, (byte)b2);
  setLCDdigit(2, (byte)b3);
  setLCDdigit(3, (byte)b4);
}


void setCustomChar(byte segment, byte mask, bool dots = false)
{
  if (segment > 3) segment = 3;
  TUBE_SEG[segment + 42] = mask;
  setLCDdigit(segment, segment + 42, dots);
}


/*---Segment Display Screen----
  --A--
  F---B
  --G--
  E---C
  --D--
   __  __   __  __
  |__||__|.|__||__|
  |__||__|'|__||__|

  mask= ~( 0b (DOT)GFEDCBA);
  ----------------------*/

byte setCustomChar(byte segment, byte a, byte b, byte c, byte d, byte e, byte f, byte g , bool dots = false)
{
  byte mask = (a & 1)  | ((b & 1) << 1) | ((c & 1) << 2) | ((d & 1) << 3) | ((e & 1) << 4) | ((f & 1) << 5) | ((g & 1) << 6);
  mask = ~mask;
  setCustomChar(segment, mask, dots);
  return mask;
}

void relaysFunct() {
  if (!USE_LCD)
  {
    byte relay_dat = ((relay_port & 0x7f) << 1);
    relay_dat = relay_dat | ((relay_port & 0x80) >> 7);
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, 255);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, 255);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, relay_dat);

    //return the latch pin high to signal chip that it
    //no longer needs to listen for information
    digitalWrite(LATCH_PIN, HIGH);
  }
}

void setRelay(byte relayNo)
{
  relayNo--;
  bitSet(relay_port, relayNo);
  relaysFunct();
}


void resetRelay(byte relayNo)
{
  relayNo--;
  bitClear(relay_port, relayNo);
  relaysFunct();
}

void setRelayAndOffRest(byte relayNo)
{

  relay_port = 0;
  setRelay(relayNo);
}



byte getRelay(byte relayNo)
{
  relayNo--;
  return bitRead(relay_port, relayNo);
}


void clearRelays()
{
  relay_port = 0;
  relaysFunct();
}
