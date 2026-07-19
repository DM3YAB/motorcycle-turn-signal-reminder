# Motorcycle Turn Signal Reminder using ATtiny10

A compact motorcycle turn signal reminder based on the ATtiny10.

Unlike conventional turn signal buzzers that beep on every flash, this project generates audible reminders only after a configurable number of turn signal flashes. This reduces annoyance during normal riding while still helping to avoid riding with the turn signal accidentally left on.

## Features

- ATtiny10 based design
- Only two external connections:
  - Turn signal (49a)
  - Ground
- No permanent +12 V supply required
- Powered from the turn signal pulses using GoldCap energy storage
- Active piezo buzzer
- Blue status LED
- Low component count
- SMD design

## Reminder sequence

| Flash count | Reminder |
|-------------|----------|
| 10 | 1 short beep |
| 20 | 2 short beeps |
| 30 | 3 short beeps |
| 40 and every additional 10 flashes | 1 long + 1 short beep |
| No flashes for 10 seconds | Counter reset |

## Hardware

Main components:

- ATtiny10
- MCP1703 5 V regulator
- Two GoldCap capacitors
- Active piezo buzzer (3–12 V)
- Blue LED
- Zener diode and input protection network

## Principle of operation

The circuit harvests energy directly from the motorcycle's turn signal output (terminal 49a). The GoldCap capacitors store sufficient energy to operate the microcontroller and buzzer without requiring a permanent battery connection.

The firmware counts the turn signal flashes and generates increasingly noticeable reminder patterns while keeping normal turn signalling completely silent.

## Repository contents

```
Firmware/
    SignalReminder.ino

Hardware/
    Schematic
    PCB

Images/
```

## License

MIT License
