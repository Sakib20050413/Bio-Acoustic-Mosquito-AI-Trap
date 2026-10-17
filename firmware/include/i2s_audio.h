#pragma once
#include <Arduino.h>
#include <driver/i2s.h>
#include "pinout.h"

#define I2S_SAMPLE_RATE     16000
#define I2S_BUFFER_SIZE     512

class I2SAudioDriver {
public:
    int32_t rawSamples[I2S_BUFFER_SIZE];
    float processedSamples[I2S_BUFFER_SIZE];

    bool begin() {
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = I2S_SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 4,
            .dma_buf_len = I2S_BUFFER_SIZE,
            .use_apll = false,
            .tx_desc_auto_clear = false,
            .fixed_mclk = 0
        };

        i2s_pin_config_t pin_config = {
            .bck_io_num = PIN_I2S_SCK,
            .ws_io_num = PIN_I2S_WS,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = PIN_I2S_SD
        };

        esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
        if (err != ESP_OK) return false;

        err = i2s_set_pin(I2S_NUM_0, &pin_config);
        return (err == ESP_OK);
    }

    // Reads audio buffer and scales from 24-bit in 32-bit container
    bool captureAudioFrame() {
        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_NUM_0, rawSamples, sizeof(rawSamples), &bytesRead, portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) return false;

        for (int i = 0; i < I2S_BUFFER_SIZE; i++) {
            // INMP441 outputs 24-bit MSB-aligned in 32-bit word
            int32_t val = rawSamples[i] >> 8;
            processedSamples[i] = (float)val / 8388608.0f; // Normalize to -1.0 to 1.0
        }
        return true;
    }
};
