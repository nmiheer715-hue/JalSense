# Live dashboard: monitor the device from a phone

JalSense has a small website that the ESP32 serves **itself**. There is no cloud and no internet needed:
the ESP32 makes its own Wi-Fi hotspot, and the page loads from the board.

## Open it

1. Power the ESP32 and wait about 10 seconds.
2. On your phone or laptop, join Wi-Fi **`JalSense-Demo`** (password `jalsense123`).
3. Turn **mobile data off** (the hotspot has no internet, and phones otherwise try to leave it).
4. Open **`http://192.168.4.1`** in a browser (type `http://`, not `https://`).

The hotspot allows up to 2 devices at a time. If the phone says "no internet", choose "stay connected".

## What you see

![The JalSense phone dashboard: BYPASS (left) and RO (right)](images/dashboard-phone.png)

*These two images are rendered from the page in the firmware with sample readings (a clean-water state and a high-TDS state), not photographs of a phone. Replace them with real screenshots when you have them.*

The page refreshes about every 0.7 seconds.

| Card | Shows |
|---|---|
| **If dispensed now** | The current decision, BYPASS or RO, and the reason (for example "TDS at or above 300 ppm", "Water is cloudy", "Lock mode: always RO") |
| **TDS** | Temperature-compensated TDS in ppm |
| **Clarity** | Turbidity as a percentage of the saved clear-water reading |
| **Temp** | Water temperature in degrees C |
| **Saved** | Litres of RO reject avoided by taking the bypass path (an estimate) |
| **Reused** | Litres of RO reject captured for reuse (an estimate) |
| **Status** | Ready, or "Dispensing via ... : N mL", plus any error such as "No flow: refill inlet tank" |

## Buttons

| Button | Does |
|---|---|
| **Dispense 250 mL** | Runs the pump chosen by the current decision for 250 mL (same as the BOOT button) |
| **Lock mode** | Forces RO every time (for areas with arsenic or fluoride, which TDS cannot detect) |
| **Auto mode** | Starts or stops automatic dispensing (`JalSense_auto.ino`). The label shows its status and the litres done in this session |
| **Set TDS** | Only appears if the build has no TDS sensor (`USE_TDS_SENSOR = false`): type the TDS-pen reading |

## Reading the data from your own code

The page gets its numbers from `http://192.168.4.1/data`, which returns JSON:

```json
{"tds":167.0,"tdsMv":1180,"tdsManual":false,"temp":25.00,"tempOk":false,"clarity":100.0,"turbMv":4100,
 "path":"BYPASS","reason":"Within limits: RO not needed","lock":false,"lockSoft":false,
 "state":"IDLE","active":"RO","dispensed":0,"total":0.000,"saved":0.000,"reused":0.000,"error":"",
 "auto":true,"autoMl":0,"autoNote":"waiting"}
```

The controls are `POST` requests: `/dispense`, `/lock`, `/auto`, and `/tds?ppm=<value>`.
There is no login: anyone who knows the hotspot password can press the buttons, so change `AP_PASS` in the
sketch if the device is used where others can reach it.

## Limits, and what comes next

- It only works **near the device**, on its own hotspot. It is not an internet website, and nothing is stored:
  the saved and reused counters reset when the ESP32 restarts.
- For monitoring from anywhere (history, graphs, alerts), the next step would be to let the ESP32 join a
  home or site Wi-Fi network and send the same readings to a cloud service (for example over MQTT or HTTPS) with
  a hosted dashboard. That is not built yet.
