# Troubleshooting

| What you see | Likely cause | Fix |
|---|---|---|
| OLED blank | wiring or address | Check 3V3, GND, SDA on GPIO21, SCL on GPIO22. If Serial says "OLED not found", try 0x3D in the code. |
| Always RO with "TDS probe check failed" | TDS signal under 150 mV | Check the probe plug and that TDS A goes to GPIO34. Use water above 100 ppm; very pure water reads too low. |
| TDS jumps around | bubbles, pump noise, or probe touching the tank | Keep the probe off the walls and away from the pumps; let bubbles clear. |
| Clarity changes with room lights | optical sensor seeing ambient light | Shade it with black tape or a tube and re-run `c`. |
| A relay stays on, or both click at power-up | JD-VCC jumper still fitted, or relay VCC on 5V | Remove the jumper; VCC to 3V3, JD-VCC to 5V. If a relay is on exactly when it should be off, set `RELAY_ACTIVE_LOW` to `false`. |
| Relay clicks, no water | pump not submerged, air lock, or kinked tube | Keep the pump fully under water; squeeze the tube to clear air. |
| "No flow: refill inlet tank" | empty tank, or flow too low for the YF-S201 | Refill. If the sensor barely counts, set `USE_FLOW_SENSOR` to `false`. |
| ESP32 resets when a pump starts | 5 V rail dip | Add a 470 µF capacitor across 5V and GND; use short, thick wires from the adapter. |
| Upload fails | driver, cable or boot mode | Install the CP210x or CH340 driver, use a data cable, hold BOOT during "Connecting…". |
| Phone can't open the dashboard | phone using mobile data | Join `JalSense-Demo`, turn off mobile data, open `http://192.168.4.1`. |
