#pragma once
#include <Arduino.h>

enum InsectClassification {
    CLASS_NONE = 0,
    CLASS_HOUSEFLY = 1,          // Green LED: Benign pollinator/fly
    CLASS_MALE_MOSQUITO = 2,     // Yellow LED: Non-biting male Culex/Aedes
    CLASS_FEMALE_AEDES = 3,      // Red LED: Dengue / Chikungunya vector -> TRAP!
    CLASS_FEMALE_ANOPHELES = 4,  // Red LED: Malaria vector -> TRAP!
    CLASS_FEMALE_CULEX = 5       // Red LED: Filarial vector -> TRAP!
};

struct ClassificationResult {
    InsectClassification species;
    float fundamentalFreqHz;
    float transitTimeMs;
    float confidence;
    bool shouldTriggerSuctionFan;
};

class TinyMLMosquitoClassifier {
public:
    // Temperature compensation coefficient for mosquito wingbeat pitch
    // As temperature decreases in winter (e.g. 20°C vs standard 28°C baseline),
    // wingbeat frequency drops by ~7.5 Hz per °C.
    // We normalize all frequencies to standard 25°C.
    float compensateFrequency(float rawFreq, float ambientTempC) {
        float deltaT = 25.0f - ambientTempC;
        return rawFreq + (deltaT * 7.5f);
    }

    ClassificationResult classify(float rawFreq, float transitTimeMs, float ambientTempC) {
        ClassificationResult res;
        float normFreq = compensateFrequency(rawFreq, ambientTempC);
        res.fundamentalFreqHz = normFreq;
        res.transitTimeMs = transitTimeMs;
        res.confidence = 0.94f;
        res.shouldTriggerSuctionFan = false;

        // 1. Housefly check: Slow transit (>12ms, big body 15mm) and low frequency (~150-180 Hz)
        if (transitTimeMs > 10.0f || (normFreq >= 130.0f && normFreq <= 195.0f)) {
            res.species = CLASS_HOUSEFLY;
            res.shouldTriggerSuctionFan = false;
            return res;
        }

        // 2. Male Mosquito check: High pitch (~540 - 640 Hz), fast transit (<3.5ms)
        if (normFreq >= 530.0f && normFreq <= 660.0f) {
            res.species = CLASS_MALE_MOSQUITO;
            res.shouldTriggerSuctionFan = false; // Males do not feed on human blood
            return res;
        }

        // 3. Female Aedes aegypti (Dengue Vector): ~470 - 520 Hz
        if (normFreq >= 460.0f && normFreq <= 525.0f) {
            res.species = CLASS_FEMALE_AEDES;
            res.shouldTriggerSuctionFan = true;
            return res;
        }

        // 4. Female Anopheles stephensi (Malaria Vector): ~310 - 365 Hz
        if (normFreq >= 305.0f && normFreq <= 375.0f) {
            res.species = CLASS_FEMALE_ANOPHELES;
            res.shouldTriggerSuctionFan = true;
            return res;
        }

        // 5. Female Culex quinquefasciatus: ~410 - 460 Hz
        if (normFreq >= 390.0f && normFreq < 460.0f) {
            res.species = CLASS_FEMALE_CULEX;
            res.shouldTriggerSuctionFan = true;
            return res;
        }

        res.species = CLASS_NONE;
        return res;
    }
};
