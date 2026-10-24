/*
  OLED_test: checks the 0.96" OLED on its own (no sensors, no pumps).
  Wiring: OLED SDA -> GPIO21, SCL -> GPIO22, VCC -> 3V3, GND -> GND.

  1. Open Serial Monitor at 115200.
  2. It scans the I2C bus and prints every address it finds (the OLED is 0x3C or 0x3D).
  3. Then it draws a border, a line of text and a counter on the OLED.

  Clean picture            -> the display and wiring are fine; the problem was elsewhere.
  Still noise / "no device" -> wiring contact (SDA, SCL, VCC, GND) or the module itself.
*/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const uint32_t I2C_HZ = 100000;      // try 50000 if it is still noisy
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
uint8_t oledAddr = 0x3C;

// ---- OLED chip type -------------------------------------------------------
// Many "0.96 inch SSD1306" modules really carry an SH1106 chip. The Adafruit SSD1306 library
// then shows garbage (one scrolling line + noise). OLED_IS_SH1106 = true writes the frame
// page by page with a 2-column offset, which is what the SH1106 needs. Set it to false for a
// real SSD1306 module.
const bool    OLED_IS_SH1106 = true;
const uint8_t OLED_ADDR      = 0x3C;

void oledFlush() {
  if (!OLED_IS_SH1106) { oled.display(); return; }
  uint8_t* buf = oled.getBuffer();
  for (uint8_t page = 0; page < 8; page++) {
    oled.ssd1306_command(0xB0 | page);   // page address
    oled.ssd1306_command(0x02);          // column low nibble: SH1106 RAM is 132 wide, glass starts at column 2
    oled.ssd1306_command(0x10);          // column high nibble
    for (uint8_t x = 0; x < 128; x += 16) {
      Wire.beginTransmission(OLED_ADDR);
      Wire.write((uint8_t)0x40);         // data follows
      Wire.write(buf + page * 128 + x, 16);
      Wire.endTransmission();
    }
  }
}


void scanBus() {
  Serial.println("Scanning I2C (SDA=21, SCL=22)...");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device at 0x%02X\n", a);
      if (a == 0x3C || a == 0x3D) oledAddr = a;
      found++;
    }
  }
  if (!found) Serial.println("  NO device found: check VCC, GND, SDA, SCL wires (loose contact?)");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(21, 22);
  Wire.setClock(I2C_HZ);
  scanBus();
  if (!oled.begin(SSD1306_SWITCHCAPVCC, oledAddr)) {
    Serial.println("OLED init failed");
    for (;;) delay(1000);
  }
  oled.clearDisplay(); oledFlush();
  Serial.printf("OLED started at 0x%02X, I2C %lu Hz\n", oledAddr, (unsigned long)I2C_HZ);
}

void loop() {
  static uint32_t n = 0;
  oled.clearDisplay();
  oled.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(2);
  oled.setCursor(10, 8);
  oled.print("OLED OK");
  oled.setTextSize(1);
  oled.setCursor(10, 32);
  oled.printf("count %lu", (unsigned long)n++);
  oled.setCursor(10, 46);
  oled.print("JalSense test");
  oledFlush();
  delay(500);
}
