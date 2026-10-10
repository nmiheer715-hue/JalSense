# FAQ

## Problem and impact

**Is RO water waste really that large?**
Yes. RO filtration sends 50–80% of its feed water out as reject: for every litre drunk, one to four litres go down the drain.

**Where does the "30 litres a day per home" estimate come from?**
A family purifying 10 litres a day for drinking and cooking, at a 75% reject rate, loses 3 litres for every litre drunk: 30 litres a day, about 11,000 litres a year. It's an estimate until a pilot measures it.

**Who would buy this?**
Households already running an RO on tanker or borewell water, apartment associations that buy tankers in bulk, and RO service companies that could fit it during annual maintenance visits.

**Why would RO companies support something that runs their membranes less?**
A membrane that runs fewer hours scales and fouls more slowly, which lowers service costs. The NGT also pushed for at least 60% water recovery, and installation is new work for technicians.

**Didn't the Supreme Court stay the NGT's RO order?**
It stayed the ban; it didn't say the waste is acceptable. JalSense doesn't depend on any ban: it's voluntary and saves the owner water.

**Which SDG targets?**
SDG 6.1 (safe drinking water) and 6.4 (water-use efficiency), plus SDG 3 (keeping minerals that over-purification strips out) and SDG 12 (responsible consumption).

## Safety and technical

**Can TDS alone tell you water is safe to drink?**
No, and JalSense doesn't claim it. TDS says whether RO's specific job, removing dissolved salts, is needed. UV and UF disinfection stay on in both paths for microbes. Arsenic, fluoride and nitrate don't show up in TDS, so homes in affected areas use lock mode.

**Why 300 ppm?**
IS 10500 gives 500 mg/L as the acceptable limit for drinking water, and WHO rates under 300 mg/L as excellent. 300 leaves a margin for sensor error of about ±10%. It's one constant in the firmware.

**What if a sensor fails?**
It fails safe. A TDS signal under 150 mV means a dry or unplugged probe, or water too pure to measure reliably: RO runs. A turbidity signal under 0.3 V means a disconnected sensor: RO. A missing temperature probe falls back to 25 °C compensation. If the bypass line shows no flow for 3 seconds, the pump stops.

**How accurate is the ESP32's ADC for this?**
It's accurate between about 150 mV and 2450 mV, so the design stays inside that range: dividers cap the turbidity signal at 2.25 V, the firmware uses factory-calibrated millivolt readings and a 21-sample median, and anything below 150 mV is treated as unknown and forces RO. A product version would add a 16-bit ADC to measure very low-TDS water too.

**Why pumps in the demo instead of valves?**
A home purifier already has water pressure, so a product uses two solenoid valves. A bench rig has no pressure, so two small pumps play the part of those valves. The sensing, decision logic and relay control are the same.

**How accurate are the sensors?**
The TDS sensor is about ±10% of full scale and is calibrated against a TDS pen. The low-cost turbidity sensor reliably detects visibly cloudy water but can't resolve 1 NTU; it's used relative to a clear-water baseline. A product version would use a nephelometric sensor.

**How would it fit an existing purifier?**
A technician fits it inline after the pre-filter: one valve feeds the RO membrane, the other routes water past it into the purifier's own UF and UV stages. Most purifiers already run on a 24 V supply, so a product version would use 24 V valves, switched by MOSFETs, powered from there.

**Don't purifiers already have a TDS controller?**
That's a blending valve that adds some raw water after RO to fix the taste. The RO still runs on every litre. JalSense decides whether RO should run at all and captures the reject when it does.

**What happens in a power cut?**
No power, no flow, same as the purifier. In a product the valves are normally closed, so nothing leaks or bypasses unpowered.

**Is RO reject water safe to reuse?**
For mopping, flushing and washing, yes. It carries concentrated salts, so long-term watering of plants with it isn't ideal.

## Cost and feasibility

**What does it cost?**
The bench prototype is about ₹2,400 in parts. A product swaps the pumps for valves and would cost less in volume. A tanker-dependent home could save close to one 12,000-litre tanker a year; the stronger case is apartments and longer membrane life.

**Does it need the internet?**
No. The phone dashboard runs over the ESP32's own Wi-Fi hotspot; no data leaves the home.

**What's next?**
A 16-bit ADC and a nephelometric turbidity sensor, a second conductivity sensor for redundancy, a pilot with lab tests to validate every bypass decision, and a 24 V valve version that fits inside a standard purifier cabinet.
