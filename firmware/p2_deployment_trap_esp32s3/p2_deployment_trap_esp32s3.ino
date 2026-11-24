/*
 * ============================================================================
 * PROJECT 2 (CSE 326): WINGBEAT-TINYNET v6.0 CLEAN-REAL — 6-CLASS Q1 FIRMWARE
 * Hardware: ESP32-S3 N16R8 | Sequential Fan Gating (Fan 0% OFF during listening)
 *
 * 100% Untouched Real Audio Training (0% synthetic sin(), 0% artificial EQ/aug)
 * + 130 Hz 2nd-Order Butterworth HPF (strips mains, ceiling fan & lab rumble)
 * + Local-Envelope Spectral Whitening R[k] (device-neutral across live air,
 *   3D funnel, Android, iPhone, and laptop speakers)
 * + Unified 0.5x/1.5x Sub-Octave Fundamental Recovery (Bees + Small Speakers)
 *
 * 6-Class Multi-Genus Bio-Acoustic TinyML (24 -> 96 -> 48 -> 6, QAT INT8):
 *   Class 0: NOISE / ENV         (ESC-50 + Room + Sawmill + Speech       -> Fan OFF)
 *   Class 1: POLLINATOR (BEE)    (135-290 Hz + Optical 26-60ms Bee Veto  -> Fan OFF)
 *   Class 2: AEDES (DENGUE/CHIK) (Female Ae. aegypti & Ae. albopictus    -> Fan 5.0s!)
 *   Class 3: ANOPHELES (MALARIA) (Female An. dirus                       -> Fan 5.0s!)
 *   Class 4: CULEX (FILARIASIS)  (Female Cx. quinquefasciatus            -> Fan 5.0s!)
 *   Class 5: MALE MOSQUITO       (Non-biting Males 665-1020 Hz           -> Fan OFF)
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include <DHT.h>
#include <U8g2lib.h>
#include "wingbeat_q1_int8.h"

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
#define BTN_FAN_TEST  38
#define BTN_PAGE      39
#define BTN_SERIAL    1
#define BTN_RESET_NVS 2

// Active-LOW verified on JQC3F-05VDC-C relay module (ON when GPIO 13 is LOW)
#define RELAY_ACTIVE_HIGH   false

#define SAMPLE_RATE         8000
#define FFT_SAMPLES         512
#define MIN_ACTIVE_RMS      160.0f
#define MIN_ACTIVE_SNR      11.5f
#define VECTOR_STREAK_REQ   4
#define MALE_STREAK_REQ     4
#define POL_STREAK_REQ      4
#define VEC_PROB_MIN        0.72f
#define MAL_PROB_MIN        0.72f
#define POL_PROB_MIN        0.70f
#define FAN_COOLDOWN_MS     1500UL
#define OLED_REFRESH_MS     250UL
#define IR_STUCK_MS         150UL

// Exact 2nd-order Butterworth High-Pass Filter (fc = 130 Hz, fs = 8000 Hz)
// Bit-exact match with Python training pipeline (strips 50/60/120 Hz mains & ceiling fan)
static const double HP_B0 =  0.9303447533;
static const double HP_B1 = -1.8606895066;
static const double HP_B2 =  0.9303447533;
static const double HP_A1 = -1.8558317538;
static const double HP_A2 =  0.8655472595;

DHT dht(PIN_DHT22, DHT22);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Preferences prefs;

double vReal[FFT_SAMPLES], vImag[FFT_SAMPLES];
double specR[FFT_SAMPLES / 2];
double specE[FFT_SAMPLES / 2];
double specEp[FFT_SAMPLES / 2];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, FFT_SAMPLES, SAMPLE_RATE);

uint32_t cnt_aedes = 0, cnt_anoph = 0, cnt_culex = 0;
uint32_t cnt_pollinator = 0, cnt_male = 0, cnt_caught = 0;

float cached_t = 28.0f, cached_h = 72.0f;
uint32_t last_dht_ms = 0;
uint32_t fan_cooldown_until_ms = 0;
uint32_t last_oled_ms = 0;
uint32_t last_pol_count_ms = 0;
uint32_t last_male_count_ms = 0;
uint32_t status_hold_until_ms = 0;

uint8_t vector_streak = 0;
uint8_t male_streak = 0;
uint8_t pol_streak = 0;
float prev_vec_f0 = 0.0f;
float streak_anchor_f0 = 0.0f;
float sum_p_aed = 0.0f, sum_p_ano = 0.0f, sum_p_cul = 0.0f;
int held_status_code = 0; // 0=READY, 1=NOISE IGN, 2=BEE SAFE, 3=MALE IGN

// Non-blocking Edge-Triggered IR Gate State (auto-ignores desk-lamp/pot glare)
bool prev_g1_low = false;
bool prev_g2_low = false;
uint32_t g1_low_since_ms = 0;
uint32_t g2_low_since_ms = 0;
uint32_t g2_last_low_ms = 0;
bool ir1_stuck = false;
bool ir2_stuck = false;
float last_valid_dt_ms = 12.0f;
uint32_t last_ir1_pulse_ms = 0;
float ambient_rms_ema = 220.0f;

uint8_t oled_page = 0; // 0=Pro 3-Card Dashboard, 1=Live 16-Bar FFT, 2=Sensor Diagnostics
bool serial_verbose = true;
bool prev_oled_ok = false;

void setRelay(bool on) {
  digitalWrite(PIN_RELAY_FAN, (on == RELAY_ACTIVE_HIGH) ? HIGH : LOW);
}

static inline double sumEp(int k_start, int k_end_excl) {
  int ks = max(0, k_start);
  int ke = min(FFT_SAMPLES / 2, k_end_excl);
  double s = 0.0;
  for (int k = ks; k < ke; k++) s += specEp[k];
  return s;
}

static inline double maxRAround(int center_bin) {
  if (center_bin < 5 || center_bin > 247) return 0.0;
  int lo = max(5, center_bin - 1);
  int hi = min(247, center_bin + 1);
  double mx = 0.0;
  for (int k = lo; k <= hi; k++) {
    if (specR[k] > mx) mx = specR[k];
  }
  return mx;
}

void updateIRGates() {
  uint32_t now = millis();
  bool g1_low = (digitalRead(PIN_GATE_1) == LOW);
  bool g2_low = (digitalRead(PIN_GATE_2) == LOW);

  if (g1_low && !prev_g1_low) {
    g1_low_since_ms = now;
  } else if (!g1_low && prev_g1_low) {
    uint32_t dur = now - g1_low_since_ms;
    if (dur >= 2UL && dur <= 60UL) {
      last_valid_dt_ms = (float)dur;
      last_ir1_pulse_ms = now;
    }
    ir1_stuck = false;
  } else if (g1_low && (now - g1_low_since_ms > IR_STUCK_MS)) {
    ir1_stuck = true;
  } else if (!g1_low) {
    ir1_stuck = false;
  }
  prev_g1_low = g1_low;

  if (g2_low && !prev_g2_low) {
    g2_low_since_ms = now;
    g2_last_low_ms = now;
  } else if (!g2_low) {
    ir2_stuck = false;
  } else if (g2_low) {
    g2_last_low_ms = now;
    if (now - g2_low_since_ms > IR_STUCK_MS) {
      ir2_stuck = true;
    }
  }
  prev_g2_low = g2_low;
}

void runWingBeatQ1Probs(const float x_raw[P2_N_FEATURES], float probs_out[P2_N_CLASSES]) {
  float h0[P2_N_FEATURES], h1[P2_H1], h2[P2_H2], logits[P2_N_CLASSES];
  for (int i = 0; i < P2_N_FEATURES; i++) {
    float z = (x_raw[i] - P2_MU[i]) / (P2_SIGMA[i] + 1e-2f);
    if (z > 5.0f) z = 5.0f;
    if (z < -5.0f) z = -5.0f;
    h0[i] = z;
  }
  for (int j = 0; j < P2_H1; j++) {
    float acc = P2_B1[j];
    for (int i = 0; i < P2_N_FEATURES; i++) {
      acc += h0[i] * ((float)P2_W1_INT8[i * P2_H1 + j] * P2_W1_SCALE);
    }
    h1[j] = acc > 0.0f ? acc : 0.0f;
  }
  for (int j = 0; j < P2_H2; j++) {
    float acc = P2_B2[j];
    for (int i = 0; i < P2_H1; i++) {
      acc += h1[i] * ((float)P2_W2_INT8[i * P2_H2 + j] * P2_W2_SCALE);
    }
    h2[j] = acc > 0.0f ? acc : 0.0f;
  }
  float max_logit = -1e9f;
  for (int j = 0; j < P2_N_CLASSES; j++) {
    float acc = P2_B3[j];
    for (int i = 0; i < P2_H2; i++) {
      acc += h2[i] * ((float)P2_W3_INT8[i * P2_N_CLASSES + j] * P2_W3_SCALE);
    }
    logits[j] = acc;
    if (acc > max_logit) max_logit = acc;
  }
  float exp_sum = 0.0f;
  for (int j = 0; j < P2_N_CLASSES; j++) {
    probs_out[j] = expf(logits[j] - max_logit);
    exp_sum += probs_out[j];
  }
  for (int j = 0; j < P2_N_CLASSES; j++) {
    probs_out[j] /= max(exp_sum, 1e-6f);
  }
}

void showToastOLED(const char *title, const char *sub, uint16_t ms = 900) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(0, 0, 128, 64, 4);
  u8g2.drawRBox(6, 6, 116, 16, 2);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x13B_tf);
  int tw = u8g2.getStrWidth(title);
  u8g2.setCursor(max(8, (128 - tw) / 2), 19);
  u8g2.print(title);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_6x10_tf);
  int sw = u8g2.getStrWidth(sub);
  u8g2.setCursor(max(6, (128 - sw) / 2), 40);
  u8g2.print(sub);
  u8g2.drawHLine(16, 48, 96);
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor(21, 58);
  u8g2.print("ESP32-S3  512-FFT");
  u8g2.sendBuffer();
  delay(ms);
}

void drawFanIcon(int cx, int cy, uint8_t frame) {
  u8g2.setDrawColor(1);
  u8g2.drawCircle(cx, cy, 11);
  if ((frame & 1) == 0) {
    u8g2.drawBox(cx - 1, cy - 9, 3, 7);
    u8g2.drawBox(cx - 1, cy + 3, 3, 7);
    u8g2.drawBox(cx - 9, cy - 1, 7, 3);
    u8g2.drawBox(cx + 3, cy - 1, 7, 3);
  } else {
    u8g2.drawLine(cx - 6, cy - 6, cx + 6, cy + 6);
    u8g2.drawLine(cx - 6, cy - 5, cx + 5, cy + 6);
    u8g2.drawLine(cx - 5, cy - 6, cx + 6, cy + 5);
    u8g2.drawLine(cx + 6, cy - 6, cx - 6, cy + 6);
    u8g2.drawLine(cx + 5, cy - 6, cx - 6, cy + 5);
    u8g2.drawLine(cx + 6, cy - 5, cx - 5, cy + 6);
  }
  u8g2.drawDisc(cx, cy, 2);
}

void drawCaptureOLED(const char *header_txt, const char *detail_txt, uint32_t elapsed_ms, uint32_t total_ms, uint8_t anim_frame) {
  u8g2.clearBuffer();
  // Inverted top alert banner
  u8g2.setDrawColor(1);
  u8g2.drawRBox(0, 0, 128, 16, 2);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x13B_tf);
  int hw = u8g2.getStrWidth(header_txt);
  u8g2.setCursor(max(2, (128 - hw) / 2), 13);
  u8g2.print(header_txt);

  // Rotating fan icon + live countdown
  u8g2.setDrawColor(1);
  drawFanIcon(16, 33, anim_frame);

  float rem_s = (elapsed_ms < total_ms) ? ((float)(total_ms - elapsed_ms) / 1000.0f) : 0.0f;
  char tbuf[20];
  snprintf(tbuf, sizeof(tbuf), "FAN ON %.1fs", rem_s);
  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.setCursor(34, 31);
  u8g2.print(tbuf);

  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor(34, 43);
  u8g2.print(detail_txt);

  // Smooth rounded progress bar
  u8g2.drawRFrame(4, 51, 120, 11, 3);
  int fill_w = (int)((min(elapsed_ms, total_ms) * (uint32_t)116) / max((uint32_t)1, total_ms));
  if (fill_w >= 4) {
    u8g2.drawRBox(6, 53, fill_w, 7, 2);
  }
  u8g2.sendBuffer();
}

static void drawGenusCard(int cx, int cw, const char *label, uint32_t count) {
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(cx, 17, cw, 33, 3);
  u8g2.drawHLine(cx + 2, 27, cw - 4);

  u8g2.setFont(u8g2_font_5x8_tf);
  int lw = u8g2.getStrWidth(label);
  u8g2.setCursor(cx + (cw - lw) / 2, 25);
  u8g2.print(label);

  char nbuf[12];
  snprintf(nbuf, sizeof(nbuf), "%lu", (unsigned long)count);
  u8g2.setFont(u8g2_font_9x15B_tn);
  int nw = u8g2.getStrWidth(nbuf);
  u8g2.setCursor(cx + max(2, (cw - nw) / 2), 45);
  u8g2.print(nbuf);
}

void drawMainOLED(bool in_cooldown, float f0_corr, float e_rms, float dyn_floor, float snr_db) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);

  if (oled_page == 0) {
    // === PAGE 0: PRO 3-CARD GENUS DASHBOARD ===
    const char *status_txt = "READY";
    bool is_ready = true;
    bool active_hold = (millis() < status_hold_until_ms);
    if (in_cooldown) {
      status_txt = "SETTLE";
      is_ready = false;
    } else if (active_hold) {
      if (held_status_code == 2)      { status_txt = "BEE SAFE"; is_ready = false; }
      else if (held_status_code == 3) { status_txt = "MALE IGN"; is_ready = false; }
      else if (held_status_code == 1) { status_txt = "NOISE";    is_ready = false; }
    }

    // Inverted sleek top status bar (y=0..13)
    u8g2.drawRBox(0, 0, 128, 14, 2);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_6x10_tf);
    if (is_ready) {
      if (((millis() / 450UL) & 1UL) == 0) u8g2.drawDisc(6, 7, 2);
      else u8g2.drawCircle(6, 7, 2);
      u8g2.setCursor(12, 10);
      u8g2.print("READY");
    } else {
      u8g2.setCursor(4, 10);
      u8g2.print(status_txt);
    }

    // Live 5-bar micro audio VU meter inside header (x=68..87)
    float scale_step = max(80.0f, dyn_floor * 0.30f);
    int vu_lvl = constrain((int)(e_rms / scale_step), 0, 5);
    const int bar_h[5] = {2, 4, 6, 8, 10};
    for (int b = 0; b < 5; b++) {
      int bx = 68 + b * 4;
      if (b < vu_lvl) {
        u8g2.drawBox(bx, 12 - bar_h[b], 3, bar_h[b]);
      } else {
        u8g2.drawPixel(bx + 1, 11);
      }
    }

    // Live DHT22 temperature on far right (x=96..126)
    char tbuf[10];
    snprintf(tbuf, sizeof(tbuf), "%.1fC", cached_t);
    u8g2.setFont(u8g2_font_5x8_tf);
    int tw = u8g2.getStrWidth(tbuf);
    u8g2.setCursor(125 - tw, 10);
    u8g2.print(tbuf);

    // Center 3 Rounded Big-Number Vector Cards (41 + 2 + 42 + 2 + 41 = 128 px)
    drawGenusCard(0,  41, "AED", cnt_aedes);
    drawGenusCard(43, 42, "ANO", cnt_anoph);
    drawGenusCard(87, 41, "CUL", cnt_culex);

    // Clean Bottom Telemetry Strip (y=52..63)
    u8g2.setDrawColor(1);
    u8g2.drawHLine(0, 52, 128);
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(1, 62);  u8g2.printf("B:%lu", (unsigned long)cnt_pollinator);
    u8g2.setCursor(32, 62); u8g2.printf("M:%lu", (unsigned long)cnt_male);
    u8g2.setCursor(63, 62); u8g2.printf("C:%lu", (unsigned long)cnt_caught);
    if (ir1_stuck || ir2_stuck) {
      u8g2.drawRBox(94, 54, 34, 10, 1);
      u8g2.setDrawColor(0);
      u8g2.setCursor(96, 62); u8g2.print("IR:POT");
    } else {
      u8g2.setCursor(98, 62); u8g2.print("IR:OK");
    }
  } else if (oled_page == 1) {
    // === PAGE 1: LIVE 16-BAR REAL-TIME FFT SPECTRUM ANALYZER ===
    u8g2.drawRBox(0, 0, 128, 13, 2);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(3, 9);
    u8g2.printf("FFT %4.0fHz  SNR:%2.0f", f0_corr, snr_db);

    u8g2.setDrawColor(1);
    // 16 bars spanning bins 9..72 (140 Hz .. 1125 Hz, 4 bins per bar)
    for (int b = 0; b < 16; b++) {
      double mx = 0.0;
      int k0 = 9 + b * 4;
      for (int k = k0; k < k0 + 4 && k < 128; k++) {
        if (specR[k] > mx) mx = specR[k];
      }
      int h = constrain((int)((mx - 0.85) * 6.5), 1, 33);
      u8g2.drawBox(1 + b * 8, 49 - h, 6, h);
    }
    u8g2.drawHLine(0, 50, 128);
    // Highlight female mosquito band (bars 3..7 = 328..640 Hz)
    u8g2.drawBox(25, 51, 38, 2);

    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(1, 62);   u8g2.print("140");
    u8g2.setCursor(25, 62);  u8g2.print("[340-660]");
    u8g2.setCursor(107, 62); u8g2.print("1.1k");
  } else {
    // === PAGE 2: SENSOR & ADAPTIVE FLOOR DIAGNOSTICS ===
    u8g2.drawRBox(0, 0, 128, 13, 2);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(4, 9);
    u8g2.print("SENSORS & FLOOR  v6.0");

    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(2, 24); u8g2.printf("Temp:%.1fC  RH:%.0f%%", cached_t, cached_h);
    const char *s1 = ir1_stuck ? "POT!" : (prev_g1_low ? "HIT" : "OK");
    const char *s2 = ir2_stuck ? "POT!" : (prev_g2_low ? "HIT" : "OK");
    u8g2.setCursor(2, 36); u8g2.printf("IR1:%-4s   IR2:%-4s", s1, s2);

    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(2, 48); u8g2.printf("RMS:%4.0f FLR:%4.0f %3.0fHz", e_rms, dyn_floor, f0_corr);

    // Live RMS horizontal bar with dynamic floor marker tick (0..1600 scale)
    u8g2.drawFrame(2, 53, 124, 9);
    int rms_w = constrain((int)(e_rms * 120.0f / 1600.0f), 0, 120);
    if (rms_w > 0) u8g2.drawBox(4, 55, rms_w, 5);
    int flr_x = 4 + constrain((int)(dyn_floor * 120.0f / 1600.0f), 1, 119);
    u8g2.drawVLine(flr_x, 51, 13);
  }
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  pinMode(PIN_GATE_1, INPUT);
  pinMode(PIN_GATE_2, INPUT);
  setRelay(false);
  pinMode(PIN_RELAY_FAN, OUTPUT);
  setRelay(false);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  pinMode(BTN_FAN_TEST, INPUT_PULLUP);
  pinMode(BTN_PAGE, INPUT_PULLUP);
  pinMode(BTN_SERIAL, INPUT_PULLUP);
  pinMode(BTN_RESET_NVS, INPUT_PULLUP);

  // Clean v4 NVS namespace (6-class genus counters start fresh at 0)
  prefs.begin("trap_v4", false);
  cnt_aedes      = prefs.getUInt("aed", 0);
  cnt_anoph      = prefs.getUInt("ano", 0);
  cnt_culex      = prefs.getUInt("cul", 0);
  cnt_pollinator = prefs.getUInt("pol", 0);
  cnt_male       = prefs.getUInt("mal", 0);
  cnt_caught     = prefs.getUInt("cat", 0);

  dht.begin();
  u8g2.begin();
  showToastOLED("WingBeat v6.0", "100% Real QAT", 400);

  // 3-Zone LED Hardware Self-Test on Boot (Green -> Yellow -> Red)
  digitalWrite(LED_GREEN, HIGH);  delay(220); digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, HIGH); delay(220); digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, HIGH);    delay(220); digitalWrite(LED_RED, LOW);

  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
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
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  Serial.println("\n=== PROJECT 2 (CSE 326) WINGBEAT-TINYNET v6.0 CLEAN-REAL (130Hz HPF + WHITENED QAT INT8) STARTED ===");
}

void loop() {
  Wire.beginTransmission(0x3C);
  bool oled_ok = (Wire.endTransmission() == 0);
  if (oled_ok && !prev_oled_ok) { u8g2.begin(); }
  prev_oled_ok = oled_ok;

  updateIRGates();

  if (digitalRead(BTN_FAN_TEST) == LOW) {
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_YELLOW, HIGH);
    digitalWrite(LED_RED, HIGH);
    setRelay(true);
    uint32_t t_test0 = millis();
    uint8_t anim_f = 0;
    while (millis() - t_test0 < 1500UL) {
      drawCaptureOLED("FAN SELF-TEST", "Relay + 3 LEDs ON", millis() - t_test0, 1500UL, anim_f++);
      delay(140);
    }
    setRelay(false);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED, LOW);
    vector_streak = 0; male_streak = 0; pol_streak = 0;
    streak_anchor_f0 = 0.0f;
    sum_p_aed = 0.0f; sum_p_ano = 0.0f; sum_p_cul = 0.0f;
    fan_cooldown_until_ms = millis() + FAN_COOLDOWN_MS;
    i2s_zero_dma_buffer(I2S_NUM_0);
    last_oled_ms = 0;
  }
  if (digitalRead(BTN_PAGE) == LOW) {
    oled_page = (oled_page + 1) % 3;
    last_oled_ms = 0;
    delay(250);
  }
  if (digitalRead(BTN_SERIAL) == LOW) {
    serial_verbose = !serial_verbose;
    showToastOLED("Serial Log", serial_verbose ? "Status: ON" : "Status: OFF", 700);
    last_oled_ms = 0;
  }
  if (digitalRead(BTN_RESET_NVS) == LOW) {
    cnt_aedes = 0; cnt_anoph = 0; cnt_culex = 0;
    cnt_pollinator = 0; cnt_male = 0; cnt_caught = 0;
    prefs.putUInt("aed", 0); prefs.putUInt("ano", 0); prefs.putUInt("cul", 0);
    prefs.putUInt("pol", 0); prefs.putUInt("mal", 0); prefs.putUInt("cat", 0);
    held_status_code = 0; status_hold_until_ms = 0;
    showToastOLED("RESET DONE", "All Counts = 0", 900);
    last_oled_ms = 0;
  }

  if (millis() - last_dht_ms >= 2500 || last_dht_ms == 0) {
    last_dht_ms = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && t >= -10.0f && t <= 50.0f) cached_t = t;
    if (!isnan(h) && h >= 5.0f && h <= 100.0f) cached_h = h;
  }

  // 1. Acoustic Listening Phase (Fan 100% OFF)
  int32_t raw_buf[FFT_SAMPLES];
  size_t bytes_read = 0;
  esp_err_t i2s_err = i2s_read(I2S_NUM_0, raw_buf, sizeof(raw_buf), &bytes_read, pdMS_TO_TICKS(150));

  if (i2s_err != ESP_OK || bytes_read < sizeof(raw_buf)) {
    showToastOLED("MIC ERROR!", "Check INMP441 3.3V", 200);
    return;
  }

  double mean = 0.0;
  for (int i = 0; i < FFT_SAMPLES; i++) {
    vReal[i] = (double)(raw_buf[i] >> 14);
    vImag[i] = 0.0;
    mean += vReal[i];
  }
  mean /= FFT_SAMPLES;

  // 130 Hz 2nd-order Butterworth High-Pass Filter (bit-exact with Python hpf_130hz)
  // Strips 50/60/120 Hz mains hum, ceiling-fan motor rumble & low-frequency vibration
  double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
  double hpf_mean = 0.0;
  for (int i = 0; i < FFT_SAMPLES; i++) {
    double x0 = vReal[i] - mean;
    double y0 = HP_B0 * x0 + HP_B1 * x1 + HP_B2 * x2 - HP_A1 * y1 - HP_A2 * y2;
    vReal[i] = y0;
    hpf_mean += y0;
    x2 = x1; x1 = x0;
    y2 = y1; y1 = y0;
  }
  hpf_mean /= FFT_SAMPLES;

  double e_rms = 0.0;
  for (int i = 0; i < FFT_SAMPLES; i++) {
    vReal[i] -= hpf_mean;
    e_rms += vReal[i] * vReal[i];
  }
  e_rms = sqrt(e_rms / FFT_SAMPLES);

  FFT.windowing(FFTWindow::Hann, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  // Local-Envelope Spectral Whitening R[k] = spec[k] / flank_mean[k] (+-8 bins excluding +-2)
  // Cancels smooth microphone, 3D-printed funnel, Android, iPhone & laptop speaker EQ tilt!
  for (int k = 0; k < FFT_SAMPLES / 2; k++) {
    specR[k] = 0.0; specE[k] = 0.0; specEp[k] = 0.0;
  }
  for (int k = 5; k < 248; k++) {
    int lo1 = max(2, k - 8), hi1 = max(3, k - 2);
    int lo2 = min(253, k + 3), hi2 = min(255, k + 9);
    double sfl = 0.0;
    for (int j = lo1; j < hi1; j++) sfl += vReal[j];
    for (int j = lo2; j < hi2; j++) sfl += vReal[j];
    int cnt = max(1, (hi1 - lo1) + (hi2 - lo2));
    double rk = vReal[k] / (sfl / (double)cnt + 1e-6);
    specR[k] = rk;
    double ek = (rk > 1.35) ? (rk - 1.35) : 0.0;
    specE[k] = ek;
    specEp[k] = ek * ek;
  }

  // Universal DHT22 Thermal Spectral Bin Warping (+7.0 Hz/degC ref 28.0C)
  // Displays true live ambient (-10C..+50C) while clamping ectothermic flight warp to [10C, 45C]
  double t_eff = max(10.0, min(45.0, (double)cached_t));
  double df_t = 7.0 * (t_eff - 28.0);
  int bsh = (int)lround(df_t / ((double)SAMPLE_RATE / FFT_SAMPLES));
  auto clampK = [bsh](int k) -> int { return max(9, min(175, k + bsh)); };

  int k_lo = max(9, 9 + bsh);
  int k_hi = min(80, 68 + bsh);
  int band_len = max(2, k_hi - k_lo);

  double max_raw_bio = 0.0;
  for (int k = k_lo; k < k_hi; k++) {
    if (vReal[k] > max_raw_bio) max_raw_bio = vReal[k];
  }
  max_raw_bio += 1e-6;

  int peak_bin = k_lo;
  double best_R_cand = -1.0;
  for (int k = k_lo; k < k_hi; k++) {
    double rc = (vReal[k] >= 0.08 * max_raw_bio) ? specR[k] : 0.0;
    if (rc > best_R_cand) {
      best_R_cand = rc;
      peak_bin = k;
    }
  }
  double pk_R = specR[peak_bin];
  double pk_raw = vReal[peak_bin];
  double snr_db = 20.0 * log10(max(pk_R, 1.0));

  // Unified 0.5x / 1.5x Sub-Octave Fundamental Recovery in Whitened Space R[k]:
  //   - Snaps 2*f0-dominant Bees (280-570 Hz) back to true Bee f0 (140-285 Hz) -> BEE SAFE
  //   - Snaps small-speaker 2*f0-peaking Females (680-1218 Hz) back to true f0 (340-640 Hz)
  if (peak_bin >= clampK(19) && peak_bin <= clampK(78)) {
    int hb = peak_bin / 2;
    int h3b = (peak_bin * 3) / 2;
    if (hb >= clampK(9) && hb <= clampK(41) && h3b < 246) {
      int best_hb = max(9, hb - 1);
      for (int k = max(9, hb - 1); k <= min(80, hb + 1); k++) {
        if (specR[k] > specR[best_hb]) best_hb = k;
      }
      double r_half = specR[best_hb];
      double m_half = vReal[best_hb];

      int best_3h = max(9, h3b - 1);
      for (int k = max(9, h3b - 1); k <= min(246, h3b + 1); k++) {
        if (specR[k] > specR[best_3h]) best_3h = k;
      }
      double r_3h = specR[best_3h];
      double min_raw_frac = (peak_bin < clampK(42)) ? 0.14 : 0.03;
      if (r_half >= 2.3 && r_3h >= 2.2 && m_half >= min_raw_frac * pk_raw) {
        peak_bin = best_hb;
        pk_R = r_half;
        pk_raw = m_half;
        snr_db = 20.0 * log10(max(pk_R, 1.0));
      }
    }
  }

  double f0_raw = peak_bin * ((double)SAMPLE_RATE / FFT_SAMPLES);
  float f0_corr = (float)(f0_raw - df_t);

  double r_h2 = maxRAround(peak_bin * 2);
  double r_h3 = maxRAround(peak_bin * 3);
  double r_h4 = (peak_bin * 4 < 247) ? maxRAround(peak_bin * 4) : 0.0;

  double h2_prom = log1p(r_h2);
  double h3_prom = log1p(r_h3);
  double h4_prom = log1p(r_h4);
  double h2_h1_w = log1p(min(r_h2 / (pk_R + 1e-6), 3.0));
  double h3_h1_w = log1p(min(r_h3 / (pk_R + 1e-6), 3.0));
  double h3_h2_w = log1p(min(r_h3 / (r_h2 + 1e-6), 3.0));
  double harm_comb_w = log1p((pk_R + r_h2 + r_h3) / 3.0);

  // Ceiling-fan-safe whitened sub-harmonic ratio (ignore f0/3 if < bin 12 / 187.5 Hz)
  int b2 = max(9, peak_bin / 2);
  int b3 = peak_bin / 3;
  int b23 = max(9, (2 * peak_bin) / 3);
  double sub_R = max(maxRAround(b2), maxRAround(b23));
  if (b3 >= 12) sub_R = max(sub_R, maxRAround(b3));
  double sub_pk_w = min(sub_R / (pk_R + 1e-6), 2.5);

  int k9 = clampK(9), k14 = clampK(14), k21 = clampK(21);
  int k28 = clampK(28), k35 = clampK(35), k42 = clampK(42), k68 = clampK(68);
  double ep_bio = sumEp(k9, k68) + 1e-6;

  double pk3_tonal_w = sumEp(max(9, peak_bin - 1), min(80, peak_bin + 2)) / ep_bio;
  double skirt_w = (specR[max(9, peak_bin - 3)] + specR[max(9, peak_bin - 2)] +
                    specR[min(80, peak_bin + 2)] + specR[min(80, peak_bin + 3)]) / (4.0 * pk_R + 1e-6);

  double r_sum = 0.0, r_log_sum = 0.0;
  double ep_b_sum = 0.0, e_b_sum = 0.0, w_freq = 0.0;
  for (int k = k_lo; k < k_hi; k++) {
    double rp = max(specR[k], 1e-4);
    r_sum += rp;
    r_log_sum += log(rp);
    ep_b_sum += (specEp[k] + 1e-8);
    double eb = specE[k] + 1e-6;
    e_b_sum += eb;
    w_freq += (k * ((double)SAMPLE_RATE / FFT_SAMPLES)) * eb;
  }
  double flatness_w = exp(r_log_sum / (double)band_len) / (r_sum / (double)band_len + 1e-6);

  double ent_sum = 0.0;
  for (int k = k_lo; k < k_hi; k++) {
    double pp = (specEp[k] + 1e-8) / ep_b_sum;
    ent_sum -= pp * log(pp + 1e-12);
  }
  double spec_entropy_w = ent_sum / log((double)band_len);

  double w_cent_raw = w_freq / e_b_sum;
  double w_centroid = w_cent_raw - df_t;
  double var_sum = 0.0;
  for (int k = k_lo; k < k_hi; k++) {
    double df = k * ((double)SAMPLE_RATE / FFT_SAMPLES) - w_cent_raw;
    var_sum += df * df * (specE[k] + 1e-6);
  }
  double w_spread = sqrt(var_sum / e_b_sum);

  double wb_140_220  = log1p(100.0 * sumEp(k9, k14) / ep_bio);
  double wb_220_328  = log1p(100.0 * sumEp(k14, k21) / ep_bio);
  double wb_328_438  = log1p(100.0 * sumEp(k21, k28) / ep_bio);
  double wb_438_547  = log1p(100.0 * sumEp(k28, k35) / ep_bio);
  double wb_547_656  = log1p(100.0 * sumEp(k35, k42) / ep_bio);
  double wb_656_1062 = log1p(100.0 * sumEp(k42, k68) / ep_bio);

  double hi_max_R = 0.0;
  int hi_tonal = 0;
  int hi_len = max(1, 180 - k68);
  for (int k = k68; k < 180; k++) {
    if (specR[k] > hi_max_R) hi_max_R = specR[k];
    if (specR[k] >= 2.5) hi_tonal++;
  }
  double hi_crest_w = log1p(hi_max_R);
  double hi_tonal_cnt = (double)hi_tonal / (double)hi_len;

  float feats[P2_N_FEATURES] = {
    f0_corr, (float)snr_db, (float)h2_prom, (float)h3_prom, (float)h4_prom,
    (float)h2_h1_w, (float)h3_h1_w, (float)h3_h2_w,
    (float)harm_comb_w, (float)sub_pk_w, (float)pk3_tonal_w, (float)skirt_w,
    (float)flatness_w, (float)spec_entropy_w, (float)w_centroid, (float)w_spread,
    (float)wb_140_220, (float)wb_220_328, (float)wb_328_438, (float)wb_438_547, (float)wb_547_656, (float)wb_656_1062,
    (float)hi_crest_w, (float)hi_tonal_cnt
  };

  bool in_cooldown = (millis() < fan_cooldown_until_ms);
  float dyn_rms_floor = max(MIN_ACTIVE_RMS, min(2000.0f, ambient_rms_ema * 1.18f));
  bool above_floor = (!in_cooldown && snr_db >= MIN_ACTIVE_SNR && e_rms >= dyn_rms_floor);

  float probs[P2_N_CLASSES] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  if (above_floor) {
    runWingBeatQ1Probs(feats, probs);
  }

  // Hierarchical Tier-1 Guild Decision (v6.0 Clean-Real — Device-Neutral Whitened Physics):
  //   tier1: 0=Noise/Env, 1=Bee, 2=Female Vector {Aedes(2)+Anopheles(3)+Culex(4)}, 5=Male
  float p_vec = probs[2] + probs[3] + probs[4];
  int tier1 = 0;
  float tier1_prob = probs[0];

  bool vec_harm_ok = (r_h2 >= 2.2 && r_h3 >= 2.0 && sub_pk_w <= 0.58);
  bool mal_harm_ok = (r_h2 >= 2.0 && r_h3 >= 1.8 && sub_pk_w <= 0.58);

  if (above_floor) {
    if (p_vec >= VEC_PROB_MIN && f0_corr >= 340.0f && f0_corr <= 660.0f && vec_harm_ok) {
      tier1 = 2; tier1_prob = p_vec;
    } else if (probs[5] >= MAL_PROB_MIN && f0_corr >= 665.0f && f0_corr <= 1020.0f && mal_harm_ok) {
      tier1 = 5; tier1_prob = probs[5];
    } else if (probs[1] >= POL_PROB_MIN && f0_corr >= 135.0f && f0_corr <= 290.0f && r_h2 >= 1.7) {
      tier1 = 1; tier1_prob = probs[1];
    }
  }

  if (!in_cooldown && tier1 == 0 && e_rms < 2200.0) {
    ambient_rms_ema = 0.92f * ambient_rms_ema + 0.08f * (float)e_rms;
  }

  // Stage-2 Optical Pollinator Veto (only when IR1 is healthy & saw a slow 26-60ms bee pulse)
  bool optical_bee_veto = (!ir1_stuck && (millis() - last_ir1_pulse_ms < 500UL) &&
                           last_valid_dt_ms >= 26.0f && last_valid_dt_ms <= 60.0f);
  if (tier1 == 2 && optical_bee_veto) {
    tier1 = 1;
  }

  // 4-Frame Pitch-Locked Streak + Anchor Lock (rejects vocal pitch glides like "mew-mew"!)
  if (tier1 == 2) {
    bool stable_pitch = (vector_streak == 0) ||
                        (fabsf(f0_corr - prev_vec_f0) <= 38.0f && fabsf(f0_corr - streak_anchor_f0) <= 46.0f);
    if (stable_pitch) {
      if (vector_streak == 0) streak_anchor_f0 = f0_corr;
      vector_streak = min((uint8_t)10, (uint8_t)(vector_streak + 1));
      sum_p_aed += probs[2];
      sum_p_ano += probs[3];
      sum_p_cul += probs[4];
    } else {
      vector_streak = 1;
      streak_anchor_f0 = f0_corr;
      sum_p_aed = probs[2]; sum_p_ano = probs[3]; sum_p_cul = probs[4];
    }
    prev_vec_f0 = f0_corr;
  } else {
    vector_streak = 0;
    streak_anchor_f0 = 0.0f;
    sum_p_aed = 0.0f; sum_p_ano = 0.0f; sum_p_cul = 0.0f;
  }
  male_streak = (tier1 == 5) ? min((uint8_t)10, (uint8_t)(male_streak + 1)) : 0;
  pol_streak  = (tier1 == 1) ? min((uint8_t)10, (uint8_t)(pol_streak + 1)) : 0;

  if (tier1 == 1 && pol_streak >= POL_STREAK_REQ) {
    if (millis() - last_pol_count_ms >= 3000UL) {
      cnt_pollinator++; prefs.putUInt("pol", cnt_pollinator);
      last_pol_count_ms = millis();
    }
    held_status_code = 2; status_hold_until_ms = millis() + 1500UL;
  } else if (tier1 == 5 && male_streak >= MALE_STREAK_REQ) {
    if (millis() - last_male_count_ms >= 3000UL) {
      cnt_male++; prefs.putUInt("mal", cnt_male);
      last_male_count_ms = millis();
    }
    held_status_code = 3; status_hold_until_ms = millis() + 1500UL;
  } else if (above_floor && tier1 == 0 && e_rms >= max(1000.0, (double)ambient_rms_ema * 1.55) && millis() >= status_hold_until_ms) {
    held_status_code = 1; status_hold_until_ms = millis() + 700UL;
  }

  bool hold_active = (millis() < status_hold_until_ms);
  digitalWrite(LED_GREEN,  (hold_active && held_status_code == 2) ? HIGH : LOW);
  digitalWrite(LED_YELLOW, (hold_active && held_status_code == 3) ? HIGH : LOW);
  digitalWrite(LED_RED,    (tier1 == 2 && vector_streak >= 2) ? HIGH : LOW);

  // 2. Selective Suction Capture Phase (ONLY on verified 4-frame female vector!)
  if (tier1 == 2 && vector_streak >= VECTOR_STREAK_REQ) {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED, HIGH);
    const char *genus_title = "AEDES (DENGUE)!";
    if (sum_p_ano >= sum_p_aed && sum_p_ano >= sum_p_cul) {
      cnt_anoph++; prefs.putUInt("ano", cnt_anoph);
      genus_title = "ANOPH (MALARIA)";
    } else if (sum_p_cul >= sum_p_aed && sum_p_cul >= sum_p_ano) {
      cnt_culex++; prefs.putUInt("cul", cnt_culex);
      genus_title = "CULEX (FILAR.)";
    } else {
      cnt_aedes++; prefs.putUInt("aed", cnt_aedes);
      genus_title = "AEDES (DENGUE)!";
    }

    char detail_buf[24];
    snprintf(detail_buf, sizeof(detail_buf), "%.0fHz  CONF:%.0f%%", f0_corr, p_vec * 100.0f);

    bool ir2_healthy_before = (!ir2_stuck && (millis() - g2_last_low_ms > 1000UL));
    setRelay(true);
    bool caught = false;
    bool g2_prev_fan = (digitalRead(PIN_GATE_2) == LOW);
    uint32_t fan_t0 = millis();
    uint32_t last_cap_draw = 0;
    uint8_t anim_frame = 0;
    while (millis() - fan_t0 < 5000UL) {
      uint32_t elap = millis() - fan_t0;
      if (last_cap_draw == 0 || (millis() - last_cap_draw >= 160UL)) {
        last_cap_draw = millis();
        drawCaptureOLED(genus_title, detail_buf, elap, 5000UL, anim_frame++);
      }
      bool g2_now = (digitalRead(PIN_GATE_2) == LOW);
      if (ir2_healthy_before && g2_now && !g2_prev_fan) {
        caught = true;
      }
      g2_prev_fan = g2_now;
      delay(15);
    }
    setRelay(false);
    digitalWrite(LED_RED, LOW);
    if (caught) { cnt_caught++; prefs.putUInt("cat", cnt_caught); }
    vector_streak = 0; male_streak = 0; pol_streak = 0;
    streak_anchor_f0 = 0.0f;
    sum_p_aed = 0.0f; sum_p_ano = 0.0f; sum_p_cul = 0.0f;
    held_status_code = 0; status_hold_until_ms = 0;
    fan_cooldown_until_ms = millis() + FAN_COOLDOWN_MS;
    i2s_zero_dma_buffer(I2S_NUM_0);
    last_oled_ms = 0;
  }

  static uint32_t last_idle_log_ms = 0;
  if (serial_verbose && (above_floor || (millis() - last_idle_log_ms >= 2000UL))) {
    if (!above_floor) last_idle_log_ms = millis();
    Serial.printf("[P2v6.0] %s F0:%.1fHz T:%.1fC T1:%d P:%.2f STK:%u [A:%.2f N:%.2f C:%.2f] SNR:%.1f RMS:%.0f FLR:%.0f RH2:%.2f RH3:%.2f SUB:%.2f IR:%s/%s AED:%lu ANO:%lu CUL:%lu BEE:%lu MAL:%lu CAT:%lu\n",
                  above_floor ? "ACT" : "IDL",
                  f0_corr, cached_t, tier1, tier1_prob, vector_streak, probs[2], probs[3], probs[4],
                  snr_db, e_rms, dyn_rms_floor, r_h2, r_h3, sub_pk_w,
                  ir1_stuck ? "POT" : "OK", ir2_stuck ? "POT" : "OK",
                  cnt_aedes, cnt_anoph, cnt_culex, cnt_pollinator, cnt_male, cnt_caught);
  }

  uint32_t refresh_int = (oled_page == 1) ? 120UL : OLED_REFRESH_MS;
  if (millis() - last_oled_ms >= refresh_int || last_oled_ms == 0) {
    last_oled_ms = millis();
    drawMainOLED(in_cooldown, f0_corr, (float)e_rms, dyn_rms_floor, (float)snr_db);
  }
}
