/*
Motorcycle Turn Signal Beeper using ATtiny10
------------------------------------------
Arduino/vMicro application for a minimalist ATtiny10 core
(setup()/loop(); no Arduino convenience functions required).

Hardware, SOT-23-6:
Pin 1 / PB0 / TPIDATA = Blue debug LED, active HIGH, 1.2 kOhm
Pin 2                 = GND
Pin 3 / PB1 / ADC1    = Turn signal via input circuit
Pin 4 / PB2           = Active 3–12 V piezo via driver, active HIGH
Pin 5                 = VDD = 5 V
Pin 6 / PB3 / RESET   = Reset/TPI

Input:
Via 4.7 V Zener diode and 10 kOhm / 4.7 kOhm voltage divider.
Expected ADC level when turn signal is ON: approx. 2.3–3.1 V.

Behavior:
- LED follows the detected turn signal.
- Each rising edge of the turn signal is counted once.
- 10 seconds without blinking: counter resets to 0.
- 10 pulses: one short tone.
- 20 pulses: two short tones.
- 30 pulses: three short tones.
- 40 pulses and every subsequent 10:
  one long tone followed by one short tone.

Programmer:
TPI configuration remains secure:
RSTDISBL = 1, WDTON = 1, CKOUT = 1.
Internal 8 MHz RC oscillator starts with a divide-by-8 prescaler,
resulting in a CPU clock speed of 1 MHz.
*/

#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

// -----------------------------------------------------------------------------
// Connection definitions
// -----------------------------------------------------------------------------

#define LED_BIT       PB0
#define ADC_BIT       PB1
#define BUZZER_BIT    PB2

#define LED_ON()       (PORTB |=  _BV(LED_BIT))
#define LED_OFF()      (PORTB &= (uint8_t)~_BV(LED_BIT))
#define BUZZER_ON()    (PORTB |=  _BV(BUZZER_BIT))
#define BUZZER_OFF()   (PORTB &= (uint8_t)~_BV(BUZZER_BIT))

// -----------------------------------------------------------------------------
// Time base
// -----------------------------------------------------------------------------

#define TICK_MS              10U
#define RESET_TICKS        1000U    // 1000 * 10 ms = 10 Sekunden

// -----------------------------------------------------------------------------
// ADC-Hysterese
// ATtiny10: 8-Bit-ADC, Referenz = VDD
//
// 5,0 V: 80 Digit ≈ 1,57 V  :  30 Digit ≈ 0,59 V
// -----------------------------------------------------------------------------

#define ADC_ON_THRESHOLD      80U
#define ADC_OFF_THRESHOLD     30U

// -----------------------------------------------------------------------------
// Beep pattern
// -----------------------------------------------------------------------------

enum
{
  PATTERN_NONE = 0,
  PATTERN_ONE_SHORT,
  PATTERN_TWO_SHORT,
  PATTERN_THREE_SHORT,
  PATTERN_LONG_SHORT
};

// -----------------------------------------------------------------------------
// State variables
// -----------------------------------------------------------------------------

static uint8_t blinkIsOn    = 0;
static uint8_t blinkCount   = 0;
static uint16_t quietTicks  = RESET_TICKS;

static uint8_t pattern      = PATTERN_NONE;
static uint8_t patternPhase = 0;
static uint8_t patternTicks = 0;

// -----------------------------------------------------------------------------
// Protected registers
// -----------------------------------------------------------------------------

static void watchdogDisable(void)
{
  const uint8_t oldSreg = SREG;
  cli();

  CCP = 0xD8;
  WDTCSR = 0;

  SREG = oldSreg;
}

// -----------------------------------------------------------------------------
// ADC
// -----------------------------------------------------------------------------

static void adcInit(void)
{
#ifdef PRR
  PRR &= (uint8_t)~_BV(PRADC);
#endif

  // Reference = VDD, Input = ADC1/PB1.
  ADMUX = 1U;

  // 1 MHz / 8 = 125 kHz ADC clock.
  ADCSRA = _BV(ADEN) | _BV(ADPS1) | _BV(ADPS0);

#ifdef DIDR0
  DIDR0 |= _BV(ADC1D);
#endif

  // Discard the first conversion after powering on the ADC.
  ADCSRA |= _BV(ADSC);
  while (ADCSRA & _BV(ADSC))
  {
  }
  (void)ADCL;
}

static uint8_t adcRead(void)
{
  ADCSRA |= _BV(ADSC);

  while (ADCSRA & _BV(ADSC))
  {
  }

  return ADCL;
}

// -----------------------------------------------------------------------------
// Beep pattern control
// Everything runs in a non-blocking manner with a 10 ms interval.
// -----------------------------------------------------------------------------

static void stopPattern(void)
{
  BUZZER_OFF();
  pattern = PATTERN_NONE;
  patternPhase = 0;
  patternTicks = 0;
}

static void startPattern(uint8_t newPattern)
{
  pattern = newPattern;
  patternPhase = 0;
  BUZZER_ON();

  switch (newPattern)
  {
    case PATTERN_ONE_SHORT:
    case PATTERN_TWO_SHORT:
    case PATTERN_THREE_SHORT:
      patternTicks = 4;       // 40 ms short tone
      break;

    case PATTERN_LONG_SHORT:
      patternTicks = 16;      // 160 ms long tone
      break;

    default:
      stopPattern();
      break;
  }
}

static void servicePattern(void)
{
  if (pattern == PATTERN_NONE)
  {
    return;
  }

  if (patternTicks != 0)
  {
    --patternTicks;
    return;
  }

  ++patternPhase;

  switch (pattern)
  {
    case PATTERN_ONE_SHORT:
      stopPattern();
      break;

    case PATTERN_TWO_SHORT:
      // 40 ms ON, 30 ms OFF, 40 ms ON
      if (patternPhase == 1)
      {
        BUZZER_OFF();
        patternTicks = 3;
      }
      else if (patternPhase == 2)
      {
        BUZZER_ON();
        patternTicks = 4;
      }
      else
      {
        stopPattern();
      }
      break;

    case PATTERN_THREE_SHORT:
      // 40 ms ON, 30 ms OFF, 40 ms ON, 30 ms OFF, 40 ms ON
      if ((patternPhase == 1) || (patternPhase == 3))
      {
        BUZZER_OFF();
        patternTicks = 3;
      }
      else if ((patternPhase == 2) || (patternPhase == 4))
      {
        BUZZER_ON();
        patternTicks = 4;
      }
      else
      {
        stopPattern();
      }
      break;

    case PATTERN_LONG_SHORT:
      // 160 ms ON, 80 ms OFF, 80 ms ON
      if (patternPhase == 1)
      {
        BUZZER_OFF();
        patternTicks = 8;
      }
      else if (patternPhase == 2)
      {
        BUZZER_ON();
        patternTicks = 8;
      }
      else
      {
        stopPattern();
      }
      break;

    default:
      stopPattern();
      break;
  }
}

// -----------------------------------------------------------------------------
// Blink counter
// -----------------------------------------------------------------------------

static void newBlinkPulse(void)
{
  quietTicks = 0;

  if (blinkCount < 40)
  {
    ++blinkCount;
  }

  if (blinkCount == 10)
  {
    startPattern(PATTERN_ONE_SHORT);
  }
  else if (blinkCount == 20)
  {
    startPattern(PATTERN_TWO_SHORT);
  }
  else if (blinkCount == 30)
  {
    startPattern(PATTERN_THREE_SHORT);
  }
  else if (blinkCount >= 40)
  {
    startPattern(PATTERN_LONG_SHORT);

    // The current 40th pulse already counts as the first pulse
	// of the next block of ten. At 31, after nine further
	// rising edges, the OFF state is triggered again at 40.
    blinkCount = 31;
  }
}

static void serviceBlinkInput(void)
{
	const uint8_t adcValue = adcRead();

	// Detect flashing signal with hysteresis
	if (blinkIsOn == 0)
	{
		if (adcValue >= ADC_ON_THRESHOLD)
		{
			blinkIsOn = 1;
			LED_ON();

			quietTicks = 0;
			newBlinkPulse();
		}
	}
	else
	{
		if (adcValue <= ADC_OFF_THRESHOLD)
		{
			blinkIsOn = 0;
			LED_OFF();

			// The quiet period begins only after the switch-off has been detected.
			quietTicks = 0;
		}
	}

	// As long as the turn signal is ON, no reset timer runs.
	if (blinkIsOn)
	{
		quietTicks = 0;
	}
	else if (blinkCount != 0)
	{
		if (quietTicks < RESET_TICKS)
		{
			++quietTicks;
		}
		else
		{
			blinkCount = 0;
			quietTicks = RESET_TICKS;
		}
	}
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

void setup(void)
{
  cli();

  watchdogDisable();

  // PB0 and PB2 output; PB1 and PB3 input.
  DDRB = _BV(LED_BIT) | _BV(BUZZER_BIT);

  // Defined initial state.
  LED_OFF();
  BUZZER_OFF();

#ifdef PUEB
  // Do not modify internal pull-ups on ADC1 and RESET; set ADC pull-up to OFF.
  PUEB &= (uint8_t)~_BV(ADC_BIT);
#endif

  adcInit();

  // The program does not require interrupts.
}

void loop(void)
{
  serviceBlinkInput();
  servicePattern();

  _delay_ms(TICK_MS);
}
