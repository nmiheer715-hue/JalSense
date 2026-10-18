# Design notes

Why each part of the circuit and firmware is there.

## Sensing

**Reading sensors with the ESP32's own ADC.** The ESP32's ADC (at its default 11 dB attenuation) is accurate between about 150 mV and 2450 mV. The design stays inside that window: the turbidity divider caps its signal at 2.25 V, the firmware uses the factory-calibrated `analogReadMilliVolts()`, and any TDS signal under 150 mV is treated as unknown and forces RO. That covers a dry or unplugged probe, and water under about 60 ppm, which can't be measured reliably here. A product version would add a 16-bit ADC such as the ADS1115 to measure very pure water too.

**Temperature compensation.** A TDS probe measures conductivity, which rises about 2% per °C. Every reading is corrected to 25 °C with the DS18B20; if the probe is missing, the firmware falls back to 25 °C.

**Median of 21 readings.** A median throws away spikes from bubbles or pump noise, which an average would smear into the result.

**Voltage dividers.** ESP32 pins must stay under 3.3 V. The turbidity module outputs up to 4.5 V: 4.5 × 10/(10+10) = 2.25 V. The flow sensor's 5 V pulses: 5 × 20/(10+20) = 3.3 V, comfortably above the ESP32's logic-high threshold of about 2.5 V.

**Flow counting.** The turbine spins a magnet past a Hall sensor. An interrupt on GPIO27 counts every falling edge, so no pulse is missed while the loop does other work. Litres = pulses ÷ pulses-per-litre, calibrated with a measuring jug. The flow sensor sits on the bypass line, because the litres that skip RO are the ones that become "litres saved"; the RO path is metered by calibrated pump run time.

**DS18B20 pull-up.** 1-Wire is an open-drain bus: the sensor only pulls the line low, and the resistor returns it high between bits. 4.7 kΩ is typical; 2 kΩ is stronger and fine for a 1 m cable.

## Switching

**Relay module instead of MOSFETs.** A GPIO gives a few milliamps at 3.3 V; a pump needs a couple of hundred milliamps at 5 V and makes electrical noise. The relay contacts switch the pump straight from the 5 V supply, and the module's optocouplers keep the pump circuit separate from the ESP32. A product would use MOSFETs (smaller, silent, longer-lasting); for a bench build the module is quicker to wire, and its click makes each decision audible.

**JD-VCC jumper removed.** With the jumper on, the optocoupler LEDs are fed from 5 V, and the ESP32's 3.3 V HIGH may not switch a relay fully off. With VCC on 3.3 V and JD-VCC (the coil supply) on 5 V, HIGH means off and LOW means on.

**Active-LOW, off at boot.** The firmware writes HIGH to both relay inputs before doing anything else, so neither pump runs at power-up. GPIO25 and GPIO26 are not boot-strapping pins and float harmlessly during reset.

**Two pumps instead of two valves.** A home purifier already has water pressure, so a product uses two normally-closed solenoid valves. A bench rig has no pressure, so two submersible pumps play the part of those valves.

## Power

**One 5 V supply.** The adapter feeds the pumps, the relay coils, the turbidity module and the flow sensor directly. The ESP32's regulator makes 3.3 V for the TDS board, OLED, DS18B20 and the relay inputs.

**USB and adapter never both on the 5V pin.** Tying the laptop's 5 V to the adapter's 5 V through the board lets the two sources push current into each other. When USB is connected, the wire from the 5V rail to the ESP32's 5V pin comes out; GND stays connected.

**Bulk capacitor.** A pump draws a surge at start-up. A 470 µF capacitor across 5V and GND supplies it locally so the rail doesn't dip and reset the ESP32.

## Decision logic

The checks run in a fixed order: lock mode, then sensor faults, then clarity, then TDS. Faults come before water-quality limits, so a broken sensor can never lead to a bypass. Clarity comes before TDS because TDS measures dissolved salts and turbidity measures suspended particles; they are independent, so low-TDS water can still be cloudy, and the screen should name the visible problem first. The decision is locked when a dispense starts. The bypass pump stops after 3 s without flow pulses, and every run has a 20 s cap.
