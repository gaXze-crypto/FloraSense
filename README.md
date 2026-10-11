# FloraSense V1 — Giving Plants a Voice

A remote-controlled, interactive plant companion built with an ELEGOO / Arduino Mega 2560 and an 8×8 MAX7219 LED matrix. It monitors temperature, air humidity, ambient light, **water level** (not soil moisture), and visitor proximity; expresses emotions through faces, RGB lighting and a passive buzzer; and includes three mini-games.

## Required Arduino libraries

Install using Arduino IDE Library Manager:
- **IRremote** by Armin Joachimsmeyer (`IRremote.hpp`)
- **LedControl** by Eberhard Fahle (`LedControl.h`)
- **DHT sensor library** by Adafruit (`DHT.h`)
- **Adafruit Unified Sensor** (DHT dependency)

Choose **Arduino Mega or Mega 2560** as the board. Open `FloraSense.ino` and upload. It starts in software-OFF mode; press **ON** on the IR remote.

## Wiring

| Component | Component connection | Arduino Mega |
|---|---|---|
| DHT11 module | Signal / DATA | **D2** |
| DHT11 module | VCC, GND | 5V, GND |
| Passive buzzer | +, − | **D8**, GND |
| Common-cathode RGB LED | Red through 220–330Ω | **D9** |
| Common-cathode RGB LED | Green through 220–330Ω | **D10** |
| Common-cathode RGB LED | Blue through 220–330Ω | **D11** |
| Common-cathode RGB LED | Common cathode | GND |
| MAX7219 8×8 module **IN** side | DIN, CS, CLK | **D22, D23, D24** |
| MAX7219 module | VCC, GND | 5V, GND |
| HC-SR04 | TRIG, ECHO | **D26, D27** |
| HC-SR04 | VCC, GND | 5V, GND |
| IR receiver module (verify its markings) | Signal (Y / S / OUT) | **D28** |
| IR receiver | Power, Ground | 5V, GND |
| **Analog water-level/contact sensor** | S / AO | **A0** |
| Analog water-level/contact sensor | +, − | 5V, GND |
| Photoresistor divider | Midpoint between LDR and 10kΩ resistor | **A1** |
| Photoresistor divider | LDR other side, resistor other side | 5V, GND |

**Water sensor meaning:** An analog water-level probe tells you whether water is touching the probe / how far up the probe it reaches. It **does not** measure soil moisture and does not automatically tell you how much water a plant needs. Mount it in a water reservoir or saucer only if that is actually what you intend to monitor. Keep its electronics dry, prevent short circuits, and note that continuously powering cheap resistive probes causes corrosion. The water-level percentage is an approximate calibrated value, not a precise water volume.

**If the sensor is not connected yet:** change `WATER_ENABLED` to `false` in the sketch to avoid false low-water alerts. Do not connect a sensor of unknown pinout to 5V without checking its label/datasheet.

## IR remote controls (verified NEC codes)

| Key | Code | Action |
|---|---|---|
| ON | `0x45` | Software power on/off |
| OK | `0x40` | Open menu / select / Snake pause / Reaction tap |
| UP / DOWN | `0x09` / `0x07` | Navigate; game directions |
| LEFT / RIGHT | `0x44` / `0x43` | Navigate; game directions |
| FUNC/STOP | `0x47` | Back / exit game |
| VOL+ | `0x46` | Play greeting melody |
| VOL− | `0x15` | Toggle mute |
| EQ | `0x19` | Manually switch day/night mode |

In the main menu: **Mood → Environment → Games → Sound**. Environment shows 2-digit readings and prints their labels to Serial Monitor (9600 baud); use UP/DOWN to cycle temperature (°C), humidity (%), relative light (%), water level (%), and illustrative health score (/100). The display has no room to label each number, so use Serial Monitor to identify it. In **Games**, choose Snake, Simon, or Reaction with arrows and OK.

### Games

- **Snake:** Four arrow keys steer the snake. Collect food, avoid walls and yourself. OK pauses/resumes; FUNC/STOP exits. After losing, score is shown briefly and the game restarts paused. OK resumes.
- **Simon:** Watch a growing sequence of arrows, then repeat it with the remote arrows. Each round adds one more direction. A mistake displays the number of completed rounds and restarts.
- **Reaction:** Wait for the lightning icon, then press OK fast. Early presses lose. Serial Monitor shows reaction time in milliseconds; LED matrix shows hundredths of a second (capped at 99). A new attempt starts automatically.

## Calibration

Inside `FloraSense.ino`, adjust:

```cpp
constexpr int WATER_EMPTY_RAW=0;
constexpr int WATER_FULL_RAW=550;
constexpr int WATER_LOW_PERCENT=25;
constexpr bool WATER_ENABLED=true;
constexpr int DARK_RAW=600;
```

Open Serial Monitor at **9600 baud**. Note `WaterRaw` with the probe dry / out of water, then with the probe at your chosen full level. Replace `WATER_EMPTY_RAW` and `WATER_FULL_RAW`. The sketch maps readings between these values to 0–100%. Set `WATER_ENABLED=false` if you aren't using the sensor. Calibrate light for your specific plant and placement; LDR readings aren't true lux measurements.

## Design notes / limitations

- One Arduino sketch; no app, EEPROM memory or servo required. Remote is the **only** control interface.
- Software ON/OFF does **not** remove power from the Arduino: IR continues to operate; sensors can continue reading while the face, buzzer and RGB LED are off.
- EQ switches **manual** day/night mode because there is no real-time clock. Night mode suppresses darkness-related alerts, not other important conditions.
- Sensor thresholds and the 0–100 environmental health score are **demonstration heuristics**, not plant-specific horticultural diagnoses.
- Sound is generated by rapid `digitalWrite()` changes instead of `tone()`, preserving the previously tested IR operation. Blocking DHT and ultrasonic sensor reads can still cause intermittent sound jitter. Ultrasonic checks are skipped during melodies and games.
- IR repeat frames are ignored, plus a same-key cooldown. Extremely long holds may still generate occasional new full frames, depending on remote firmware.
- Proximity detection identifies **objects**, not people. A visitor entering <15 cm triggers a greeting, once per entry with 30-second cooldown.
- The code was prepared as a consolidated prototype; verify compile/upload and all interactions on your exact kit before presenting it as hardware-tested.

## Roadmap

- Improve timing with non-blocking ultrasonic measurement and more reliable audio scheduling.
- Calibrate sensor thresholds for a particular plant, and improve water-sensor corrosion resistance.
- Add optional servo-powered leaf later.
- Consider optional persistent memory and an app only in a future version.

## License

For a public repository, add a `LICENSE` file of your choice (for example, MIT) once you've decided how you want others to reuse the code.
