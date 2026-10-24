# Calibration

Open the Serial Monitor at 115200 baud with the line ending set to **Newline**. Calibration values are saved in the ESP32's flash and survive power cycles.

1. **TDS.** Put the TDS probe and a TDS pen meter in the same glass of clean water, not touching. Wait 30 seconds and read the pen, say 180 ppm, then send `p180`. Send `s`: TDS should match the pen within about 10%, and the pin reading should be above 150 mV. A solution of 400 to 700 ppm gives a good single-point calibration (we got 548 ppm against a 550 ppm pen). The pin reading must stay **below about 2.2 V**: near 2.3 V the sensor is saturated and the water is outside its 0 to 1000 ppm range. Check clean tap water too. If the correction factor `k` looks wrong (it should be near 1.0), send `x` and calibrate again.
2. **Clarity.** Put the turbidity sensor in clear water, shaded from direct light (black tape around it helps), and send `c`. Clarity should read about 100%. Two or three drops of milk in a glass of water should drop clarity below 85% and flip the decision to RO.
3. **Pump rates.** Hold pump A's outlet over a measuring jug and send `a`: the pump runs for exactly 10 seconds. Send the volume collected, for example `ma210`. Repeat for pump B with `b`, then `mb190`.
4. **Flow sensor.** Send `r`, dispense on the bypass path into the jug a few times until you have roughly a litre, then send the volume actually collected in millilitres, for example `l960`.
5. **Check.** Dispense once on each path into the jug. You should get 250 mL within about 25 mL.
6. **At a new location.** Re-run `c` in clear water under the local lighting; the turbidity sensor is optical.

`x` resets everything to the defaults (clarity reference 4100 mV, TDS factor 1.0, 450 pulses per litre, 20 mL/s for both pumps), so repeat steps 1 to 4 afterwards.

Other serial commands (`JalSense_auto`): `o` turns auto mode on or off, `t<ppm>` sets a typed-in TDS when the build has no TDS meter (`USE_TDS_SENSOR = false`), `h` lists the commands.

## TDS sensor output at 25 °C

From the DFRobot transfer curve used in the firmware, before the `p` correction factor.

| Water | TDS board output | What JalSense does |
|---|---:|---|
| Dry probe | ≈ 0 V | under 150 mV → fault → RO |
| About 60 ppm | 0.15 V | fault threshold, the bottom of the ESP32 ADC's accurate range |
| 100 ppm | 0.25 V | bypass |
| 300 ppm | 0.81 V | decision threshold → RO |
| 1000 ppm | 2.16 V | top of the sensor's range → RO |
| well above 1000 ppm | about 2.3 V or more | **saturated**: the reading is not meaningful and can fall below 300 ppm. Do not use this meter for such water |

Use a clean demo sample between 100 and 250 ppm on the pen, so it sits well inside the accurate range.

## Firmware settings

| Constant | Default | What it does |
|---|---|---|
| `TDS_LIMIT_PPM` | 300 | at or above this, run RO |
| `TDS_MIN_VALID_MV` | 150 | TDS pin below this → dry probe or too low to trust → RO |
| `CLOUDY_RATIO` | 0.85 | clarity under 85% of the clear-water reading → RO |
| `DISPENSE_ML` | 250 | volume per press |
| `NO_FLOW_MS` | 3000 | bypass pump stops if the flow sensor sees nothing for 3 s |
| `MAX_PUMP_MS` | 20000 | absolute cap on one dispense |
| `USE_FLOW_SENSOR` | true | false = meter both paths by pump run time |
| `RELAY_ACTIVE_LOW` | true | the relay module switches on when its input is LOW |
| `RO_REJECT_FRACTION` | 0.75 | assumed RO reject share for the saved/reused counters |
| `USE_TDS_SENSOR` | true | false = no TDS meter fitted; TDS is a typed-in value |
| `OLED_IS_SH1106` | true | true for OLED modules with an SH1106 chip (most 0.96" modules sold as SSD1306), false for a real SSD1306 |
| `AUTO_ON_AT_BOOT` | true | start with auto mode on |
| `AUTO_STABLE_MS` | 3000 | the decision must stay the same this long before a pump starts |
| `AUTO_PAUSE_MS` | 3000 | rest between two automatic dispenses |
| `AUTO_SESSION_ML` | 1000 | auto mode switches itself off after this much water |
