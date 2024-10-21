# About The Display

The 4 digit 7-segment display is connected to two 8-bit shift registers (SR). A
third shift register is used to control the relays. The SRs are connected
serially with the 24-bits of data shifting through as:

  U5 (relays) > U3 (display) > U4 (display)

A 7-segment display can show the digits 0-9 as well as a handful of other
characters; 4 x 7-segment digits can display more than just numbers, e.g. "ON",
"OFF", "Err".

Going from characters in code to displaying them involves three main parts:

1) mapping the shift register outputs to display pins (segments+digit commons)
    - this is largely determined by the board layout (essentially arbitrary)

2) defining a "font": mapping characters to display segments, e.g.
  the character "3" = segments A,B,C,D,G
    - this can be (arbitrarily) represented as a 8-bit value in the bit
        order `DP A B C D E F G`  (DP = decimal point for that digit, if present)
    - the MAX7219 has a 'font' ready to go (e.g. https://github.com/JemRF/max7219)
    - DP is omitted from the character fonts as it's not a persistent part of most
        characters
    - for the PLC board: the display does not actually have DPs for each digit,
        rather, there are DPs 2 and 3 are the lower and upper colon LEDs
        - DP1 and 4 appear to be not connected

3) the 4-digit display is multiplexed: segment cathode connections are shared
  across all digits, with a separate common anode for each digit
    - a "solid" 4-digit display results when each digit is individually updated
        quickly enough to fool the eye (~<50ms refresh?, so ~10ms/digit)

Ultimately, a specific bit sequence needs to be loaded to the shift registers
then latched to drive a single digit/character, repeated for all digits.

In the following, we pre-calculate the values of the combination of the
shift-register-to-display-pin and character-font definitions such that writing
the resulting "magic number" to the pair of display shift registers will result
in the desired character being displayed in the desired position.

This approach allows defining a relatively compact set of symbols for each
character which can then be combined via a single bit operation with the desired
digit select bit (and another for the decimal point as desired) to generate a
complete bit pattern for each digit position.

The alternative is to do a bunch of bit operations on a more obviously defined
character-segment mapping (font); given the font will never change it's more
efficient in time and space to take the pre-calculated approach (on space
efficiency: the pre-calculated shift register data is 16 bits per character vs.
8 for the font, so technically less space efficient for data but saves on code).
Putting any thought into saving a few bytes on a micro that has oodles of spare
memory is of course the very epitome of over-optimisation but it beats doing
sudoku.

## Common Anode / Common Cathode

Some doc (code comments) for this board incorrectly state the display is common
cathode; it's actually common anode. Interestingly it's a moot point as the
entirety of the display (every pin) is driven via the shift registers and thus
from software: the only difference would be the values of the various constants.

## 1. Mapping the Shift Register Outputs to Display Pins

The mapping from relays and display pins to shift register outputs is as follows
(from the schematic):

```text
Relays         4-digit, 7-segment LED display
R1 - U5:Q1      c - U3:Q7       x - U4:Q7
R2 - U5:Q2      f - U3:Q6       x - U4:Q6
R3 - U5:Q3     DP - U3:Q5      K4 - U4:Q5
R4 - U5:Q4      a - U3:Q4       b - U4:Q4
R5 - U5:Q5      d - U3:Q3       g - U4:Q3
R6 - U5:Q6     K1 - U3:Q2      K3 - U4:Q2
R7 - U5:Q7      e - U3:Q1      K2 - U4:Q1
R8 - U5:Q0      x - U3:Q0       x - U4:Q0

- x = don't care (not connected)
```

Transforming that into a 24-bit word:

```text
|     _relayBuffer        |                    digitBuffer                    |
|     Relays (U5)         |      Display (U3)       |      Display (U4)       |
|  7  6  5  4  3  2  1  0 |  7  6  5  4  3  2  1  0 |  7  6  5  4  3  2  1  0 |
| R7 R6 R5 R4 R3 R2 R1 R8 |  c  f DP  a  d K1  e  x    x  x K4  c  g K3 K2  x |
```

The ordering here reflects the physical arrangement of the shift registers,
which is U5 (relays) > U3 (display) > U4 (display) (i.e. not U5>U4>U3).

The relays are physically labelled as 1 through 8; coils (and corresponding
LEDs) are driven from the 12V supply via the ULN2803. The ULN2803 is in turn
driven by the shift register U5. The connections are as noted in the above
`_relayBuffer` map (i.e. Q0 = R8, Q1 = R1 ... Q7 = R7). The interface provided
in `relaySet()` and `relaySetN()` accounts for this sequencing.

Aside when powering from 12V: the serial programmer's Vcc (5V) should not be
connected, otherwise the IO22D08's 5V regulator will fight with the connected
USB port.

### Worked Example

Relays 1, 3, 4 on, and a digit '3' displayed in position 1

- i.e. segments (a, b, c, d, g), K1 enabled
- logic levels: "H" = high/enabled/active/on, "L" = low/disabled/inactive/off

The relays are essentially active-high; H = 1 / relay-on.

Focusing only on the display for the moment: a common anode display requires the
segments to be driven active-low and the digit common (K) pin driven
active-high. Thus, the segment bits presented by the SR to the display have to
inverted to make them active-low.

To flip only the segment bits (incl. DP), apply an XOR mask as below (assigning
the don't cares to 0)

```text
|     _relayBuffer        |                    digitBuffer                    |
|     Relays (U5)         |      Display (U3)       |      Display (U4)       |
|  7  6  5  4  3  2  1  0 |  7  6  5  4  3  2  1  0 |  7  6  5  4  3  2  1  0 |
| R7 R6 R5 R4 R3 R2 R1 R8 |  c  f DP  a  d K1  e  x    x  x K4  c  g K3 K2  x |
|                         |  H  L  L  H  H  H  L  x    x  x  L  H  H  L  L  x |
                          |  1  1  1  1  1  0  1  0    0  0  0  1  1  0  0  0 |
(= 0x__FA18)
```

If the display were common cathode, the K1-K4 bits would need to be flipped;

```text
                           | 0  0  0  0  0  1  0  0    0  0  1  0  0  1  1  0 |
(= 0x__0426)
```

Continuing the example (digit '3' displayed in position 1):

```text
|     _relayBuffer        |                    digitBuffer                    |
|     Relays (U5)         |      Display (U3)       |      Display (U4)       |
|  7  6  5  4  3  2  1  0 |  7  6  5  4  3  2  1  0 |  7  6  5  4  3  2  1  0 |
| R7 R6 R5 R4 R3 R2 R1 R8 |  c  f DP  a  d K1  e  x    x  x K4  c  g K3 K2  x |
|  L  L  L  H  H  L  H  L |  H  L  L  H  H  H  L  x    x  x  L  H  H  L  L  x |
- assigning the don't cares to 0:
|  0  0  0  1  1  0  1  0 |  1  0  0  1  1  1  0  0    0  0  0  1  1  0  0  0 |
                            XOR with the segment active-low mask:
                          |  1  1  1  1  1  0  1  0    0  0  0  1  1  0  0  0 |
=
|  0  0  0  1  1  0  1  0 |  0  1  1  0  0  1  1  0    0  0  0  0  0  0  0  0 |
(= 0x1A6600)
```

Writing this 24-bit value out to the shift registers via the data pin will
result in relays 1, 3, 4 on, and a digit '3' displayed in position 1.
(Of course, it's not _quite_ that simple: the 74HC595 shift register is
most significant bit (MSb) first - Q7-Q0 have to be shifted in with Q7 first,
so you can't just `shiftout()` the 24 bits in one go, not that you can do that
anyway as `shiftout()` handles only one byte at a time. See below re. the 595.)

### Four x Digit Buffers = Display Buffer

Now, recall that the 4 digits in the display are multiplexed: the digits (shift
register outputs) have to be cycled through all four "quickly" for the eye to
perceive a solid display. It's overall simplest and most efficient to decouple
the display rendering (character generation to SR bit values) from the display
refresh (shifting out the SR bits), by way of complete display buffer that holds
the entire 4-digit bit stream (4 x 16 bits). Note the relay outputs are _not_
multiplexed and don't need to be refreshed, however the relay SR is serially in
line with the display SRs: the same relay state _has_ to be shifted out simply
to push the display bits out to the display SRs. Given the SR outputs only
update when explicitly latched, there's no harm in this and indeed is an
efficient use of the available hardware.

There is no need to repeat the relay state in the display buffer, so they're
kept separate right up until serialisation out the SR data pin.

### Maximum Refresh Rate

The 24-bits of relay+display buffer, repeated for each of the 4 digits, results
in sending out 96 bits every display refresh cycle. The PLC board layout
precludes using the micro's hardware SPI, though Arduino's software `shiftout()`
is still plenty fast enough: apparently it'll spit out 8 bits in ~0.1ms, or
~1.2ms for refreshing all four digits. That's more than fine for this
application given we're not doing much work the rest of the time. If desired,
there exist faster `shiftout()` implementations:
https://github.com/RobTillaart/Arduino/tree/master/libraries/FastShiftOut
http://nerdralph.blogspot.com/2015/03/fastest-avr-software-spi-in-west.html

### Some Details On The 74HC595

tl'dr of this section: the 74HC595 is "natively" most significant bit (MSb),
for an octet to be 1:1 reflected on the 595's Q7-Q0 outputs it has to be shifted
in MSBFIRST.

TI's 74HC595 data sheet labels the shift register outputs as Q_A through Q_H;
some other vendors label them as Q0 through Q7, with Q1-Q7 conveniently lining
up with pins 1-7; Q0 is over on pin 15. The PLC schematic uses the latter
labelling.

Conventional bit ordering is least significant bit (LSb) in the binary 1's
place: the right-most bit 0 is least-significant; there's no inherent reason to
require the shift register outputs to correspond to that but it'll be less
confusing when Q7-Q0 maps to bits 7-0.

The 74HC595 is "natively" most significant bit (MSb): after shifting in 8 bits,
the Q7 output will contain the first (0'th) bit, and Q0 the last (7'th) bit. Put
another way, if you wanted to reflect a uint8_t value in the micro on the shift
register in the conventional LSb ordering, you'd have to shift that value in to
the shift register MSBFIRST.

i.e. if `hgfedcba` is a binary value with h-a representing a bit position (h-a
ordering here also reflecting TI's labelling):

```text
shiftOut(MSBFIRST, hgfedcba) -> Q7=h Q6=g ... Q1=b Q0=a
```

On the PLC board the pair of 8-bit shift registers used for the display are
daisy chained with U4 following U3; the 16 bits `ponmlkji_hgfedcba` would need
to go out as:

```text
shiftOut(MSBFIRST, hgfedcba), shiftOut(MSBFIRST, ponmlkji) ->
    U3: Q7=p Q6=o ... Q1=j Q0=i
    U4: Q7=h Q6=g ... Q1=b Q0=a
```

That is, U3 will end up holding the upper 8 bits, U4 the lower.

### Pre-Calculating the Font+Register Mapping

Carrying on from the Display-SR pin mapping above, the following reflects, per
the schematic, the bit pattern associated with each pin on the display along
with the 16-bit numerical representation of that pattern; also shown is the mask
that selects all the segments/commons: (at this point these are all logic level,
ignoring display common cathode/anode)

```text
segments - IC:pin - SR pattern
       a - U3:Q4  - 00010000 00000000 (0x1000)
       b - U4:Q4  - 00000000 00010000 (0x0010)
       c - U3:Q7  - 10000000 00000000 (0x8000)
       d - U3:Q3  - 00001000 00000000 (0x0800)
       e - U3:Q1  - 00000010 00000000 (0x0200)
       f - U3:Q6  - 01000000 00000000 (0x4000)
       g - U4:Q3  - 00000000 00001000 (0x0008)
      DP - U3:Q5  - 00100000 00000000 (0x2000)
               mask 11111010 00011000 (0xFA18)

commons (display digit select (aka DIG1-DIG4; see below))
      K1 - U3:Q2  - 00000100 00000000 (0x0400)
      K2 - U4:Q1  - 00000000 00000010 (0x0002)
      K3 - U4:Q2  - 00000000 00000100 (0x0004)
      K4 - U4:Q5  - 00000000 00100000 (0x0020)
               mask 00000100 00100110 (0x0426)
```

- aside: the schematic reuses the labels K1-K4 for the buttons, those
  are connected directly to the micro and aren't relevant to the shift
  register processing

Shift register outputs U3:Q0 and U4:Q0,Q6,Q7 are unused.

A common anode display requires the segment to be driven _low_ (i.e. active
low); the above (logic level) segment values need to be inverted when presented
by the SR to the display. The commons are active-high, so stay as-is.

## 2. Defining A Font: Mapping Characters to Display Segments

A 7-segment character + associated decimal point can be defined with 8-bits:

```text
dp a b c d e f g
```

The MAX7219, and the library at https://github.com/JemRF/max7219, provide a
'font' ready to go. Lifting the desired characters from there:

```text
0b01111110, // 0
0b00110000, // 1
0b01101101, // 2
0b01111001, // 3
0b00110011, // 4
0b01011011, // 5
0b01011111, // 6
0b01110000, // 7
0b01111111, // 8
0b01111011, // 9
0b01111110, // O (same as 0)
0b00010101, // n
0b01000111, // F
0b01001111, // E
0b00000101, // r
```

- DP is always 0 as it's not a fixed part of any of the above characters

To map the above patterns to the necessary shift register outputs requires
combining the above character font with the previous shift-register-to-display-
pin mapping, enabling every SR output that corresponds to every bit of the
character. i.e. ORing all the SR values that correspond to an enabled font
segment. This can be expressed as the bitwise sum-product of the two bit
vectors.

```text
For example for the character '4' = 0b00110011 =
    0 x 0x2000 +    // DP
    1 x 0x1000 +    // a
    1 x 0x0010 +    // b
    1 x 0x8000 + ...
for a final value of 0xC018.
```

Repeating the above for the whole character set (via a spreadsheet), we get:

```text
0xDA10, // 0
0x8010, // 1
0x1A18, // 2
0x9818, // 3
0xC018, // 4
0xD808, // 5
0xDA08, // 6
0x9010, // 7
0xDA18, // 8
0xD818, // 9
0xDA10, // O
0x8208, // n
0x5208, // F
0x5A08, // E
0x0208, // r
```

Aside: the digit selection bits K1-K4 are "mixed in" to the character data at
display refresh time.

We're not done yet: the above table is only the logic level values; still need
to XOR the segment mask to flip the segment bits to active-low. The arrays in
the code reflect these final values.

In summary, the display is managed by:

- _updateDigit() does the rendering: writes the relevant "magic number"
  constants corresponding to the desired characters/symbols into the buffer
  - the only time bit operations are needed are to "mix in" the DPs/colon
- refreshDisplayAndRelays() cycles out the bitstream (could be triggered via
  an interrupt to maintain a consistent refresh period)

## Buttons and Inputs

The 'K1-4' button and 'IN1-8' optocoupled inputs are active-low.
