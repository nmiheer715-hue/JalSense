# Calibration

Open the Serial Monitor at 115200 baud with the line ending set to **Newline**. Calibration values are saved in the ESP32's flash and survive power cycles.

1. **TDS.** Put the TDS probe and a TDS pen meter in the same glass of clean water, not touching. Wait 30 seconds and read the pen, say 180 ppm, then send `p180`. Send `s` and check the reading matches the pen within about 10%.
2. **Clarity.** Put the turbidity sensor in clear water, shaded from direct light (black tape around it helps), and send `c`. Clarity should read about 100%. Two or three drops of milk in a glass of water should drop clarity below 85% and flip the decision to RO.
3. **Flow.** Send `r` to reset the pulse counter. Press Dispense repeatedly into a measuring jug until you have roughly a litre, then send the volume actually collected in millilitres, for example `l960`. Required if you use a YF-S401 instead of the YF-S201: its pulse rate is about 13 times higher.
4. **Check.** Dispense once into the jug. You should get 250 mL within about 25 mL.
5. **At a new location.** Re-run `c` in clear water under the local lighting; the turbidity sensor is optical.

`x` resets everything to the defaults (clarity reference 4100 mV, TDS factor 1.0, 450 pulses per litre).

## TDS sensor output at 25 °C

From the DFRobot transfer curve used in the firmware, before the `p` correction factor.

| Water | TDS board output | Meaning |
|---|---:|---|
| Dry probe | ≈ 0 V | reads under 20 ppm → fault → RO |
| 20 ppm | 0.047 V | fault threshold |
| 50 ppm | 0.121 V | below the ESP32 ADC's accurate range; fine on the ADS1115 |
| 100 ppm | 0.249 V | bypass |
| 300 ppm | 0.814 V | decision threshold |
| 1000 ppm | 2.158 V | top of the sensor's range |

## Firmware settings

| Constant | Default | What it does |
|---|---|---|
| `TDS_LIMIT_PPM` | 300 | at or above this, run RO |
| `TDS_MIN_VALID_PPM` | 20 | below this, the probe is dry or unplugged → RO |
| `CLOUDY_RATIO` | 0.85 | clarity under 85% of the clear-water reading → RO |
| `DISPENSE_LITRES` | 0.25 | volume per press |
| `NO_FLOW_MS` | 3000 | stop the pump if no pulses for 3 s |
| `MAX_PUMP_MS` | 20000 | absolute cap on one dispense |
| `RO_REJECT_FRACTION` | 0.75 | assumed RO reject share for the saved/reused counters |
