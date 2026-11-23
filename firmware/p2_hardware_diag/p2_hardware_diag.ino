/*
 * ============================================================================
 * PROJECT 2 (CSE 326): FULL NON-BLOCKING HARDWARE DIAGNOSTIC SKETCH
 * Hardware: ESP32-S3 N16R8 (44-Pin)
 * Tests:
 *   - I2C Scan + 1.3" SH1106 OLED (0x3C, SDA=GPIO 8, SCL=GPIO 9)
 *   - INMP441 I2S Mic (SCK=4, WS=5, SD=6) with 100ms timeout (NEVER blocks!)
 *   - DHT22 (GPIO 7) rate-limited every 2.5s
 *   - FC-51 Gates (GPIO 10, GPIO 14)
 *   - 4x Yellow Buttons (GPIO 38, 39, 1, 2) -> BTN1 tests 1s Fan Relay pulse
 *   - 3-Zone LEDs (GPIO 40, 41, 42)
 * ============================================================================
 */
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <U8g2lib.h>
#include <DHT.h>

#define I2S_SCK       4
#define I2S_WS        5
#define I2S_SD        6
#define PIN_DHT22     7
#define I2C_SDA       8
#define I2C_SCL       9
#define PIN_GATE_1    10
#define PIN_RELAY_FAN 13
#define PIN_GATE_2    14
#define LED_GREEN     40
#define LED_YELLOW    41
#define LED_RED       42
#define BTN_1         38
#define BTN_2         39
#define BTN_3         1
#define BTN_4         2

// Active-LOW verified on user's JQC3F-05VDC-C relay module (ON when GPIO 13 is LOW)
#define RELAY_ACTIVE_HIGH false

DHT dht(PIN_DHT22, DHT22);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

bool i2s_ready = false;
bool prev_oled_ok = false;
float cached_t = NAN, cached_h = NAN;
uint32_t last_dht_ms = 0;
uint8_t led_step = 0;

void setRelay(bool on) {
  digitalWrite(PIN_RELAY_FAN, (on == RELAY_ACTIVE_HIGH) ? HIGH : LOW);
}

void initI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 8000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false
  };
  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) == ESP_OK &&
      i2s_set_pin(I2S_NUM_0, &pins) == ESP_OK) {
    i2s_ready = true;
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(PIN_GATE_1, INPUT);
  pinMode(PIN_GATE_2, INPUT);
  pinMode(PIN_RELAY_FAN, OUTPUT);
  setRelay(false); // Fan strictly OFF on boot

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);
  pinMode(BTN_4, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  dht.begin();
  u8g2.begin();
  initI2S();

  Serial.println("\n=== PROJECT 2 (CSE 326) HARDWARE DIAGNOSTIC STARTED ===");
  Serial.println("Tip: Press BTN1 (GPIO 38) to test 1.0s Fan Relay pulse.");
}

void loop() {
  // 1. I2C probe (OLED 0x3C or fallback scan) + auto-reinit if hot-plugged
  Wire.beginTransmission(0x3C);
  bool oled_ok = (Wire.endTransmission() == 0);
  if (oled_ok && !prev_oled_ok) {
    u8g2.begin();
  }
  prev_oled_ok = oled_ok;
  uint8_t alt_addr = 0;
  if (!oled_ok) {
    for (uint8_t a = 1; a < 127; a++) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) { alt_addr = a; break; }
    }
  }

  // 2. Non-blocking I2S mic check (100ms timeout)
  int32_t mic_buf[128];
  size_t bytes_read = 0;
  long mic_peak = 0;
  if (i2s_ready) {
    i2s_read(I2S_NUM_0, mic_buf, sizeof(mic_buf), &bytes_read, pdMS_TO_TICKS(100));
    int samples = bytes_read / sizeof(int32_t);
    for (int i = 0; i < samples; i++) {
      long v = abs(mic_buf[i] >> 14);
      if (v > mic_peak) mic_peak = v;
    }
  }

  // 3. Rate-limited DHT22 read (every 2.5s)
  if (millis() - last_dht_ms >= 2500 || last_dht_ms == 0) {
    last_dht_ms = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) cached_t = t;
    if (!isnan(h)) cached_h = h;
  }

  int g1 = digitalRead(PIN_GATE_1);
  int g2 = digitalRead(PIN_GATE_2);
  int b1 = !digitalRead(BTN_1);
  int b2 = !digitalRead(BTN_2);
  int b3 = !digitalRead(BTN_3);
  int b4 = !digitalRead(BTN_4);

  if (b1) {
    Serial.println("[DIAG] BTN1 pressed -> 1.0s Fan Relay Pulse!");
    setRelay(true);
    delay(1000);
    setRelay(false);
  }

  // 4. Rolling LED heartbeat (1 LED per loop, zero long delay)
  digitalWrite(LED_GREEN,  (led_step % 3 == 0) ? HIGH : LOW);
  digitalWrite(LED_YELLOW, (led_step % 3 == 1) ? HIGH : LOW);
  digitalWrite(LED_RED,    (led_step % 3 == 2) ? HIGH : LOW);
  led_step++;

  // 5. OLED Update
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 11); u8g2.printf("P2 DIAG | OLED:%s", oled_ok ? "0x3C" : "ERR");
  u8g2.setCursor(0, 24); u8g2.printf("DHT:%.1fC %.0f%%", isnan(cached_t) ? -1.0 : cached_t, isnan(cached_h) ? -1.0 : cached_h);
  u8g2.setCursor(0, 37); u8g2.printf("MIC:%s PK:%ld", (bytes_read > 0) ? "OK" : "NO", mic_peak);
  u8g2.setCursor(0, 50); u8g2.printf("G1:%d G2:%d B:%d%d%d%d", g1, g2, b1, b2, b3, b4);
  u8g2.setCursor(0, 62); u8g2.printf("BTN1=TEST FAN RELAY");
  u8g2.sendBuffer();

  Serial.printf("[DIAG] OLED:%s(alt:0x%02X) | MIC_B:%u PK:%ld | DHT:%.1fC,%.0f%% | G1:%d G2:%d | BTN:%d%d%d%d\n",
                oled_ok ? "0x3C" : "ERR", alt_addr, (unsigned)bytes_read, mic_peak,
                isnan(cached_t) ? -1.0 : cached_t, isnan(cached_h) ? -1.0 : cached_h,
                g1, g2, b1, b2, b3, b4);
  delay(250);
}
