# Hardware

The JalSense prototype is built on a breadboard from off-the-shelf modules. No custom PCB.

## Wiring diagram

**As built:** [`jalsense-circuit-schematic-v2.png`](jalsense-circuit-schematic-v2.png) ([SVG](jalsense-circuit-schematic-v2.svg)) is the schematic of the working prototype: 5.5 k resistors, relay VCC on 5 V with the JD-VCC jumper ON, the analog TDS meter on GPIO34. The diagram below is the original design drawing; where they differ, follow the as-built schematic and the notes in [`../docs/prototype-build-notes.md`](../docs/prototype-build-notes.md).

![JalSense prototype wiring](jalsense-wiring.svg)

Wires that end in a coloured tag connect by name to every other tag with the same name: all **+5V** tags are the breadboard rail fed by the adapter, all **+3V3** tags are a rail fed from the ESP32's 3V3 pin, and every ground symbol goes to the shared GND rail. A 2600 × 1660 PNG for slides: [`jalsense-wiring.png`](jalsense-wiring.png).

Dividers keep every input inside the ESP32's limits: turbidity 4.5 V × 1/2 = 2.25 V at GPIO35 (R1 = R2); flow pulses 5 V × 2/3 = 3.3 V at GPIO27 (R4 = 2 × R3, built as two equal resistors in series). The ratios matter, not the values: the design used 10 kΩ, the prototype was built with **5.5 kΩ** resistors and needed no code change.

## Parts list

Also available as [`bom.csv`](bom.csv). Prices marked ≈ are estimates.

| Part | Qty | Job | ₹ |
|---|---:|---|---:|
| ESP32-WROOM-32 DevKit, 38-pin | 1 | controller, Wi-Fi dashboard | ≈ 400 |
| Analog TDS meter V1.0 | 1 | dissolved salts in the inlet water | 350 |
| Turbidity sensor with module | 1 | detects cloudy water | ≈ 450 |
| DS18B20 waterproof probe, 1 m | 1 | temperature compensation for TDS | ≈ 120 |
| YF-S201 water flow sensor | 1 | litres on the bypass path | — |
| 5 V dual relay module with optocoupler | 1 | switches pump A and pump B | ≈ 90 |
| DC 3–6 V mini submersible pump | 2 | pump A = bypass, pump B = RO | ≈ 140 |
| 0.96" OLED, I2C (SSD1306 or SH1106 chip) | 1 | live readings and decision | ≈ 220 |
| 5 V 2 A adapter | 1 | powers everything | ≈ 150 |
| DC jack female pigtail, 2.1 × 5.5 mm | 1 | adapter to breadboard rails | ≈ 25 |
| MB102 830-point breadboard | 1 | holds the circuit | ≈ 120 |
| Resistors, six of one value (10 kΩ, or 5.5 kΩ as built); a 4.7 kΩ pull-up is typical for the DS18B20 | 6 | dividers (R1 to R4) and DS18B20 pull-up (R5) | 30 |
| 6 mm silicone tube | 1 m | pump outlets to the jars | 15 |
| **Total** | | | **≈ 2,100** |

Worth adding (about ₹100): another metre of 6 mm tube, two 1/2-inch to 6 mm hose-barb reducers for the YF-S201, and jumper wires. A **470 µF 16 V capacitor** across 5V and GND is part of the build (it is in the schematic).

## Wiring table

| From | To | Net / note |
|---|---|---|
| DC jack red (+) | breadboard red rail | +5V |
| DC jack black (−) | breadboard blue rail | GND |
| +5V rail | turbidity VCC, flow sensor red, relay VCC (JD-VCC jumper ON), relay COM1 and COM2; ESP32 5V pin only when the ESP32 is *not* on USB | +5V |
| ESP32 3V3 pin | TDS +, OLED VCC, DS18B20 red, R5 | +3V3 |
| ESP32 GND | GND rail; every module's GND / black / − | GND |
| TDS board A | GPIO34 | 0–2.3 V |
| Turbidity A | R1 → GPIO35; R2 (same value) from GPIO35 to GND | at most 2.25 V |
| Flow sensor yellow | R3 → GPIO27; R4 (two resistors of the same value as R3 in series) from GPIO27 to GND | 5 V pulses → 3.3 V |
| DS18B20 yellow | GPIO4; R5 (about 4.7 kΩ) from GPIO4 to +3V3 | 1-Wire pull-up |
| OLED SDA / SCL | GPIO21 / GPIO22 | I2C, address 0x3C. On the ESP32's upper pin row (D23, D22, D21, D4) there is no free breadboard hole: use the pin tips |
| Relay IN1 / IN2 | GPIO25 / GPIO26 | LOW = relay on |
| Relay NO1 | pump A red; pump A black to GND | bypass path (never connect a pump to COM or NC) |
| Relay NO2 | pump B red; pump B black to GND | RO path |
| BOOT button (on the board) | GPIO0 | Dispense |
| Jumper wire (optional) | GPIO23 to GND | fitted = lock mode |

## Before first power-on

- **Relay module, two options.** *Design option:* remove the jumper between JD-VCC and VCC, feed VCC from 3V3 and JD-VCC from 5V. *As built:* on our module the relay coils would not switch with that arrangement, but they worked with the **JD-VCC jumper ON and VCC wired to 5V**, and both pumps stayed off while idle. Check that your pumps are off at power-up before trusting either wiring.
- **Pump wiring:** 5 V on COM1 and COM2, each pump's red lead on **NO**, black leads on GND. A pump on COM or NC runs all the time.
- **Power:** the ESP32 on USB and the pumps and relay on the 5 V 2 A adapter, with the grounds joined, stopped the OLED and Wi-Fi glitching when a pump started. When the laptop's USB cable is plugged in, keep the wire from the 5V rail to the ESP32's 5V pin unplugged (GND stays connected).
- **OLED:** many "SSD1306" modules carry an SH1106 chip. The `JalSense_auto` firmware handles that (`OLED_IS_SH1106`).
- **Flow signal:** spin the flow sensor's wheel and measure GPIO27: it should swing between about 0 V and 3.3 V. If the high level is below 2.6 V, change R3 to 1 kΩ.
- **Turbidity module:** use its A (analog) pin, not D.
- Keep the breadboard above the water line, with a tray under the plumbing.
- Upload with Upload Speed 115200 and the pump adapter unplugged; hold BOOT while it connects.

## Notes

- The **LM2596** step-down module isn't used: it can't make 5 V from a 5 V adapter. Keep it as a spare.
- The analog **TDS meter reads 0 to 1000 ppm**. Far above that it saturates and can read wrongly low.
- The **YF-S201** counts reliably from about 1 L/min. If a mini pump through 6 mm tube barely registers, set `USE_FLOW_SENSOR` to `false` in the firmware and both paths are metered by calibrated pump run time.
- In a **product**, the two pumps become two 24 V solenoid valves on the purifier's own water pressure, switched by MOSFETs rather than relays.
