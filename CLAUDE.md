# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A single Arduino sketch ([spectrolink2nest.ino](spectrolink2nest.ino)) that emulates a Braemar Spectrolink wall controller. It lets an external controller (designed for, but not limited to, a Nest thermostat) drive a Braemar ducted heating/cooling system by reading simple on/off GPIO inputs and emitting the proprietary Spectrolink serial protocol the furnace expects.

There is no build system, test suite, or dependency manifest. Build and upload with the Arduino IDE. The only external dependency is the **SendOnlySoftwareSerial** library, which must be installed via the Arduino Library Manager.

## Build / upload / debug

- **Compile & upload:** Arduino IDE → select board → Upload. (Or `arduino-cli compile` / `arduino-cli upload` if preferred.)
- **Debug:** Set `enableSerial = true` ([spectrolink2nest.ino:81](spectrolink2nest.ino#L81)). This opens the USB serial monitor at **9600 baud** for logging and switches input from physical pins to keyboard test pins. Single-char commands over serial toggle state: `1`/`2`/`3` toggle zones, `h` toggles heat, `c` toggles cool ([ReadSerialInput](spectrolink2nest.ino#L173)).
- The Spectrolink protocol output goes out on **pin 11 at 300 baud** regardless of `enableSerial`. USB serial and the Spectrolink TX are separate serial links — that's why SendOnlySoftwareSerial is used (frees the hardware UART for USB debugging).

## Hardware contract (see [README.md](README.md))

Inputs are **active-LOW** (`INPUT_PULLUP`); connect a pin to ground to turn it ON:
- Pin 3/4/5 → Zone 1/2/3, Pin 7 → Air Conditioning, Pin 8 → Heater
- Pin 11 → Spectrolink TX (the live control link)

A single Nest can't drive zones, so zones are toggled via external relays (Shelly). Nest's 24V AC heat/cool calls are converted to 12V DC to throw relays into pins 7/8.

## Architecture

Everything is one `loop()`-driven state machine in a single file. Two clocks govern it:
1. **Message tick** (`messagePeriod`, 1000ms): once per second the sketch reads inputs ([CheckPins](spectrolink2nest.ino#L196)), advances safety timers ([cooldownTimersTick](spectrolink2nest.ino#L141)), and transmits one protocol frame ([SendMessage](spectrolink2nest.ino#L363)).
2. **Serial input poll**: every loop iteration in debug mode.

Two pieces of state drive every output frame:
- **`systemState`** — the off/cooling/heating mode byte (the `stateXxx` constants).
- **`zoneByte`** — which combination of zones is open, recomputed by [UpdateZoneByte](spectrolink2nest.ino#L335) from the three `isZoneNOpen` flags.

### The Spectrolink protocol

Output is always an **8-byte frame** (`output[8]`) written byte-by-byte in [WriteOutput](spectrolink2nest.ino#L498). `output[0..6]` are data; `output[7]` is the checksum. Frame layout depends on `messageType`, chosen by walking `messageSequence`:
- **Type 1 — standard** ([CreateStandardMessage](spectrolink2nest.ino#L381)): the main control frame. Header `202`, then zone/mode bytes.
- **Type 2–4 — "low"** ([CreateLowMessage](spectrolink2nest.ino#L426)) and **Type 10–12 — "high"** ([CreateHighMessage](spectrolink2nest.ino#L438)): header `218`, used by slave units. The full `messageSequence` `{1,1,1,2,1,3,1,4,1,1,1,10,1,11,1,12}` is active, so these are transmitted.

**The checksum (`output[7]`) is computed, not looked up.** It is a CRC-8 — polynomial `0x62` (reflected / right-shift form), init `0x00`, no final XOR — over the seven data bytes `output[0..6]` in **reverse byte order** (`output[6]` first … `output[0]` last), LSB-first. Implemented in [spectrolinkCrc](spectrolink2nest.ino#L487). This was reverse-engineered from captured frames and reproduces every validated hard-coded value the sketch previously carried in `switch` lookup tables. (The old tables had one wrong entry — `zone123+heat` was `144`; the true CRC is `114` — but that combo is force-suppressed by the all-zones-heating guard and never transmitted.) The `stateXxx` and `zoneXxx` byte constants are still reverse-engineered data values — do not change them.

### Safety logic (the reason for "Heater stability fixes")

Several guards exist because the real furnace misbehaves otherwise — treat them as load-bearing, not optional:
- **Reset sequence** ([Reset](spectrolink2nest.ino#L283) / `resetSendCounter`): sends N special "reset" standard frames before normal operation resumes. Performed at boot, on heat/cool mode change, before starting heating, and periodically every `autoResetTime` (5 min) while off.
- **Cooldowns**: `heatingCooldownTime` (3 min) and `airconCooldownTime` (30 s) block re-enabling after shutoff.
- **Max runtime**: `heatingMaximumRuntime` (30 min) forces the heater off for a cool-down period.
- **Hysteresis** (`hysteresisWait`): input must persist `hysteresisWait` ticks before a state change commits, debouncing the thermostat.
- **All-zones-heating guard** ([CheckPins:261](spectrolink2nest.ino#L261)): with all 3 zones open in heat mode the system fails, so Zone 1 is force-closed.

## Conventions when editing

- Keep all logic in the single sketch; this project is intentionally one file.
- Timer/threshold tuning is done via the `const int` values at the top of the file (lines ~83–96) — prefer adjusting those over inlining literals.
- New (zone, mode) combinations need no checksum work — set the data bytes `output[0..6]` and `spectrolinkCrc()` derives `output[7]`.
