/*
  ============================================================================
  JalSense: smart RO retrofit controller (round-two prototype firmware)
  ============================================================================
  Board    : ESP32 DevKit V1 (ESP32-WROOM-32, 30-pin)
  Core     : Arduino-ESP32 2.x or 3.x  (Boards Manager: "esp32 by Espressif")
  Libraries: OneWire, DallasTemperature, Adafruit SSD1306, Adafruit GFX,
             Adafruit ADS1X15 (install all five from Library Manager)

  Why an external ADC: the ESP32's own ADC is inaccurate below ~150 mV, which
  is exactly where low-TDS water reads (50 ppm is ~0.12 V) and where a dry
  probe reads. The ADS1115 (16-bit) measures both correctly, so the decision
  and the "probe out of water" fail-safe can be trusted.

  What it does
    1. Reads incoming water: TDS (ppm, temperature-compensated), clarity
       (turbidity sensor) and temperature, once a second.
    2. Decides the path: BYPASS (UF + UV only) when water is within limits,
       otherwise RO. Any sensor fault or lock mode forces RO (fail-safe).
    3. On a button press (or the phone dashboard), opens the chosen valve,
       runs the pump, counts litres with the flow sensor and stops at the
       target volume. Stops early if no flow is detected (dry-run protection).
    4. Shows everything on the OLED and on a phone dashboard served over the
       ESP32's own Wi-Fi hotspot (no internet needed at the venue).

  Serial monitor (115200 baud, "Newline") calibration commands
    h          help
    c          clear-water calibration: probe in CLEAR water, saves clarity 100%
    p<ppm>     TDS calibration against a TDS pen, e.g.  p180
    r          reset flow pulse counter (before the flow calibration)
    l<mL>      flow calibration: after r, dispense into a measuring jug until
               roughly 1 L, then send the volume collected, e.g.  l960
    s          print current readings
    x          reset calibration to defaults
  Calibration values are saved in flash and survive power cycles.

  Pin map: see the JalSense round-two kit page (schematic + wiring table).
  ============================================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>

// ----------------------------- Pins ----------------------------------------
// Analog sensors go to the ADS1115 (I2C 0x48), not to ESP32 pins:
const int ADS_CH_TDS       = 0;   // A0  TDS board AOUT (0-2.3 V), board on 3V3
const int ADS_CH_TURB      = 1;   // A1  turbidity AOUT via 10k/12k divider
const int PIN_TEMP         = 4;   // DS18B20 data, 4.7k pull-up to 3V3
const int PIN_FLOW         = 27;  // YF-S201 pulse via 10k/18k divider
const int PIN_VALVE_BYPASS = 25;  // MOSFET gate -> valve A (UF+UV bypass path)
const int PIN_VALVE_RO     = 26;  // MOSFET gate -> valve B (RO path)
const int PIN_PUMP         = 32;  // MOSFET gate -> 12 V pump
const int PIN_BUTTON       = 13;  // Dispense push button to GND
const int PIN_LOCK         = 23;  // Lock-mode toggle to GND (closed = always RO)
// I2C bus: SDA = GPIO21, SCL = GPIO22. OLED SSD1306 at 0x3C, ADS1115 at 0x48 (ADDR to GND)

// ----------------------------- Decision limits -----------------------------
const float TDS_LIMIT_PPM     = 300.0;  // at or above -> RO  (BIS IS 10500 acceptable: 500)
const float TDS_MIN_VALID_PPM = 20.0;   // below -> probe dry/unplugged -> RO (fail-safe)
const float CLOUDY_RATIO      = 0.85;   // clarity below 85% of clear-water reading -> RO
const float TURB_MIN_VALID_MV = 300.0;  // sensor below 0.3 V -> unplugged/fault -> RO
const float TURB_DIVIDER      = (10.0 + 12.0) / 12.0;  // measured mV -> sensor mV

// ----------------------------- Dispense ------------------------------------
const float         DISPENSE_LITRES = 0.25;   // per button press
const unsigned long VALVE_LEAD_MS   = 200;    // open valve before pump starts
const unsigned long VALVE_LAG_MS    = 300;    // keep valve open after pump stops
const unsigned long NO_FLOW_MS      = 3000;   // no pulses for 3 s -> stop (empty tank)
const unsigned long MAX_PUMP_MS     = 20000;  // absolute safety cap
const float         RO_REJECT_FRACTION = 0.75; // assumed RO reject share (inside 50-80%)

// ----------------------------- Hotspot -------------------------------------
const char* AP_SSID = "JalSense-Demo";
const char* AP_PASS = "jalsense123";          // at least 8 characters

// ----------------------------- Calibration (saved in flash) ----------------
float turbClearMv    = 4100.0;  // sensor mV in clear water   (command: c)
float tdsK           = 1.0;     // TDS correction factor       (command: p<ppm>)
float pulsesPerLitre = 450.0;   // YF-S201 nominal             (commands: r, l)

// ----------------------------- Objects -------------------------------------
Adafruit_SSD1306  oled(128, 64, &Wire, -1);
Adafruit_ADS1115  ads;
OneWire           oneWire(PIN_TEMP);
DallasTemperature tempSensor(&oneWire);
WebServer         server(80);
Preferences       prefs;
bool              oledOk = false;
bool              adsOk  = false;

// ----------------------------- State ---------------------------------------
enum Path   { PATH_BYPASS, PATH_RO };
enum Reason { R_CLEAN, R_HIGH_TDS, R_CLOUDY, R_LOCK, R_FAULT_TDS, R_FAULT_TURB };
enum State  { IDLE, OPENING, PUMPING, CLOSING };

struct Readings {
  float tempC   = 25.0;
  bool  tempOk  = false;
  float tdsRaw  = 0;     // before tdsK
  float tdsPpm  = 0;
  float turbMv  = 0;     // sensor-side millivolts
  float clarity = 0;     // % of clear-water reading
};

Readings now;
Path     nextPath   = PATH_RO;   // what would happen if dispensed now
Reason   nextReason = R_FAULT_TDS;

State         state = IDLE;
Path          activePath   = PATH_RO;
Reason        activeReason = R_FAULT_TDS;
unsigned long stateSince = 0;
uint32_t      startPulses = 0, lastPulseCount = 0;
unsigned long lastPulseAt = 0;
float         dispensedL = 0;
String        lastError = "";

float litresDispensed = 0;   // all water through the system this session
float litresSaved     = 0;   // RO reject avoided by bypassing
float litresReused    = 0;   // RO reject captured in the reuse tank

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

// Median of n ADS1115 readings in millivolts (rejects spikes).
// Returns 0 if the ADC is missing, which the decision treats as a sensor fault.
float medianMv(int channel, int n) {
  if (!adsOk) return 0;
  const int MAXN = 31;
  float buf[MAXN];
  if (n > MAXN) n = MAXN;
  for (int i = 0; i < n; i++) buf[i] = ads.computeVolts(ads.readADC_SingleEnded(channel)) * 1000.0;
  for (int i = 1; i < n; i++) {            // insertion sort
    float v = buf[i]; int j = i - 1;
    while (j >= 0 && buf[j] > v) { buf[j + 1] = buf[j]; j--; }
    buf[j + 1] = v;
  }
  float m = buf[n / 2];
  return m < 0 ? 0 : m;
}

// The safety-critical rule. Order matters: lock and faults are checked first.
Path decide(const Readings& r, Reason& why) {
  if (digitalRead(PIN_LOCK) == LOW)          { why = R_LOCK;       return PATH_RO; }
  if (r.tdsPpm < TDS_MIN_VALID_PPM)          { why = R_FAULT_TDS;  return PATH_RO; }
  if (r.turbMv < TURB_MIN_VALID_MV)          { why = R_FAULT_TURB; return PATH_RO; }
  if (r.tdsPpm >= TDS_LIMIT_PPM)             { why = R_HIGH_TDS;   return PATH_RO; }
  if (r.turbMv < turbClearMv * CLOUDY_RATIO) { why = R_CLOUDY;     return PATH_RO; }
  why = R_CLEAN;
  return PATH_BYPASS;
}

// ----------------------------- Sensors -------------------------------------
void sampleSensors() {
  // DS18B20 runs non-blocking: read the conversion started last second, then start the next
  float t = tempSensor.getTempCByIndex(0);
  if (t != DEVICE_DISCONNECTED_C && t > -10 && t < 60) { now.tempC = t; now.tempOk = true; }
  else { now.tempC = 25.0; now.tempOk = false; }       // fall back to 25 C
  tempSensor.requestTemperatures();

  // TDS: DFRobot transfer curve with temperature compensation
  float v    = medianMv(ADS_CH_TDS, 15) / 1000.0;
  float coef = 1.0 + 0.02 * (now.tempC - 25.0);
  float cv   = v / coef;
  now.tdsRaw = (133.42 * cv * cv * cv - 255.86 * cv * cv + 857.39 * cv) * 0.5;
  if (now.tdsRaw < 0) now.tdsRaw = 0;
  now.tdsPpm = now.tdsRaw * tdsK;

  // Turbidity: higher voltage = clearer water
  now.turbMv  = medianMv(ADS_CH_TURB, 15) * TURB_DIVIDER;
  now.clarity = turbClearMv > 0 ? 100.0 * now.turbMv / turbClearMv : 0;

  nextPath = decide(now, nextReason);
}

// ----------------------------- Dispense state machine ----------------------
void allOff() {
  digitalWrite(PIN_PUMP, LOW);
  digitalWrite(PIN_VALVE_BYPASS, LOW);
  digitalWrite(PIN_VALVE_RO, LOW);
}

bool startDispense() {
  if (state != IDLE) return false;
  sampleSensors();                                  // decide on fresh readings
  activePath = decide(now, activeReason);           // decision is locked for this cycle
  lastError = "";
  dispensedL = 0;
  digitalWrite(activePath == PATH_BYPASS ? PIN_VALVE_BYPASS : PIN_VALVE_RO, HIGH);
  state = OPENING;
  stateSince = millis();
  Serial.printf("Dispense -> %s (%s)\n", pathName(activePath), reasonText(activeReason));
  return true;
}

void runDispense() {
  unsigned long t = millis();
  switch (state) {
    case IDLE:
      break;

    case OPENING:                                   // valve opens before the pump starts
      if (t - stateSince >= VALVE_LEAD_MS) {
        startPulses = flowPulses;
        lastPulseCount = startPulses;
        lastPulseAt = t;
        digitalWrite(PIN_PUMP, HIGH);
        state = PUMPING;
        stateSince = t;
      }
      break;

    case PUMPING: {
      uint32_t p = flowPulses;
      if (p != lastPulseCount) { lastPulseCount = p; lastPulseAt = t; }
      dispensedL = (p - startPulses) / pulsesPerLitre;
      bool done    = dispensedL >= DISPENSE_LITRES;
      bool noFlow  = (t - lastPulseAt) > NO_FLOW_MS;
      bool timeout = (t - stateSince) > MAX_PUMP_MS;
      if (done || noFlow || timeout) {
        digitalWrite(PIN_PUMP, LOW);                // pump stops first
        if (noFlow)       lastError = "No flow: refill inlet tank";
        else if (timeout) lastError = "Timeout: check tubing";
        state = CLOSING;
        stateSince = t;
      }
      break;
    }

    case CLOSING:                                   // valve closes after pressure drops
      if (t - stateSince >= VALVE_LAG_MS) {
        digitalWrite(PIN_VALVE_BYPASS, LOW);
        digitalWrite(PIN_VALVE_RO, LOW);
        dispensedL = (flowPulses - startPulses) / pulsesPerLitre;
        litresDispensed += dispensedL;
        if (activePath == PATH_BYPASS)
          litresSaved += dispensedL * RO_REJECT_FRACTION / (1.0 - RO_REJECT_FRACTION);
        else
          litresReused += dispensedL * RO_REJECT_FRACTION;
        Serial.printf("Done: %.3f L via %s. Saved %.2f L, reused %.2f L. %s\n",
                      dispensedL, pathName(activePath), litresSaved, litresReused,
                      lastError.c_str());
        state = IDLE;
      }
      break;
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
  if (digitalRead(PIN_LOCK) == LOW) { oled.setCursor(98, 0); oled.print("LOCK"); }

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
    if (!adsOk) oled.print("ADS1115 missing");
    else if (lastError.length()) oled.print(lastError.substring(0, 21));
    else oled.printf("Saved %.1fL Reuse %.1fL", litresSaved, litresReused);
  } else {
    oled.printf("Dispensing: %s", pathName(activePath));
    oled.setCursor(0, 44);
    oled.printf("%.2f / %.2f L", dispensedL, DISPENSE_LITRES);
    oled.setCursor(0, 54);
    oled.print(reasonShort(activeReason));
  }
  oled.display();
}

// ----------------------------- Phone dashboard -----------------------------
const char PAGE[] PROGMEM = R"rawliteral(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>JalSense</title>
<style>body{font-family:system-ui,sans-serif;margin:0;background:#0e2a38;color:#eef4f3}
main{max-width:480px;margin:auto;padding:16px}h1{font-size:22px;margin:4px 0 12px}
.big{font-size:44px;font-weight:700;margin:4px 0}.card{background:#163a4b;border-radius:14px;padding:14px;margin:10px 0}
.row{display:flex;gap:10px}.row .card{flex:1}.lbl{font-size:13px;color:#bfd3db;text-transform:uppercase;letter-spacing:1px}
.val{font-size:28px;font-weight:600}#path.BYPASS{color:#5fc4d6}#path.RO{color:#f0a36b}
button{width:100%;font-size:20px;padding:14px;border:0;border-radius:12px;background:#5fc4d6;color:#0e2a38;font-weight:700}
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
</main><script>
async function tick(){try{const d=await (await fetch('/data')).json();
const p=document.getElementById('path');p.textContent=d.path;p.className=d.path;
why.textContent=d.reason;tds.textContent=Math.round(d.tds)+' ppm';cla.textContent=Math.round(d.clarity)+'%';
tmp.textContent=d.temp.toFixed(1)+'°C';sav.textContent=d.saved.toFixed(2)+' L';reu.textContent=d.reused.toFixed(2)+' L';
st.textContent=d.state=='IDLE'?'Ready':'Dispensing via '+d.active+': '+d.dispensed.toFixed(2)+' L';err.textContent=d.error;}catch(e){}}
setInterval(tick,700);tick();</script></body></html>)rawliteral";

const char* stateName(State s) {
  switch (s) { case IDLE: return "IDLE"; case OPENING: return "OPENING";
               case PUMPING: return "PUMPING"; case CLOSING: return "CLOSING"; }
  return "";
}

void handleData() {
  char json[512];
  snprintf(json, sizeof(json),
    "{\"tds\":%.1f,\"temp\":%.2f,\"tempOk\":%s,\"clarity\":%.1f,\"turbMv\":%.0f,"
    "\"path\":\"%s\",\"reason\":\"%s\",\"lock\":%s,\"state\":\"%s\",\"active\":\"%s\","
    "\"dispensed\":%.3f,\"total\":%.3f,\"saved\":%.3f,\"reused\":%.3f,\"error\":\"%s\"}",
    now.tdsPpm, now.tempC, now.tempOk ? "true" : "false", now.clarity, now.turbMv,
    pathName(nextPath), reasonText(nextReason), digitalRead(PIN_LOCK) == LOW ? "true" : "false",
    stateName(state), pathName(activePath), dispensedL, litresDispensed,
    litresSaved, litresReused, lastError.c_str());
  server.send(200, "application/json", json);
}

void handleDispense() {
  bool ok = startDispense();
  server.send(ok ? 200 : 409, "text/plain", ok ? "started" : "busy");
}

// ----------------------------- Calibration over Serial ---------------------
void saveCal() {
  prefs.putFloat("turbClear", turbClearMv);
  prefs.putFloat("tdsK", tdsK);
  prefs.putFloat("ppl", pulsesPerLitre);
}

void printReadings() {
  Serial.printf("TDS %.0f ppm (raw %.0f, k=%.3f) | temp %.2f C%s | turb %.0f mV, clarity %.0f%% "
                "| flow pulses %lu (%.0f/L) | next: %s (%s)\n",
                now.tdsPpm, now.tdsRaw, tdsK, now.tempC, now.tempOk ? "" : " (sensor missing, using 25)",
                now.turbMv, now.clarity, (unsigned long)flowPulses, pulsesPerLitre,
                pathName(nextPath), reasonText(nextReason));
}

void printHelp() {
  Serial.println(F("Commands: c = clear-water cal | p<ppm> = TDS cal vs pen | r = reset pulses | "
                   "l<mL> = flow cal | s = readings | x = defaults | h = help"));
}

void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;
  char c = cmd.charAt(0);
  if (c == 'c') {
    sampleSensors();
    turbClearMv = now.turbMv; saveCal();
    Serial.printf("Clear-water reference saved: %.0f mV\n", turbClearMv);
  } else if (c == 'p') {
    float pen = cmd.substring(1).toFloat();
    sampleSensors();
    if (pen > 0 && now.tdsRaw > 1) {
      tdsK = pen / now.tdsRaw; saveCal();
      Serial.printf("TDS factor saved: k = %.3f\n", tdsK);
    } else Serial.println("Use p<pen reading>, with the probe in water, e.g. p180");
  } else if (c == 'r') {
    flowPulses = 0; Serial.println("Pulse counter reset. Dispense ~1 L into a jug, then send l<mL>");
  } else if (c == 'l') {
    float ml = cmd.length() > 1 ? cmd.substring(1).toFloat() : 1000.0;  // l or l<mL>
    if (flowPulses > 50 && ml >= 100) {
      pulsesPerLitre = flowPulses / (ml / 1000.0); saveCal();
      Serial.printf("Saved %.0f pulses per litre (%lu pulses for %.0f mL)\n",
                    pulsesPerLitre, (unsigned long)flowPulses, ml);
    } else Serial.println("Run water into a measuring jug first, then send l<mL collected>, e.g. l950");
  } else if (c == 's') {
    sampleSensors(); printReadings();
  } else if (c == 'x') {
    turbClearMv = 4100; tdsK = 1.0; pulsesPerLitre = 450; saveCal();
    Serial.println("Calibration reset to defaults");
  } else printHelp();
}

// ----------------------------- Setup / loop --------------------------------
void setup() {
  pinMode(PIN_VALVE_BYPASS, OUTPUT);
  pinMode(PIN_VALVE_RO, OUTPUT);
  pinMode(PIN_PUMP, OUTPUT);
  allOff();                                         // loads off before anything else

  Serial.begin(115200);
  delay(200);
  Serial.println("\nJalSense starting");

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LOCK, INPUT_PULLUP);
  pinMode(PIN_FLOW, INPUT);                         // external divider sets the level
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW), onFlowPulse, FALLING);

  prefs.begin("jalsense", false);
  turbClearMv    = prefs.getFloat("turbClear", 4100.0);
  tdsK           = prefs.getFloat("tdsK", 1.0);
  pulsesPerLitre = prefs.getFloat("ppl", 450.0);

  tempSensor.begin();
  tempSensor.setWaitForConversion(false);
  tempSensor.requestTemperatures();

  Wire.begin(21, 22);
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOk) Serial.println("OLED not found at 0x3C (check wiring, or try 0x3D)");
  adsOk = ads.begin(0x48);
  if (adsOk) {
    ads.setGain(GAIN_ONE);                          // +/-4.096 V range, 0.125 mV per bit
    ads.setDataRate(RATE_ADS1115_475SPS);
  } else {
    Serial.println("ADS1115 not found at 0x48: sensors read as FAULT, RO forced");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("Dashboard: join Wi-Fi '"); Serial.print(AP_SSID);
  Serial.print("' and open http://"); Serial.println(WiFi.softAPIP());

  server.on("/", []() { server.send_P(200, "text/html", PAGE); });
  server.on("/data", handleData);
  server.on("/dispense", HTTP_POST, handleDispense);
  server.begin();

  delay(800);                                       // first temperature conversion
  sampleSensors();
  printHelp();
  printReadings();
}

void loop() {
  static unsigned long lastSample = 0, lastDraw = 0;
  server.handleClient();
  handleSerial();
  handleButton();
  runDispense();

  unsigned long t = millis();
  if (state == IDLE && t - lastSample >= 1000) { lastSample = t; sampleSensors(); }
  if (t - lastDraw >= (state == IDLE ? 500UL : 200UL)) { lastDraw = t; updateDisplay(); }
}
