# Troubleshooting

| What you see | Likely cause | Fix |
|---|---|---|
| OLED blank | wiring or address | Check 3V3, GND, SDA on GPIO21, SCL on GPIO22. If Serial says "OLED not found", try 0x3D in the code. |
| OLED says "ADS1115 missing" | ADC not on the bus | ADDR must go to GND; VDD must be 3.3 V; check SDA/SCL aren't swapped. |
| TDS reads near 0 in water | probe connector or board power | Reseat the probe plug; TDS board on 3V3; AOUT to ADS1115 A0. |
| TDS jumps around | bubbles, or probe touching the tank | Keep the probe off the walls and 2 cm from the turbidity sensor. |
| Clarity changes with room lights | optical sensor seeing ambient light | Shade it with black tape or a tube and re-run `c`. |
| Valve clicks, no water | pilot valve needs pressure, or an air lock | Prime the pump; if still dry, see "If the valves won't open" in [hardware/README.md](../hardware/README.md). |
| "No flow: refill inlet tank" | empty tank, kinked tube, or flow too low for the sensor | Refill, straighten tubing, check the flow sensor's arrow points with the flow; consider a YF-S401. |
| ESP32 resets when the pump starts | supply dip or motor noise | C1 close to the loads, C3 across the pump, thick ground wires back to the adapter. |
| MOSFET hot | wrong part or weak gate drive | Confirm IRLZ44N, not IRFZ44N; check the 220 Ω gate resistor. |
| Upload fails | driver, cable or boot mode | Install the CP210x or CH340 driver, use a data cable, hold BOOT during "Connecting…". |
| Phone can't open the dashboard | phone using mobile data | Join `JalSense-Demo`, turn off mobile data, open `http://192.168.4.1`. |
