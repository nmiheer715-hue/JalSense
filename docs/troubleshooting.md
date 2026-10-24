# Troubleshooting

| What you see | Likely cause | Fix |
|---|---|---|
| OLED blank | wiring or address | Check 3V3, GND, SDA on GPIO21, SCL on GPIO22. If Serial says "OLED not found", try 0x3D in the code. Run `firmware/tools/OLED_test2` to scan the bus. |
| OLED shows one scrolling line and noise | the module has an SH1106 chip, not an SSD1306 | Use `JalSense_auto` with `OLED_IS_SH1106 = true`. |
| OLED frozen or noisy only when a pump runs | supply dip or electrical noise | Power the ESP32 from USB and the pumps from the adapter (common ground); 470 µF across 5V and GND. |
| Always RO with "TDS probe check failed" | TDS signal under 150 mV | Check the probe plug and that TDS A goes to GPIO34. Use water above 100 ppm; very pure water reads too low. |
| TDS jumps around | bubbles, pump noise, or probe touching the tank | Keep the probe off the walls and away from the pumps; let bubbles clear. |
| Clarity changes with room lights | optical sensor seeing ambient light | Shade it with black tape or a tube and re-run `c`. |
| A relay stays on, or both click at power-up | relay wiring option or wrong polarity | Two wirings exist: (A) jumper removed, VCC 3V3, JD-VCC 5V; (B) jumper ON, VCC 5V (what worked on our module). If a relay is on exactly when it should be off, set `RELAY_ACTIVE_LOW` to `false`. |
| Relay LED lights but no click, pump silent | relay coil has no power | With the jumper removed, JD-VCC must go to the 5 V rail and the relay GND to the adapter GND. Simplest fix: jumper ON and relay VCC on 5V. |
| Both pumps run, or a pump runs with no command | pump red lead on COM or NC | 5 V goes on COM, the pump's red lead on **NO**. |
| Relay clicks, no water | pump not submerged, air lock, or kinked tube | Keep the pump fully under water; squeeze the tube to clear air. |
| "No flow: refill inlet tank" | empty tank, or flow too low for the YF-S201 | Refill. If the sensor barely counts, set `USE_FLOW_SENSOR` to `false`. |
| ESP32 resets when a pump starts | 5 V rail dip | Add a 470 µF capacitor across 5V and GND; use short, thick wires from the adapter. |
| Upload fails | driver, cable or boot mode | Install the CP210x or CH340 driver, use a data cable, hold BOOT during "Connecting…". If it stops partway ("serial data stream stopped"): Upload Speed 115200, unplug the pump adapter, short cable. |
| TDS reads far too low or too high | sensor saturated, or a bad correction factor `k` | Pin near 2.3 V means over range (above 1000 ppm). Send `x`, then calibrate with `p<ppm>` in a 400 to 700 ppm solution. |
| Temperature shows 25.0 C with a "?" | DS18B20 not detected | Red 3V3, black GND, yellow GPIO4, pull-up (about 4.7 kΩ) from GPIO4 to 3V3. |
| Clarity stays 100% out of water | air is "clear" to the sensor | Normal. Test in water with a few drops of milk. |
| Wi-Fi slow to appear | phone scanning and connectivity check | Wait about 10 s after power-up, turn mobile data off, forget and rejoin `JalSense-Demo`. |
| Phone can't open the dashboard | phone using mobile data | Join `JalSense-Demo`, turn off mobile data, open `http://192.168.4.1`. |
