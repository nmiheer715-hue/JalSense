# Demo script

Five acts, about three minutes, inside an 8-minute pitch.

## Prepare the samples

- **Clean:** 100–250 ppm on a TDS pen. Packaged drinking water usually works. Don't use RO output: it reads too low and trips the fault check.
- **Salty:** about ¼ teaspoon of salt in 1 litre of the clean water, reading above 600 ppm. Carry 500 mL in a labelled bottle.
- **Cloudy (optional):** 2–3 drops of milk in a separate cup.
- Keep both pumps fully under water, or they suck air.

## The acts

1. **Clean water → bypass (45 s).** Pour the clean sample into the inlet tank. The screen shows TDS, clarity and BYPASS. Press BOOT: relay 1 clicks and pump A fills the drinking jar with 250 mL. *"A normal RO would have sent 750 mL down the drain for this glass."*
2. **Pull a sensor → fail-safe (30 s).** Lift the TDS probe out of the water. The screen flips to RO with "TDS probe check failed". Put it back; it returns to BYPASS. *"If a sensor fails, JalSense never gambles."*
3. **Lock mode (20 s).** Tap Lock mode on the phone (or fit the GPIO23 jumper): LOCK and RO. Turn it off again. *"For arsenic or fluoride areas, which TDS can't detect."*
4. **Salty water → RO (50 s).** Pour in the salt sample. TDS climbs past 300 and the decision flips to RO before anyone touches anything. Press BOOT: relay 2 clicks, pump B fills the RO jar, and the reuse counter rises.
5. **A judge presses the button (35 s).** Hand over a phone on the JalSense dashboard and let a judge dispense.

## Pack list

Spare ESP32 already flashed, jumper wires, spare resistors, multimeter, TDS pen, measuring jug, towels, tray, extension board, laptop with the Arduino IDE, USB cable, the Wokwi simulation open in a browser tab, and a backup video of the demo.
