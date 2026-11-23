/*
 * ============================================================================
 * PROJECT 2 (CSE 326): WINGBEAT-TINYNET v5.0 — 6-CLASS HIERARCHICAL Q1 FIRMWARE
 * Hardware: ESP32-S3 N16R8 | Sequential Fan Gating (Fan 0% OFF during listening)
 *
 * 6-Class Multi-Genus Bio-Acoustic TinyML (24 -> 96 -> 48 -> 6, 10-Round QAT INT8):
 *   Class 0: NOISE / ENV         (ESC-50 birds/horn/sawmill/rain/speech/fan -> Fan OFF)
 *   Class 1: POLLINATOR (BEE)    (145-265 Hz + Optical 26-60ms Bee Veto    -> Fan OFF)
 *   Class 2: AEDES (DENGUE/CHIK) (Female Ae. aegypti & Ae. albopictus      -> Fan 5.0s!)
 *   Class 3: ANOPHELES (MALARIA) (Female An. dirus                         -> Fan 5.0s!)
 *   Class 4: CULEX (FILARIASIS)  (Female Cx. quinquefasciatus              -> Fan 5.0s!)
 *   Class 5: MALE MOSQUITO       (Non-biting Males >665 Hz                 -> Fan OFF)
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
#define MIN_ACTIVE_RMS      1100.0f
#define MIN_ACTIVE_SNR      13.0f
#define VECTOR_STREAK_REQ   4
#define MALE_STREAK_REQ     4
#define POL_STREAK_REQ      4
#define VEC_PROB_MIN        0.75f
#define MAL_PROB_MIN        0.76f
#define POL_PROB_MIN        0.75f
#define FAN_COOLDOWN_MS     1500UL
#define OLED_REFRESH_MS     400UL
#define IR_STUCK_MS         150UL

DHT dht(PIN_DHT22, DHT22);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Preferences prefs;

double vReal[FFT_SAMPLES], vImag[FFT_SAMPLES];
double fftPow[FFT_SAMPLES / 2];
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
float ambient_rms_ema = 1100.0f;

bool show_diag_page = false;
bool serial_verbose = true;
bool prev_oled_ok = false;

void setRelay(bool on) {
  digitalWrite(PIN_RELAY_FAN, (on == RELAY_ACTIVE_HIGH) ? HIGH : LOW);
}

static inline double sumPow(int k_start, int k_end_excl) {
  int ks = max(0, k_start);
  int ke = min(FFT_SAMPLES / 2, k_end_excl);
  double s = 0.0;
  for (int k = ks; k < ke; k++) s += fftPow[k];
  return s;
}

static inline double meanMag(int k_start, int k_end_excl) {
  int ks = max(0, k_start);
  int ke = min(FFT_SAMPLES / 2, k_end_excl);
  if (ke <= ks) return 0.0;
  double s = 0.0;
  for (int k = ks; k < ke; k++) s += vReal[k];
  return s / (double)(ke - ks);
}

static inline double maxMagAround(int center_bin) {
  int lo = max(1, center_bin - 1);
  int hi = min(255, center_bin + 1);
  double mx = 0.0;
  for (int k = lo; k <= hi; k++) {
    if (vReal[k] > mx) mx = vReal[k];
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
  u8g2.setDrawColor(1);
  u8g2.clearBuffer();
  u8g2.drawFrame(1, 1, 126, 62);
  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.setCursor(8, 28); u8g2.print(title);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(8, 48); u8g2.print(sub);
  u8g2.sendBuffer();
  delay(ms);
}

void drawMainOLED(bool in_cooldown, float f0_corr, float e_rms) {
  u8g2.setDrawColor(1);
  u8g2.clearBuffer();

  if (!show_diag_page) {
    const char *status_txt = "READY";
    bool active_hold = (millis() < status_hold_until_ms);
    if (in_cooldown) {
      status_txt = "SETTLING";
    } else if (active_hold) {
      if (held_status_code == 2)      status_txt = "BEE SAFE";
      else if (held_status_code == 3) status_txt = "MALE IGN";
      else if (held_status_code == 1) status_txt = "NOISE IGN";
    }

    // Bold top status bar (safe x=4..124, zero glare, zero clipping)
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.setCursor(4, 13);  u8g2.print(status_txt);
    u8g2.setCursor(88, 13); u8g2.print("[OFF]");
    u8g2.drawHLine(2, 17, 124);

    // Genus Vector Counters (Aedes=Dengue/Chik, Anopheles=Malaria, Culex=Filariasis)
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(4, 31);
    u8g2.printf("AED:%lu ANO:%lu CUL:%lu", (unsigned long)cnt_aedes, (unsigned long)cnt_anoph, (unsigned long)cnt_culex);

    // Ecological & Catch Summary
    u8g2.setCursor(4, 45);
    u8g2.printf("Cat:%lu Bee:%lu Mal:%lu", (unsigned long)cnt_caught, (unsigned long)cnt_pollinator, (unsigned long)cnt_male);
    u8g2.drawHLine(2, 50, 124);

    // Clean bottom telemetry + IR health indicator
    u8g2.setCursor(4, 62);
    const char *ir_tag = (ir1_stuck || ir2_stuck) ? "IR:POT!" : "IR:OK";
    u8g2.printf("%.1fC  %.0f%%  %s", cached_t, cached_h, ir_tag);
  } else {
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.setCursor(4, 13); u8g2.print("SENSOR CHECK");
    u8g2.drawHLine(2, 17, 124);

    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(4, 31); u8g2.printf("Temp:%.1fC  Hum:%.0f%%", cached_t, cached_h);
    const char *s1 = ir1_stuck ? "POT!" : (prev_g1_low ? "HIT" : "OK");
    const char *s2 = ir2_stuck ? "POT!" : (prev_g2_low ? "HIT" : "OK");
    u8g2.setCursor(4, 45); u8g2.printf("IR1:%-4s   IR2:%-4s", s1, s2);
    u8g2.setCursor(4, 59); u8g2.printf("%.0fHz   RMS:%.0f", f0_corr, e_rms);
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
  showToastOLED("WingBeat v5.2", "Phone+Live QAT", 400);

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
  Serial.println("\n=== PROJECT 2 (CSE 326) WINGBEAT-TINYNET v5.2 (PHONE+LIVE OCTAVE RECOVERY + VOCAL GUARD) STARTED ===");
}

void loop() {
  Wire.beginTransmission(0x3C);
  bool oled_ok = (Wire.endTransmission() == 0);
  if (oled_ok && !prev_oled_ok) { u8g2.begin(); }
  prev_oled_ok = oled_ok;

  updateIRGates();

  if (digitalRead(BTN_FAN_TEST) == LOW) {
    u8g2.setDrawColor(1);
    u8g2.clearBuffer();
    u8g2.drawFrame(1, 1, 126, 62);
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.setCursor(14, 28); u8g2.print("FAN TEST 1.5s");
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(16, 48); u8g2.print("Relay + LEDs ON");
    u8g2.sendBuffer();
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_YELLOW, HIGH);
    digitalWrite(LED_RED, HIGH);
    setRelay(true);
    delay(1500);
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
    show_diag_page = !show_diag_page;
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

  double e_rms = 0.0;
  int zcr_cnt = 0;
  for (int i = 0; i < FFT_SAMPLES; i++) {
    vReal[i] -= mean;
    e_rms += vReal[i] * vReal[i];
    if (i > 0 && ((vReal[i] >= 0.0) != (vReal[i - 1] >= 0.0))) {
      zcr_cnt++;
    }
  }
  e_rms = sqrt(e_rms / FFT_SAMPLES);
  double zcr = (double)zcr_cnt / FFT_SAMPLES;

  FFT.windowing(FFTWindow::Hann, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  for (int k = 0; k < FFT_SAMPLES / 2; k++) {
    fftPow[k] = vReal[k] * vReal[k];
  }

  // Universal DHT22 Thermal Spectral Bin Warping (+7.0 Hz/degC ref 28.0C)
  // Displays true live ambient (-10C..+50C) while clamping ectothermic flight warp to [10C, 45C]
  double t_eff = max(10.0, min(45.0, (double)cached_t));
  double df_t = 7.0 * (t_eff - 28.0);
  int bsh = (int)lround(df_t / ((double)SAMPLE_RATE / FFT_SAMPLES));
  auto clampK = [bsh](int k) -> int { return max(5, min(175, k + bsh)); };

  int k_lo = max(6, 8 + bsh);
  int k_hi = min(80, 68 + bsh);
  int band_len = max(2, k_hi - k_lo);

  int peak_bin = k_lo;
  double peak_mag = 0.0, mag_sum = 0.0, w_freq = 0.0, log_sum = 0.0, p_band_sum = 0.0;
  for (int k = k_lo; k < k_hi; k++) {
    double m = vReal[k];
    double f = k * ((double)SAMPLE_RATE / FFT_SAMPLES);
    mag_sum += m;
    w_freq += f * m;
    log_sum += log(max(m, 1e-6));
    p_band_sum += fftPow[k];
    if (m > peak_mag) { peak_mag = m; peak_bin = k; }
  }

  double noise_avg = (mag_sum - peak_mag) / (double)(band_len - 1) + 1e-6;
  double snr_db = 20.0 * log10((peak_mag + 1e-6) / noise_avg);

  // Smartphone micro-speaker octave-jump recovery (v5.2):
  // Small phone speakers attenuate 350-550 Hz so 2*f0 (700-1100 Hz) can exceed f0!
  // A true 665-1020 Hz Male has flat noise at 0.5*peak & 1.5*peak; a Female whose 2*f0 peaked has sharp tonal peaks at BOTH!
  if (peak_bin >= clampK(42) && peak_bin <= clampK(78)) {
    int hb = peak_bin / 2;
    int h3b = (peak_bin * 3) / 2;
    if (hb >= clampK(22) && hb <= clampK(41) && h3b < 250) {
      int best_hb = hb;
      for (int k = max(5, hb - 1); k <= min(80, hb + 1); k++) {
        if (vReal[k] > vReal[best_hb]) best_hb = k;
      }
      double m_half = vReal[best_hb];
      double sk_half = 0.25 * (vReal[max(2, best_hb - 3)] + vReal[max(2, best_hb - 2)] +
                               vReal[min(254, best_hb + 2)] + vReal[min(254, best_hb + 3)]) + 1e-6;
      int best_3h = h3b;
      for (int k = max(5, h3b - 1); k <= min(252, h3b + 1); k++) {
        if (vReal[k] > vReal[best_3h]) best_3h = k;
      }
      double m_3half = vReal[best_3h];
      double sk_3h = 0.25 * (vReal[max(2, best_3h - 3)] + vReal[max(2, best_3h - 2)] +
                             vReal[min(254, best_3h + 2)] + vReal[min(254, best_3h + 3)]) + 1e-6;
      if (m_half >= 0.04 * peak_mag && m_half >= 2.2 * sk_half &&
          m_3half >= 0.08 * peak_mag && m_3half >= 2.2 * sk_3h) {
        peak_bin = best_hb;
        peak_mag = m_half;
      }
    }
  }

  double f0_raw = peak_bin * ((double)SAMPLE_RATE / FFT_SAMPLES);
  float f0_corr = (float)(f0_raw - df_t);

  double h2_mag = maxMagAround(peak_bin * 2);
  double h3_mag = maxMagAround(peak_bin * 3);
  double h4_mag = (peak_bin * 4 < 255) ? maxMagAround(peak_bin * 4) : 0.0;
  double h2_h1 = h2_mag / (peak_mag + 1e-6);
  double h3_h1 = h3_mag / (peak_mag + 1e-6);
  double h4_h1 = h4_mag / (peak_mag + 1e-6);
  double harm_comb = (peak_mag + h2_mag + h3_mag) / (meanMag(8, 130) * 3.0 + 1e-6);

  // Ceiling-fan-safe sub-harmonic ratio: ignore f0/3 if it falls inside 120-165 Hz ceiling-fan band (< bin 12)
  int b2 = max(4, peak_bin / 2);
  int b3 = peak_bin / 3;
  int b23 = max(4, (2 * peak_bin) / 3);
  double sub_mag = max(maxMagAround(b2), maxMagAround(b23));
  if (b3 >= 12) sub_mag = max(sub_mag, maxMagAround(b3));
  double sub_pk = sub_mag / (peak_mag + 1e-6);

  int k12 = clampK(12), k21 = clampK(21), k22 = clampK(22);
  int k28 = clampK(28), k35 = clampK(35), k42 = clampK(42);
  int k74 = clampK(74), k86 = clampK(86);

  double e_tot = sumPow(5, 180) + 1e-8;
  double e_80_1050 = sumPow(5, clampK(68)) + 1e-8;
  double e_vec = sumPow(k22, k42) + 1e-8;

  double pk3_tonal = sumPow(max(5, peak_bin - 1), min(80, peak_bin + 2)) / e_80_1050;
  double skirt = (vReal[max(5, peak_bin - 3)] + vReal[max(5, peak_bin - 2)] +
                  vReal[min(80, peak_bin + 2)] + vReal[min(80, peak_bin + 3)]) / (4.0 * peak_mag + 1e-6);
  double flatness = exp(log_sum / (double)band_len) / (mag_sum / (double)band_len + 1e-6);

  double ent_sum = 0.0;
  double p_denom = p_band_sum + 1e-8;
  for (int k = k_lo; k < k_hi; k++) {
    double pp = fftPow[k] / p_denom;
    ent_sum -= pp * log(pp + 1e-12);
  }
  double spec_entropy = ent_sum / log((double)band_len);

  double centroid_raw = w_freq / (mag_sum + 1e-6);
  double centroid = centroid_raw - df_t;
  double var_sum = 0.0;
  for (int k = k_lo; k < k_hi; k++) {
    double df = k * ((double)SAMPLE_RATE / FFT_SAMPLES) - centroid_raw;
    var_sum += df * df * vReal[k];
  }
  double spec_spread = sqrt(var_sum / (mag_sum + 1e-6));

  double log_low_vec = log1p(sumPow(5, k21) / e_vec);
  double log_h2_vec  = log1p(sumPow(k42, k86) / e_vec);
  double log_hi_vec  = log1p(sumPow(k86, 160) / e_vec);

  double b_78_188    = log1p(100.0 * sumPow(5, k12) / e_tot);
  double b_188_328   = log1p(100.0 * sumPow(k12, k21) / e_tot);
  double b_328_438   = log1p(100.0 * sumPow(k21, k28) / e_tot);
  double b_438_547   = log1p(100.0 * sumPow(k28, k35) / e_tot);
  double b_547_656   = log1p(100.0 * sumPow(k35, k42) / e_tot);
  double b_656_1150  = log1p(100.0 * sumPow(k42, k74) / e_tot);
  double b_1150_2800 = log1p(100.0 * sumPow(k74, 180) / e_tot);

  float feats[P2_N_FEATURES] = {
    f0_corr, (float)snr_db, (float)h2_h1, (float)h3_h1, (float)h4_h1, (float)harm_comb, (float)sub_pk,
    (float)pk3_tonal, (float)skirt, (float)flatness, (float)spec_entropy, (float)centroid, (float)spec_spread, (float)zcr,
    (float)log_low_vec, (float)log_h2_vec, (float)log_hi_vec,
    (float)b_78_188, (float)b_188_328, (float)b_328_438, (float)b_438_547, (float)b_547_656, (float)b_656_1150, (float)b_1150_2800
  };

  bool in_cooldown = (millis() < fan_cooldown_until_ms);
  float dyn_rms_floor = max(MIN_ACTIVE_RMS, min(2400.0f, ambient_rms_ema * 1.12f));
  bool above_floor = (!in_cooldown && snr_db >= MIN_ACTIVE_SNR && e_rms >= dyn_rms_floor);

  float probs[P2_N_CLASSES] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  if (above_floor) {
    runWingBeatQ1Probs(feats, probs);
  }

  // Hierarchical Tier-1 Guild Decision:
  //   tier1: 0=Noise/Env, 1=Bee, 2=Female Vector {Aedes(2)+Anopheles(3)+Culex(4)}, 5=Male
  float p_vec = probs[2] + probs[3] + probs[4];
  int tier1 = 0;
  float tier1_prob = probs[0];

  // Bio-acoustic wing-edge harmonic guard (v5.2 — supports BOTH live flat mic AND bright iPhone speaker H2 up to 4.8x!):
  // Real chitin wingbeats ALWAYS carry 3rd harmonic & >1.15 kHz buzz, whereas human "mew-mew" / falsetto has h3 < 0.07 & bhi < 0.70.
  bool vec_harm_ok = (h2_h1 >= 0.085 && h3_h1 >= 0.085 && b_1150_2800 >= 0.90 && sub_pk <= 0.65);
  bool mal_harm_ok = (h2_h1 >= 0.085 && h2_h1 <= 1.80 && h3_h1 >= 0.080 && b_1150_2800 >= 0.85 && sub_pk <= 0.58);

  if (above_floor) {
    if (p_vec >= VEC_PROB_MIN && f0_corr >= 345.0f && f0_corr <= 655.0f && vec_harm_ok) {
      tier1 = 2; tier1_prob = p_vec;
    } else if (probs[5] >= MAL_PROB_MIN && f0_corr >= 665.0f && f0_corr <= 1020.0f && mal_harm_ok) {
      tier1 = 5; tier1_prob = probs[5];
    } else if (probs[1] >= POL_PROB_MIN && f0_corr >= 140.0f && f0_corr <= 280.0f && h2_h1 >= 0.10) {
      tier1 = 1; tier1_prob = probs[1];
    }
  }

  if (!in_cooldown && tier1 == 0 && e_rms < 2600.0) {
    ambient_rms_ema = 0.94f * ambient_rms_ema + 0.06f * (float)e_rms;
  }

  // Stage-2 Optical Pollinator Veto (only when IR1 is healthy & saw a slow 26-60ms bee pulse)
  bool optical_bee_veto = (!ir1_stuck && (millis() - last_ir1_pulse_ms < 500UL) &&
                           last_valid_dt_ms >= 26.0f && last_valid_dt_ms <= 60.0f);
  if (tier1 == 2 && optical_bee_veto) {
    tier1 = 1;
  }

  // 4-Frame Pitch-Locked Streak + Total-Drift Anchor Lock (rejects vocal pitch glides like "mew-mew"!)
  if (tier1 == 2) {
    bool stable_pitch = (vector_streak == 0) ||
                        (fabsf(f0_corr - prev_vec_f0) <= 36.0f && fabsf(f0_corr - streak_anchor_f0) <= 42.0f);
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
  } else if (above_floor && tier1 == 0 && e_rms >= max(2000.0, (double)ambient_rms_ema * 1.45) && millis() >= status_hold_until_ms) {
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

    u8g2.setDrawColor(1);
    u8g2.clearBuffer();
    u8g2.drawFrame(1, 1, 126, 62);
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.setCursor(8, 17); u8g2.print(genus_title);
    u8g2.drawHLine(4, 22, 120);
    u8g2.setFont(u8g2_font_8x13B_tf);
    u8g2.setCursor(14, 40); u8g2.print("FAN ON: 5.0s");
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(14, 56); u8g2.printf("%.0fHz  Conf:%.0f%%", f0_corr, p_vec * 100.0f);
    u8g2.sendBuffer();

    bool ir2_healthy_before = (!ir2_stuck && (millis() - g2_last_low_ms > 1000UL));
    setRelay(true);
    bool caught = false;
    bool g2_prev_fan = (digitalRead(PIN_GATE_2) == LOW);
    uint32_t fan_t0 = millis();
    while (millis() - fan_t0 < 5000UL) {
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

  if (serial_verbose && above_floor) {
    Serial.printf("[P2v5.2] F0:%.1fHz T:%.1fC T1:%d P:%.2f STK:%u [A:%.2f N:%.2f C:%.2f] SNR:%.1f RMS:%.0f H2:%.2f H3:%.2f BHI:%.2f SUB:%.2f IR:%s/%s AED:%lu ANO:%lu CUL:%lu BEE:%lu MAL:%lu CAT:%lu\n",
                  f0_corr, cached_t, tier1, tier1_prob, vector_streak, probs[2], probs[3], probs[4],
                  snr_db, e_rms, h2_h1, h3_h1, b_1150_2800, sub_pk,
                  ir1_stuck ? "POT" : "OK", ir2_stuck ? "POT" : "OK",
                  cnt_aedes, cnt_anoph, cnt_culex, cnt_pollinator, cnt_male, cnt_caught);
  }

  if (millis() - last_oled_ms >= OLED_REFRESH_MS || last_oled_ms == 0) {
    last_oled_ms = millis();
    drawMainOLED(in_cooldown, f0_corr, (float)e_rms);
  }
}
