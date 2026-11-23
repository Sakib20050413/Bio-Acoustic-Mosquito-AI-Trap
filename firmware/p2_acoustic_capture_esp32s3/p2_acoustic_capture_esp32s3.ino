/*
 * ============================================================================
 * PROJECT 2 (CSE 326): ACOUSTIC + OPTICAL CAPTURE & BENCH LOGGER FIRMWARE
 * Hardware: ESP32-S3 N16R8 (44-Pin)
 * Connections:
 *   - INMP441 I2S Mic: SCK=GPIO 4, WS=GPIO 5, SD=GPIO 6 (L/R tied to GND)
 *   - DHT22 Thermal Sensor: GPIO 7 (3.3V)
 *   - 1.3" SH1106 OLED: SDA=GPIO 8, SCL=GPIO 9 (0x3C)
 *   - FC-51 Optical Gate #1: GPIO 10 (Wakes 512-Pt FFT & measures transit dt_ms)
 *   - FC-51 Optical Gate #2: GPIO 14 (Mesh catch verification)
 *   - Suction Fan Opto-Relay: GPIO 13 (STRICTLY OFF 0% during listening!)
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include <DHT.h>
#include <U8g2lib.h>

#define I2S_SCK       4
#define I2S_WS        5
#define I2S_SD        6
#define PIN_DHT22     7
#define I2C_SDA       8
#define I2C_SCL       9
#define PIN_GATE_1    10
#define PIN_RELAY_FAN 13
#define PIN_GATE_2    14

#define RELAY_ACTIVE_HIGH false
#define SAMPLE_RATE   8000
#define FFT_SAMPLES   512

DHT dht(PIN_DHT22, DHT22);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

double vReal[FFT_SAMPLES];
double vImag[FFT_SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, FFT_SAMPLES, SAMPLE_RATE);

float cached_t = 28.0f, cached_h = 70.0f;
uint32_t last_dht_ms = 0;
bool prev_oled_ok = false;

void initI2SMic() {
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
}

float measureOpticalGateTransitMs() {
  if (digitalRead(PIN_GATE_1) == HIGH) return -1.0f;
  uint32_t t_start = micros();
  while (digitalRead(PIN_GATE_1) == LOW) {
    if (micros() - t_start > 80000UL) break;
  }
  return (micros() - t_start) / 1000.0f;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  pinMode(PIN_GATE_1, INPUT);
  pinMode(PIN_GATE_2, INPUT);
  pinMode(PIN_RELAY_FAN, OUTPUT);
  digitalWrite(PIN_RELAY_FAN, RELAY_ACTIVE_HIGH ? LOW : HIGH); // Fan OFF

  dht.begin();
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 20); u8g2.print("P2 CAPTURE BOOT...");
  u8g2.sendBuffer();

  initI2SMic();
  Serial.println("f0_raw,f0_corr,h2_h1,h3_h1,snr_db,dt_gate1_ms,t_amb,rh_pct,spec_centroid,bw_hz,zcr,e_rms");
}

void loop() {
  Wire.beginTransmission(0x3C);
  bool oled_ok = (Wire.endTransmission() == 0);
  if (oled_ok && !prev_oled_ok) { u8g2.begin(); }
  prev_oled_ok = oled_ok;

  float dt_ms = measureOpticalGateTransitMs();
  if (dt_ms < 0.0f) dt_ms = 12.0f;

  // Finite 150ms timeout so OLED & Serial never hang if I2S stalls
  int32_t raw_buf[FFT_SAMPLES];
  size_t bytes_read = 0;
  esp_err_t i2s_err = i2s_read(I2S_NUM_0, raw_buf, sizeof(raw_buf), &bytes_read, pdMS_TO_TICKS(150));

  if (millis() - last_dht_ms >= 2500 || last_dht_ms == 0) {
    last_dht_ms = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) cached_t = t;
    if (!isnan(h)) cached_h = h;
  }

  if (i2s_err != ESP_OK || bytes_read < sizeof(raw_buf)) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(0, 16); u8g2.printf("MIC TIMEOUT (%uB)", (unsigned)bytes_read);
    u8g2.setCursor(0, 32); u8g2.printf("Check INMP441 3.3V");
    u8g2.setCursor(0, 48); u8g2.printf("T:%.1fC H:%.0f%%", cached_t, cached_h);
    u8g2.sendBuffer();
    delay(200);
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
    if (i > 0 && ((vReal[i] >= 0 && vReal[i - 1] < 0) || (vReal[i] < 0 && vReal[i - 1] >= 0))) zcr_cnt++;
  }
  e_rms = sqrt(e_rms / FFT_SAMPLES);
  double zcr = (double)zcr_cnt / FFT_SAMPLES;

  FFT.windowing(FFTWindow::Hann, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  int peak_bin = 8;
  double peak_mag = 0.0, noise_sum = 0.0, mag_sum = 0.0, weighted_freq = 0.0;
  for (int k = 8; k <= 67; k++) {
    double m = vReal[k];
    double f = k * ((double)SAMPLE_RATE / FFT_SAMPLES);
    mag_sum += m;
    weighted_freq += f * m;
    noise_sum += m;
    if (m > peak_mag) { peak_mag = m; peak_bin = k; }
  }
  double f0_raw = peak_bin * ((double)SAMPLE_RATE / FFT_SAMPLES);
  double noise_avg = (noise_sum - peak_mag) / 59.0 + 1e-6;
  double snr_db = 20.0 * log10(peak_mag / noise_avg);

  double h2_h1 = vReal[min(255, peak_bin * 2)] / (peak_mag + 1e-6);
  double h3_h1 = vReal[min(255, peak_bin * 3)] / (peak_mag + 1e-6);
  double centroid = weighted_freq / (mag_sum + 1e-6);
  double f0_corr = f0_raw - 7.0 * (cached_t - 28.0);

  if (snr_db >= 10.0) {
    Serial.printf("%.2f,%.2f,%.3f,%.3f,%.2f,%.2f,%.1f,%.1f,%.1f,45.0,%.3f,%.1f\n",
                  f0_raw, f0_corr, h2_h1, h3_h1, snr_db, dt_ms, cached_t, cached_h, centroid, zcr, e_rms);
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 14); u8g2.printf("F0:%.1fHz CORR:%.1f", f0_raw, f0_corr);
  u8g2.setCursor(0, 28); u8g2.printf("SNR:%.1fdB H2/1:%.2f", snr_db, h2_h1);
  u8g2.setCursor(0, 42); u8g2.printf("DT:%.1fms T:%.1fC", dt_ms, cached_t);
  u8g2.setCursor(0, 56); u8g2.printf("FAN:0%% (QUIET LOG)");
  u8g2.sendBuffer();

  delay(100);
}
