# Calibration

Open the Serial Monitor at 115200 baud with the line ending set to **Newline**. Calibration values are saved in the ESP32's flash and survive power cycles.

1. **TDS.** Put the TDS probe and a TDS pen meter in the same glass of clean water, not touching. Wait 30 seconds and read the pen, say 180 ppm, then send `p180`. Send `s`: TDS should match the pen within about 10%, and the pin reading should be above 150 mV.
2. **Clarity.** Put the turbidity sensor in clear water, shaded from direct light (black tape around it helps), and send `c`. Clarity should read about 100%. Two or three drops of milk in a glass of water should drop clarity below 85% and flip the decision to RO.
3. **Pump rates.** Hold pump A's outlet over a measuring jug and send `a`: the pump runs for exactly 10 seconds. Send the volume collected, for example `ma210`. Repeat for pump B with `b`, then `mb190`.
4. **Flow sensor.** Send `r`, dispense on the bypass path into the jug a few times until you have roughly a litre, then send the volume actually collected in millilitres, for example `l960`.
5. **Check.** Dispense once on each path into the jug. You should get 250 mL within about 25 mL.
6. **At a new location.** Re-run `c` in clear water under the local lighting; the turbidity sensor is optical.

`x` resets everything to the defaults (clarity reference 4100 mV, TDS factor 1.0, 450 pulses per litre, 20 mL/s for both pumps).

## TDS sensor output at 25 °C

From the DFRobot transfer curve used in the firmware, before the `p` correction factor.

| Water | TDS board output | What JalSense does |
|---|---:|---|
| Dry probe | ≈ 0 V | under 150 mV → fault → RO |
| About 60 ppm | 0.15 V | fault threshold, the bottom of the ESP32 ADC's accurate range |
| 100 ppm | 0.25 V | bypass |
| 300 ppm | 0.81 V | decision threshold → RO |
| 1000 ppm | 2.16 V | top of the sensor's range → RO |

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
