/*
  ============================================================================
  JalSense: smart RO retrofit controller (round-two prototype firmware)
  ============================================================================
  Board    : ESP32-WROOM-32 DevKit, 38-pin
  Core     : Arduino-ESP32 2.x or 3.x  (Boards Manager: "esp32 by Espressif")
  Libraries: OneWire, DallasTemperature, Adafruit SSD1306, Adafruit GFX
             (install all four from Library Manager)

  Prototype hardware
    - Analog TDS meter V1.0 (on 3.3 V) -> GPIO34
    - Turbidity module (on 5 V) -> 10k/10k divider -> GPIO35
    - DS18B20 waterproof probe -> GPIO4, 2k pull-up to 3.3 V
    - YF-S201 flow sensor (on 5 V) on the bypass line -> 10k / 20k divider -> GPIO27
    - 5 V dual relay module with optocoupler (active-LOW inputs)
        IN1 <- GPIO25 : pump A, bypass path (drinking-water jar)
        IN2 <- GPIO26 : pump B, RO path (RO / reject jar)
    - 0.96" SSD1306 OLED on I2C: SDA GPIO21, SCL GPIO22, address 0x3C
    - Dispense: the ESP32's BOOT button (GPIO0) or the phone dashboard
    - Lock mode: a jumper wire from GPIO23 to GND, or the phone dashboard

  What it does
    1. Reads the incoming water once a second: TDS (temperature-compensated),
       clarity (turbidity) and temperature.
    2. Decides the path: BYPASS when the water is within limits, otherwise RO.
       Lock mode or any sensor fault forces RO (fail-safe).
    3. On Dispense, runs the chosen pump until 250 mL has been delivered:
       measured by the flow sensor on the bypass path, by calibrated run time
       on the RO path. Stops early if the bypass line shows no flow for 3 s.
    4. Shows everything on the OLED and on a phone dashboard served from the
       ESP32's own Wi-Fi hotspot (no internet needed).

  Serial monitor (115200 baud, line ending "Newline") commands
    h          help
    s          print current readings
    c          clarity calibration: turbidity sensor in CLEAR water
    p<ppm>     TDS calibration against a TDS pen, e.g.  p180
    r          reset flow pulse counter (before flow calibration)
    l<mL>      flow calibration: after r, dispense on the bypass path into a
               measuring jug, then send the volume collected, e.g.  l960
    a / b      run pump A / pump B for exactly 10 s (into a measuring jug)
    ma<mL>     save pump A's 10-second volume, e.g.  ma210
    mb<mL>     save pump B's 10-second volume, e.g.  mb190
    x          reset all calibration to defaults
  Calibration is saved in flash and survives power cycles.
  ============================================================================
*/

// 1 = Wokwi simulation build: no Wi-Fi hotspot and no flow sensor
// (Wokwi has neither); both pumps are metered by run time.
// Leave at 0 for the real hardware.
#define JALSENSE_SIM 0

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#if !JALSENSE_SIM
#include <WiFi.h>
#include <WebServer.h>
#endif

#if JALSENSE_SIM
const bool USE_FLOW_SENSOR  = false;
#else
const bool USE_FLOW_SENSOR  = true;   // set false to meter both paths by run time
#endif
const bool RELAY_ACTIVE_LOW = true;   // optocoupler relay modules switch on when IN is LOW

// ----------------------------- Pins ----------------------------------------
const int PIN_TDS         = 34;  // ADC1_CH6, TDS board AOUT (0-2.3 V)
const int PIN_TURB        = 35;  // ADC1_CH7, turbidity AOUT through 10k/10k divider
const int PIN_TEMP        = 4;   // DS18B20 data
const int PIN_FLOW        = 27;  // YF-S201 pulses through 10k / 20k divider
const int PIN_PUMP_BYPASS = 25;  // relay IN1 -> pump A
const int PIN_PUMP_RO     = 26;  // relay IN2 -> pump B
const int PIN_BUTTON      = 0;   // BOOT button on the ESP32 board
const int PIN_LOCK        = 23;  // jumper to GND = lock mode (always RO)

// ----------------------------- Decision limits -----------------------------
const float TDS_LIMIT_PPM     = 300.0;  // at or above -> RO (IS 10500 acceptable limit: 500)
const float TDS_MIN_VALID_MV  = 150.0;  // below -> dry probe, or below the ESP32 ADC's
                                        // accurate range (~60 ppm) -> RO (fail-safe)
const float CLOUDY_RATIO      = 0.85;   // clarity below 85% of clear water -> RO
const float TURB_MIN_VALID_MV = 300.0;  // sensor below 0.3 V -> unplugged / fault -> RO
const float TURB_DIVIDER      = 2.0;    // 10k/10k divider: pin mV x 2 = sensor mV

// ----------------------------- Dispense ------------------------------------
const float         DISPENSE_ML        = 250.0;
const unsigned long NO_FLOW_MS         = 3000;   // bypass: no pulses for 3 s -> stop
const unsigned long MAX_PUMP_MS        = 20000;  // absolute safety cap
const unsigned long CAL_RUN_MS         = 10000;  // 'a' / 'b' calibration runs
const float         RO_REJECT_FRACTION = 0.75;   // assumed RO reject share (inside 50-80%)

// ----------------------------- Hotspot -------------------------------------
const char* AP_SSID = "JalSense-Demo";
const char* AP_PASS = "jalsense123";

// ----------------------------- Calibration (saved in flash) ----------------
float turbClearMv    = 4100.0;  // sensor mV in clear water        (c)
float tdsK           = 1.0;     // TDS correction factor           (p<ppm>)
float pulsesPerLitre = 450.0;   // YF-S201 nominal                 (r, l<mL>)
float mlPerSecA      = 20.0;    // pump A delivery rate            (a, ma<mL>)
float mlPerSecB      = 20.0;    // pump B delivery rate            (b, mb<mL>)

// ----------------------------- Objects -------------------------------------
Adafruit_SSD1306  oled(128, 64, &Wire, -1);
OneWire           oneWire(PIN_TEMP);
DallasTemperature tempSensor(&oneWire);
Preferences       prefs;
#if !JALSENSE_SIM
WebServer         server(80);
#endif
bool oledOk   = false;
bool softLock = false;   // lock mode set from the phone dashboard

// ----------------------------- State ---------------------------------------
enum Path   { PATH_BYPASS, PATH_RO };
enum Reason { R_CLEAN, R_HIGH_TDS, R_CLOUDY, R_LOCK, R_FAULT_TDS, R_FAULT_TURB };
enum State  { IDLE, PUMPING };

struct Readings {
  float tempC   = 25.0;
  bool  tempOk  = false;
  float tdsMv   = 0;     // TDS board voltage at the pin
  float tdsRaw  = 0;     // ppm before tdsK
  float tdsPpm  = 0;
  float turbMv  = 0;     // sensor-side millivolts
  float clarity = 0;     // % of clear-water reading
};

Readings now;
Path     nextPath   = PATH_RO;
Reason   nextReason = R_FAULT_TDS;

State         state        = IDLE;
Path          activePath   = PATH_RO;
Reason        activeReason = R_FAULT_TDS;
bool          calRun       = false;   // 'a' / 'b' timed calibration run
bool          meterByFlow  = false;   // this run is measured by the flow sensor
unsigned long pumpStart    = 0;
uint32_t      startPulses  = 0, lastPulseCount = 0;
unsigned long lastPulseAt  = 0;
float         dispensedMl  = 0;
String        lastError    = "";

float litresDispensed = 0;   // all water dispensed this session
float litresSaved     = 0;   // RO reject avoided by bypassing
float litresReused    = 0;   // RO reject captured for reuse

volatile uint32_t flowPulses = 0;
void IRAM_ATTR onFlowPulse() { flowPulses = flowPulses + 1; }

// ----------------------------- Helpers -------------------------------------
const char* pathName(Path p) { return p == PATH_BYPASS ? "BYPASS" : "RO"; }

const char* reasonText(Reason r) {
  switch (r) {
    case R_CLEAN:      return "Within limits: RO not needed";
    case R_HIGH_TDS:   return "TDS at or above 300 ppm";
    case R_CLOUDY:     return "Water is cloudy";
    case R_LOCK:       return "Lock mode: always RO";
    case R_FAULT_TDS:  return "TDS probe check failed (fail-safe RO)";
    case R_FAULT_TURB: return "Turbidity sensor check failed (fail-safe RO)";
  }
  return "";
}

const char* reasonShort(Reason r) {  // fits the OLED (21 chars)
  switch (r) {
    case R_CLEAN:      return "within limits";
    case R_HIGH_TDS:   return "high TDS";
    case R_CLOUDY:     return "cloudy water";
    case R_LOCK:       return "lock mode";
    case R_FAULT_TDS:  return "TDS fault>failsafe";
    case R_FAULT_TURB: return "turb fault>failsafe";
  }
  return "";
}

bool locked() { return softLock || digitalRead(PIN_LOCK) == LOW; }

void setPump(int pin, bool on) {
  digitalWrite(pin, (on != RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

void allPumpsOff() {
  setPump(PIN_PUMP_BYPASS, false);
  setPump(PIN_PUMP_RO, false);
}

// Median of n ADC readings in millivolts (rejects spikes from bubbles and pump noise)
float medianMv(int pin, int n) {
  const int MAXN = 31;
  uint32_t buf[MAXN];
  if (n > MAXN) n = MAXN;
  for (int i = 0; i < n; i++) { buf[i] = analogReadMilliVolts(pin); delay(2); }
  for (int i = 1; i < n; i++) {            // insertion sort
    uint32_t v = buf[i]; int j = i - 1;
    while (j >= 0 && buf[j] > v) { buf[j + 1] = buf[j]; j--; }
    buf[j + 1] = v;
  }
  return (float)buf[n / 2];
}

// The safety-critical rule. Lock mode and sensor faults are checked first,
// so a broken sensor can never lead to a bypass.
Path decide(const Readings& r, Reason& why) {
  if (locked())                              { why = R_LOCK;       return PATH_RO; }
  if (r.tdsMv < TDS_MIN_VALID_MV)            { why = R_FAULT_TDS;  return PATH_RO; }
  if (r.turbMv < TURB_MIN_VALID_MV)          { why = R_FAULT_TURB; return PATH_RO; }
  if (r.tdsPpm >= TDS_LIMIT_PPM)             { why = R_HIGH_TDS;   return PATH_RO; }
  if (r.turbMv < turbClearMv * CLOUDY_RATIO) { why = R_CLOUDY;     return PATH_RO; }
  why = R_CLEAN;
  return PATH_BYPASS;
}

// ----------------------------- Sensors -------------------------------------
void sampleSensors() {
  // DS18B20 runs non-blocking: read last second's conversion, then start the next
  float t = tempSensor.getTempCByIndex(0);
  if (t != DEVICE_DISCONNECTED_C && t > -10 && t < 60) { now.tempC = t; now.tempOk = true; }
  else { now.tempC = 25.0; now.tempOk = false; }
  tempSensor.requestTemperatures();

  // TDS: DFRobot transfer curve, compensated to 25 C
  now.tdsMv  = medianMv(PIN_TDS, 21);
  float v    = now.tdsMv / 1000.0;
  float coef = 1.0 + 0.02 * (now.tempC - 25.0);
  float cv   = v / coef;
  now.tdsRaw = (133.42 * cv * cv * cv - 255.86 * cv * cv + 857.39 * cv) * 0.5;
  if (now.tdsRaw < 0) now.tdsRaw = 0;
  now.tdsPpm = now.tdsRaw * tdsK;

  // Turbidity: higher voltage = clearer water
  now.turbMv  = medianMv(PIN_TURB, 21) * TURB_DIVIDER;
  now.clarity = turbClearMv > 0 ? 100.0 * now.turbMv / turbClearMv : 0;

  nextPath = decide(now, nextReason);
}

// ----------------------------- Pumping -------------------------------------
int pumpPin(Path p) { return p == PATH_BYPASS ? PIN_PUMP_BYPASS : PIN_PUMP_RO; }

void startPump(Path p) {
  activePath  = p;
  meterByFlow = USE_FLOW_SENSOR && p == PATH_BYPASS && !calRun;
  startPulses = flowPulses;
  lastPulseCount = startPulses;
  pumpStart = lastPulseAt = millis();
  dispensedMl = 0;
  lastError = "";
  setPump(pumpPin(p), true);
  state = PUMPING;
}

bool startDispense() {
  if (state != IDLE) return false;
  sampleSensors();                                  // decide on fresh readings
  Path p = decide(now, activeReason);               // locked for this dispense
  calRun = false;
  startPump(p);
  Serial.printf("Dispense -> %s (%s)\n", pathName(p), reasonText(activeReason));
  return true;
}

void startCalRun(Path p) {
  if (state != IDLE) return;
  calRun = true;
  startPump(p);
  Serial.printf("Calibration run: pump %s for 10 s. Measure the water collected.\n",
                p == PATH_BYPASS ? "A" : "B");
}

void runPump() {
  if (state != PUMPING) return;
  unsigned long t = millis();
  unsigned long elapsed = t - pumpStart;
  bool done = false, noFlow = false;

  if (calRun) {
    done = elapsed >= CAL_RUN_MS;
  } else if (meterByFlow) {
    uint32_t p = flowPulses;
    if (p != lastPulseCount) { lastPulseCount = p; lastPulseAt = t; }
    dispensedMl = (p - startPulses) * 1000.0 / pulsesPerLitre;
    done   = dispensedMl >= DISPENSE_ML;
    noFlow = (t - lastPulseAt) > NO_FLOW_MS;
  } else {
    float rate  = activePath == PATH_BYPASS ? mlPerSecA : mlPerSecB;
    dispensedMl = rate * elapsed / 1000.0;
    done = dispensedMl >= DISPENSE_ML;
  }
  bool timeout = elapsed > MAX_PUMP_MS;

  if (done || noFlow || timeout) {
    setPump(pumpPin(activePath), false);
    state = IDLE;
    if (calRun) { calRun = false; Serial.println("Calibration run finished."); return; }
    if (noFlow)       lastError = "No flow: refill inlet tank";
    else if (timeout) lastError = "Timeout: check tubing";
    float litres = dispensedMl / 1000.0;
    litresDispensed += litres;
    if (activePath == PATH_BYPASS)
      litresSaved += litres * RO_REJECT_FRACTION / (1.0 - RO_REJECT_FRACTION);
    else
      litresReused += litres * RO_REJECT_FRACTION;
    Serial.printf("Done: %.0f mL via %s. Saved %.2f L, reused %.2f L. %s\n",
                  dispensedMl, pathName(activePath), litresSaved, litresReused,
                  lastError.c_str());
  }
}

// ----------------------------- Button --------------------------------------
void handleButton() {
  static int stable = HIGH, lastRead = HIGH;
  static unsigned long changedAt = 0;
  int r = digitalRead(PIN_BUTTON);
  if (r != lastRead) { lastRead = r; changedAt = millis(); }
  if (millis() - changedAt > 40 && r != stable) {
    stable = r;
    if (stable == LOW) startDispense();             // pressed
  }
}

// ----------------------------- OLED ----------------------------------------
void updateDisplay() {
  if (!oledOk) return;
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("JalSense");
  if (locked()) { oled.setCursor(98, 0); oled.print("LOCK"); }

  oled.setCursor(0, 12);
  oled.printf("TDS %4.0fppm  %4.1fC%s", now.tdsPpm, now.tempC, now.tempOk ? "" : "?");
  oled.setCursor(0, 22);
  oled.printf("Clarity %3.0f%%", now.clarity);

  oled.setCursor(0, 34);
  if (state == IDLE) {
    oled.setTextSize(2);
    oled.print(nextPath == PATH_BYPASS ? "BYPASS" : "RO");
    oled.setTextSize(1);
    oled.setCursor(0, 52);
    if (lastError.length()) oled.print(lastError.substring(0, 21));
    else oled.printf("Saved %.1fL Reuse %.1fL", litresSaved, litresReused);
  } else if (calRun) {
    oled.print("Calibration run");
    oled.setCursor(0, 46);
    oled.printf("pump %s  %lus", activePath == PATH_BYPASS ? "A" : "B",
                (unsigned long)((millis() - pumpStart) / 1000));
  } else {
    oled.printf("Dispensing: %s", pathName(activePath));
    oled.setCursor(0, 44);
    oled.printf("%.0f / %.0f mL", dispensedMl, DISPENSE_ML);
    oled.setCursor(0, 54);
    oled.print(reasonShort(activeReason));
  }
  oled.display();
}

// ----------------------------- Phone dashboard -----------------------------
#if !JALSENSE_SIM
const char PAGE[] PROGMEM = R"rawliteral(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>JalSense</title>
<style>body{font-family:system-ui,sans-serif;margin:0;background:#0e2a38;color:#eef4f3}
main{max-width:480px;margin:auto;padding:16px}h1{font-size:22px;margin:4px 0 12px}
.big{font-size:44px;font-weight:700;margin:4px 0}.card{background:#163a4b;border-radius:14px;padding:14px;margin:10px 0}
.row{display:flex;gap:10px}.row .card{flex:1}.lbl{font-size:13px;color:#bfd3db;text-transform:uppercase;letter-spacing:1px}
.val{font-size:28px;font-weight:600}#path.BYPASS{color:#5fc4d6}#path.RO{color:#f0a36b}
button{width:100%;font-size:20px;padding:14px;border:0;border-radius:12px;background:#5fc4d6;color:#0e2a38;font-weight:700;margin-top:8px}
button.alt{background:#163a4b;color:#eef4f3;border:1px solid #5fc4d6}
#err{color:#f0a36b;min-height:20px}</style></head><body><main>
<h1>JalSense live</h1>
<div class="card"><div class="lbl">If dispensed now</div><div id="path" class="big">--</div><div id="why"></div></div>
<div class="row"><div class="card"><div class="lbl">TDS</div><div class="val" id="tds">--</div></div>
<div class="card"><div class="lbl">Clarity</div><div class="val" id="cla">--</div></div>
<div class="card"><div class="lbl">Temp</div><div class="val" id="tmp">--</div></div></div>
<div class="row"><div class="card"><div class="lbl">Saved</div><div class="val" id="sav">--</div></div>
<div class="card"><div class="lbl">Reused</div><div class="val" id="reu">--</div></div></div>
<div class="card"><div class="lbl">Status</div><div id="st">--</div><div id="err"></div></div>
<button onclick="fetch('/dispense',{method:'POST'})">Dispense 250 mL</button>
<button class="alt" id="lk" onclick="fetch('/lock',{method:'POST'})">Lock mode</button>
</main><script>
async function tick(){try{const d=await (await fetch('/data')).json();
const p=document.getElementById('path');p.textContent=d.path;p.className=d.path;
why.textContent=d.reason;tds.textContent=Math.round(d.tds)+' ppm';cla.textContent=Math.round(d.clarity)+'%';
tmp.textContent=d.temp.toFixed(1)+'°C';sav.textContent=d.saved.toFixed(2)+' L';reu.textContent=d.reused.toFixed(2)+' L';
st.textContent=d.state=='IDLE'?'Ready':'Dispensing via '+d.active+': '+Math.round(d.dispensed)+' mL';err.textContent=d.error;
lk.textContent=d.lockSoft?'Lock mode: ON (tap to turn off)':'Lock mode: off (tap to turn on)';}catch(e){}}
setInterval(tick,700);tick();</script></body></html>)rawliteral";

void handleData() {
  char json[560];
  snprintf(json, sizeof(json),
    "{\"tds\":%.1f,\"tdsMv\":%.0f,\"temp\":%.2f,\"tempOk\":%s,\"clarity\":%.1f,\"turbMv\":%.0f,"
    "\"path\":\"%s\",\"reason\":\"%s\",\"lock\":%s,\"lockSoft\":%s,\"state\":\"%s\",\"active\":\"%s\","
    "\"dispensed\":%.0f,\"total\":%.3f,\"saved\":%.3f,\"reused\":%.3f,\"error\":\"%s\"}",
    now.tdsPpm, now.tdsMv, now.tempC, now.tempOk ? "true" : "false", now.clarity, now.turbMv,
    pathName(nextPath), reasonText(nextReason), locked() ? "true" : "false",
    softLock ? "true" : "false", state == IDLE ? "IDLE" : "PUMPING", pathName(activePath),
    dispensedMl, litresDispensed, litresSaved, litresReused, lastError.c_str());
  server.send(200, "application/json", json);
}

void handleDispense() {
  bool ok = startDispense();
  server.send(ok ? 200 : 409, "text/plain", ok ? "started" : "busy");
}

void handleLock() {
  softLock = !softLock;
  sampleSensors();
  server.send(200, "text/plain", softLock ? "locked" : "unlocked");
}
#endif

// ----------------------------- Calibration over Serial ---------------------
void saveCal() {
  prefs.putFloat("turbClear", turbClearMv);
  prefs.putFloat("tdsK", tdsK);
  prefs.putFloat("ppl", pulsesPerLitre);
  prefs.putFloat("rateA", mlPerSecA);
  prefs.putFloat("rateB", mlPerSecB);
}

void printReadings() {
  Serial.printf("TDS %.0f ppm (pin %.0f mV, raw %.0f ppm, k=%.3f) | temp %.2f C%s | "
                "turbidity %.0f mV, clarity %.0f%% | flow pulses %lu (%.0f/L) | "
                "pump A %.1f mL/s, pump B %.1f mL/s | next: %s (%s)\n",
                now.tdsPpm, now.tdsMv, now.tdsRaw, tdsK, now.tempC,
                now.tempOk ? "" : " (probe missing, using 25)", now.turbMv, now.clarity,
                (unsigned long)flowPulses, pulsesPerLitre, mlPerSecA, mlPerSecB,
                pathName(nextPath), reasonText(nextReason));
}

void printHelp() {
  Serial.println(F("Commands: s readings | c clarity cal | p<ppm> TDS cal | r reset pulses | "
                   "l<mL> flow cal | a/b run pump A/B 10 s | ma<mL>/mb<mL> save pump rate | "
                   "x defaults | h help"));
}

void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;
  char c = cmd.charAt(0);
  if (c == 's') {
    sampleSensors(); printReadings();
  } else if (c == 'c') {
    sampleSensors();
    turbClearMv = now.turbMv; saveCal();
    Serial.printf("Clear-water reference saved: %.0f mV\n", turbClearMv);
  } else if (c == 'p') {
    float pen = cmd.substring(1).toFloat();
    sampleSensors();
    if (pen > 0 && now.tdsRaw > 1) {
      tdsK = pen / now.tdsRaw; saveCal();
      Serial.printf("TDS factor saved: k = %.3f\n", tdsK);
    } else Serial.println("Use p<pen reading> with the probe in water, e.g. p180");
  } else if (c == 'r') {
    flowPulses = 0;
    Serial.println("Pulse counter reset. Dispense on the bypass path into a jug, then send l<mL>");
  } else if (c == 'l') {
    float ml = cmd.length() > 1 ? cmd.substring(1).toFloat() : 1000.0;
    if (flowPulses > 50 && ml >= 100) {
      pulsesPerLitre = flowPulses / (ml / 1000.0); saveCal();
      Serial.printf("Saved %.0f pulses per litre (%lu pulses for %.0f mL)\n",
                    pulsesPerLitre, (unsigned long)flowPulses, ml);
    } else Serial.println("Collect at least 100 mL after r, then send l<mL>, e.g. l960");
  } else if (c == 'a' && cmd.length() == 1) {
    startCalRun(PATH_BYPASS);
  } else if (c == 'b' && cmd.length() == 1) {
    startCalRun(PATH_RO);
  } else if (c == 'm' && cmd.length() > 2 && (cmd.charAt(1) == 'a' || cmd.charAt(1) == 'b')) {
    float ml = cmd.substring(2).toFloat();
    if (ml > 10) {
      float rate = ml / (CAL_RUN_MS / 1000.0);
      if (cmd.charAt(1) == 'a') mlPerSecA = rate; else mlPerSecB = rate;
      saveCal();
      Serial.printf("Pump %c rate saved: %.1f mL/s\n", toupper(cmd.charAt(1)), rate);
    } else Serial.println("Use ma<mL> or mb<mL> after a 10-second run, e.g. ma210");
  } else if (c == 'x') {
    turbClearMv = 4100; tdsK = 1.0; pulsesPerLitre = 450; mlPerSecA = 20; mlPerSecB = 20;
    saveCal();
    Serial.println("Calibration reset to defaults");
  } else printHelp();
}

// ----------------------------- Setup / loop --------------------------------
void setup() {
  pinMode(PIN_PUMP_BYPASS, OUTPUT);
  pinMode(PIN_PUMP_RO, OUTPUT);
  allPumpsOff();                                    // pumps off before anything else

  Serial.begin(115200);
  delay(200);
  Serial.println("\nJalSense starting");

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LOCK, INPUT_PULLUP);
  if (USE_FLOW_SENSOR) {
    pinMode(PIN_FLOW, INPUT);                       // external divider sets the level
    attachInterrupt(digitalPinToInterrupt(PIN_FLOW), onFlowPulse, FALLING);
  }

  prefs.begin("jalsense", false);
  turbClearMv    = prefs.getFloat("turbClear", 4100.0);
  tdsK           = prefs.getFloat("tdsK", 1.0);
  pulsesPerLitre = prefs.getFloat("ppl", 450.0);
  mlPerSecA      = prefs.getFloat("rateA", 20.0);
  mlPerSecB      = prefs.getFloat("rateB", 20.0);

  tempSensor.begin();
  tempSensor.setWaitForConversion(false);
  tempSensor.requestTemperatures();

  Wire.begin(21, 22);
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOk) Serial.println("OLED not found at 0x3C (check wiring, or try 0x3D)");

#if !JALSENSE_SIM
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("Dashboard: join Wi-Fi '"); Serial.print(AP_SSID);
  Serial.print("' and open http://"); Serial.println(WiFi.softAPIP());
  server.on("/", []() { server.send_P(200, "text/html", PAGE); });
  server.on("/data", handleData);
  server.on("/dispense", HTTP_POST, handleDispense);
  server.on("/lock", HTTP_POST, handleLock);
  server.begin();
#endif

  delay(800);                                       // first temperature conversion
  sampleSensors();
  printHelp();
  printReadings();
}

void loop() {
  static unsigned long lastSample = 0, lastDraw = 0;
#if !JALSENSE_SIM
  server.handleClient();
#endif
  handleSerial();
  handleButton();
  runPump();

  unsigned long t = millis();
  if (state == IDLE && t - lastSample >= 1000) { lastSample = t; sampleSensors(); }
  if (t - lastDraw >= (state == IDLE ? 500UL : 200UL)) { lastDraw = t; updateDisplay(); }
}
