/*
  OLED_test2: finds out WHY the OLED is not answering.
  Every 2 seconds it
    1. checks whether GPIO21 / GPIO22 are shorted to GND (reads LOW = problem)
    2. scans the I2C bus with SDA/SCL the normal way, swapped, and on two other pin pairs
    3. if it finds an OLED, it draws a test picture on it
  Open the Serial Monitor at 115200. Wiggle the OLED wires while it runs and watch
  what changes. Keep the OLED wired as:  SDA -> GPIO21, SCL -> GPIO22, VCC -> 3V3, GND -> GND.
*/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

struct Pair { int sda, scl; const char* name; };
const Pair PAIRS[] = {
  {21, 22, "normal  SDA=21 SCL=22"},
  {22, 21, "SWAPPED SDA=22 SCL=21"},
  {16, 17, "other   SDA=16 SCL=17"},
  {17, 16, "other   SDA=17 SCL=16"},
};

void lineCheck() {
  pinMode(21, INPUT_PULLUP); pinMode(22, INPUT_PULLUP);
  delay(5);
  int a = digitalRead(21), b = digitalRead(22);
  Serial.printf("Line levels at rest: GPIO21 (SDA)=%s  GPIO22 (SCL)=%s   (both should be HIGH)\n",
                a ? "HIGH" : "LOW  <-- shorted to GND or pulled down", b ? "HIGH" : "LOW  <-- shorted to GND or pulled down");
}

// returns the address found, or 0
uint8_t scanPair(const Pair& p) {
  Wire.end();
  Wire.begin(p.sda, p.scl);
  Wire.setClock(100000);
  uint8_t found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf("   -> device at 0x%02X\n", a); if (!found) found = a; }
  }
  return found;
}

void drawDemo(const Pair& p, uint8_t addr) {
  static Adafruit_SSD1306* d = nullptr;
  if (!d) {
    Wire.end(); Wire.begin(p.sda, p.scl); Wire.setClock(100000);
    d = new Adafruit_SSD1306(128, 64, &Wire, -1);
    d->begin(SSD1306_SWITCHCAPVCC, addr);
  }
  static uint32_t n = 0;
  d->clearDisplay();
  d->drawRect(0, 0, 128, 64, SSD1306_WHITE);
  d->setTextColor(SSD1306_WHITE);
  d->setTextSize(2); d->setCursor(10, 8); d->print("OLED OK");
  d->setTextSize(1); d->setCursor(10, 34); d->printf("SDA=%d SCL=%d", p.sda, p.scl);
  d->setCursor(10, 48); d->printf("count %lu", (unsigned long)n++);
  d->display();
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n=== OLED_test2 ===");
}

void loop() {
  Serial.println("----------------------------------------");
  lineCheck();
  bool any = false;
  for (const Pair& p : PAIRS) {
    Serial.printf("Scan %s\n", p.name);
    uint8_t a = scanPair(p);
    if (a) {
      any = true;
      Serial.printf("   FOUND on %s (address 0x%02X)\n", p.name, a);
      if (a == 0x3C || a == 0x3D) { drawDemo(p, a); }
      break;
    }
  }
  if (!any) Serial.println("NOTHING found on any pin pair: check VCC/GND, solder joints, wires, or the OLED module.");
  delay(2000);
}
