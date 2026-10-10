# Design notes

Why each part of the circuit and firmware is there.

## Sensing

**External 16-bit ADC (ADS1115).** The ESP32's built-in ADC is inaccurate below about 150 mV. Water at 50 ppm produces about 0.12 V from the TDS board and a dry probe produces near 0 V, so the built-in ADC could not tell them apart. That would break the "probe out of water" fail-safe. The ADS1115 is accurate down to zero.

**Temperature compensation.** A TDS probe measures conductivity, which rises about 2% per °C. Every reading is corrected to 25 °C with the DS18B20 reading; if the probe is missing, the firmware falls back to 25 °C.

**Median of 15 readings.** A median throws away spikes from bubbles or pump noise, which an average would smear into the result.

**Voltage dividers.** Inputs must stay under 3.3 V. The turbidity module outputs up to 4.5 V: 4.5 × 12/(10+12) = 2.45 V. The flow sensor's 5 V pulses: 5 × 18/(10+18) = 3.2 V, still above the ESP32's logic-high threshold of about 2.5 V.

**Flow counting.** The turbine spins a magnet past a Hall sensor. An interrupt on GPIO27 counts every falling edge, so no pulse is missed while the loop does other work. Litres = pulses ÷ pulses-per-litre, calibrated with a measuring jug.

**DS18B20 pull-up.** 1-Wire is an open-drain bus: devices only pull the line low, and the 4.7 kΩ resistor returns it high between bits.

## Switching

**Logic-level MOSFETs.** A GPIO gives tens of milliamps at 3.3 V; a valve needs roughly 0.3–0.6 A at 12 V. The IRLZ44N works as a low-side switch and conducts fully enough at a 3.3 V gate drive for these loads.

**Gate resistors.** The 220 Ω limits the current spike that charges the gate capacitance and damps ringing. The 10 kΩ pull-down keeps the MOSFET off while the ESP32 boots and its pins float, so no valve opens at power-up.

**Flyback diodes.** When the MOSFET turns off, the coil's stored energy produces a large voltage spike (V = L·di/dt). The diode gives that current a path and clamps the spike. A Schottky 1N5819 is faster than a 1N4007; both work here.

**Valve before pump.** The valve opens 200 ms before the pump starts and closes 300 ms after it stops, so the pump never pushes against a closed valve.

## Power

**Buck converter, not a 7805.** Dropping 12 V to 5 V at about 0.3 A linearly burns (12 − 5) × 0.3 ≈ 2.1 W as heat. The LM2596 switches at roughly 80–90% efficiency.

**D4 on the ESP32 supply.** USB and the buck module can both feed the ESP32 without fighting: whichever is higher conducts. That makes it safe to debug over USB while the 12 V side is live.

**Bulk capacitor on 12 V.** The pump draws a surge at start-up; the 470 µF supplies it locally so the rail doesn't dip and reset the ESP32.

## Decision logic

The checks run in a fixed order: lock mode, then sensor faults, then TDS, then clarity. Faults come before water-quality limits, so a broken sensor can never lead to a bypass. The decision is locked when a dispense starts. The pump stops after 3 s without flow pulses, which protects it from running dry.
