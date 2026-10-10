# Run JalSense in Wokwi

A browser simulation of the JalSense controller, running the same firmware as the real board. Use it to show the logic working before the hardware is ready, or to practise the demo.

## Open it

1. Go to [wokwi.com/projects/new/esp32](https://wokwi.com/projects/new/esp32) (sign in to save).
2. In the **sketch.ino** tab, replace everything with [`sketch.ino`](sketch.ino).
3. In the **diagram.json** tab, replace everything with [`diagram.json`](diagram.json).
4. Click **Library Manager** (or add a file named `libraries.txt`) and add the four libraries in [`libraries.txt`](libraries.txt).
5. Press the green **Start** button.

## What stands in for what

Wokwi has no TDS, turbidity or flow sensor and no water, so:

| On screen | Stands in for |
|---|---|
| `tds_sensor` knob | TDS meter output. Turn it up for salty water: about 0.8 V on the pin is 300 ppm. |
| `turbidity_sensor` knob | Turbidity output after the 10k/10k divider. Turn it down for cloudy water. |
| `temp_probe` | DS18B20 (click it to change the temperature) |
| Blue button | Dispense (the BOOT button on the real board) |
| Slide switch | Lock jumper on GPIO23 |
| Two relay modules + LEDs | The dual relay module and the two pumps: blue LED = pump A (bypass), orange = pump B (RO) |

The simulation build (`#define JALSENSE_SIM 1`) has no Wi-Fi hotspot and no flow sensor, so both pumps are metered by run time. Everything else, including the decision rule, fail-safes and serial commands, is identical to the real firmware.

## Try this

1. Set the turbidity knob to about two-thirds and send `c` in the serial monitor (clear-water reference).
2. Set the TDS knob low but not at zero: the OLED shows **BYPASS**. Press Dispense; the blue LED lights for about 12 s.
3. Turn TDS up past 300 ppm: the decision flips to **RO**. Press Dispense; the orange LED lights.
4. Turn TDS all the way down (a dry probe): **RO, TDS fault**.
5. Slide the switch: **LOCK**, always RO.

If both LEDs light as soon as the simulation starts, change `"transistor": "pnp"` to `"npn"` for both relay modules in `diagram.json`.
