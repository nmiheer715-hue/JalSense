# Prototype build notes

What we found and changed while building the round-two rig on a breadboard.
The newest firmware is `firmware/JalSense_auto/JalSense_auto.ino`. The original sketch in
`firmware/JalSense/` is untouched.

## What is new in `JalSense_auto.ino`

| Change | Why |
|---|---|
| **Auto mode** | Pumps start by themselves. Every few seconds it takes fresh readings, picks BYPASS (pump A) or RO (pump B) with the same rule as before, and dispenses 250 mL. |
| Auto-mode safety limits | Decision must be unchanged for 3 s; pauses on a sensor fault (a dry TDS probe usually means an empty tank); switches itself off after 1 L, or after a "No flow" / "Timeout" error. |
| **SH1106 OLED support** (`OLED_IS_SH1106 = true`) | Many "0.96 inch SSD1306" modules really carry an SH1106 chip. With the plain Adafruit SSD1306 driver the screen shows one scrolling line plus noise. The sketch now writes the frame page by page with a 2-column offset. Set it to `false` for a real SSD1306. |
| I2C at 100 kHz | More tolerant of jumper wires. |
| Wi-Fi hotspot tuning | Power-save off, fixed address `192.168.4.1`, channel 6, up to 2 phones. Phones join and load the page faster. |
| `USE_TDS_SENSOR` flag | `false` turns TDS into a typed-in value (Serial `t<ppm>` or the phone page) for builds without the analog TDS meter. |
| Phone page "Auto mode" button, Serial `o` | Turn auto mode on and off. Turning it on resets the 1 L session limit. |

## Wiring notes (the parts that caught us out)

- **Relay board:** keep the **JD-VCC jumper ON** and feed the relay **VCC from 5 V** (not 3V3).
  COM1 and COM2 go to 5 V, each pump's red lead goes on **NO** (never COM or NC), pump black leads to GND.
- **Power:** ESP32 from USB, pumps and relay from a 5 V 2 A adapter, with a common ground. Do not feed the
  ESP32 5V pin from the adapter while USB is plugged in. A 470 uF capacitor across 5 V and GND helps.
- **Resistors:** all six dividers and the DS18B20 pull-up were built with 5.5 k resistors. The two dividers
  keep their ratios (1:2 for turbidity, 1:3 for the flow signal using R3 + 2 x R4), so no code change.
- **ESP32 upper pin row (D23, D22, D21, D4):** there is no free breadboard hole, so use the pin tips.
- **Uploading:** Upload Speed 115200, unplug the pump adapter, hold BOOT while it connects, short USB cable.

## TDS notes

- The analog TDS meter V1.0 reads **0 to 1000 ppm**. Water far above that (we saw a 9990 ppm sample) saturates
  the sensor and can read wrongly low, so it must not be trusted there.
- `p<ppm>` sets the correction factor `k` against a reference meter. `x` resets `k` to 1.0.
  Calibrate in a 400 to 700 ppm solution. A tiny `k` (such as 0.263) hides a saturated sensor and is unsafe.
- The temperature probe (DS18B20) corrects TDS by about 2 % per degree C. If it prints "probe missing", the
  sketch assumes 25 C.

## Limits (be honest in demos)

- TDS and clarity say nothing about bacteria, arsenic or fluoride. Use **Lock mode** (GPIO23 to GND, or the
  phone button) to force RO where those are a concern.
- Hobby sensors drift: recalibrate against a TDS pen before each demo.
- This is a prototype. It has not been tested for drinking-water safety.

## Small test sketches

- `firmware/tools/OLED_test/OLED_test.ino`: I2C scan plus a test picture on the OLED.
- `firmware/tools/OLED_test2/OLED_test2.ino`: checks the SDA/SCL lines and scans swapped and alternative pins.
