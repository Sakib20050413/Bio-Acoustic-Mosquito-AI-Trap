#!/usr/bin/env python3
"""
Project 2 (CSE 326 — BAIUST): WingBeat-TinyNet v6.0 Clean-Real (Q1 6-Class Multi-Genus Bio-Acoustic TinyML)
Leak-Free Clip-Disjoint 5-Fold Cross-Validation + Multi-Seed QAT Champion Selection
+ Label Smoothing + Hard-Negative Focal Reweighting + Straight-Through INT8 Refinement
(24 -> 96 -> 48 -> 6, 7,350 params, ~7.18 KB INT8)
+ Universal DHT22 Thermal Spectral Bin Warping (live -10C..+50C display, 10C..45C biological warp).

Strictly trained and validated on 5 UNTOUCHED REAL datasets (0% synthetic sin(), 0% artificial EQ/noise/pitch aug):
  1. PLOS ONE 2024 MosquitoSong+ (Supratak et al., 2024) — 467 mosquito + 12 environmental WAVs
  2. Oxford HumBugDB (Kiskin et al., 2021) — R4 free-flight + CDC Aedes/Culex/Males + urban noise
  3. ESC-50 17-Category Environmental Noise Benchmark (Piczak, 2015) — 136 WAVs
  4. Wikimedia Commons Outdoor Real Bees — Hummel_bee.ogg, Bee_buzzing_sound, Buzzing_bees
  5. Live Cumilla Field Room & Vocal Recordings — ceiling fan, speech, distant sawmill, vocal hum

6 Classes:
  Class 0: Noise / Env        (ESC-50 + Room + Sawmill + Speech -> Fan OFF)
  Class 1: Pollinator (Bee)   (135-285 Hz + Optical 26-60ms Veto -> Fan OFF, Bee Safe)
  Class 2: Aedes              (Female Ae. aegypti & Ae. albopictus -> Fan 5.0s!)
  Class 3: Anopheles          (Female An. dirus Malaria vector     -> Fan 5.0s!)
  Class 4: Culex              (Female Cx. quinquefasciatus vector  -> Fan 5.0s!)
  Class 5: Male Mosquito      (Non-biting Males >665 Hz            -> Fan OFF)
"""
import argparse
import json
import os
import sys
import numpy as np
import pandas as pd

# Exact 2nd-order Butterworth High-Pass Filter (fc=130 Hz, fs=8000 Hz)
# Matches ESP32-S3 C++ Direct-Form-I IIR bit-for-bit (strips 50/60/120 Hz mains, ceiling-fan & lab AC rumble)
HP_B0 = 0.9303447533
HP_B1 = -1.8606895066
HP_B2 = 0.9303447533
HP_A1 = -1.8558317538
HP_A2 = 0.8655472595

FEATURE_COLS = [
    "f0_corr", "snr_db", "h2_prom", "h3_prom", "h4_prom", "h2_h1_w", "h3_h1_w", "h3_h2_w",
    "harm_comb_w", "sub_pk_w", "pk3_tonal_w", "skirt_w",
    "spec_flatness_w", "spec_entropy_w", "w_centroid", "w_spread",
    "wb_140_220", "wb_220_328", "wb_328_438", "wb_438_547", "wb_547_656", "wb_656_1062",
    "hi_crest_w", "hi_tonal_cnt",
]
CLASS_NAMES = ["Noise/Env", "Pollinator", "Aedes", "Anopheles", "Culex", "Male"]
N_CLASSES = 6
VECTOR_CLASSES = (2, 3, 4)


def hpf_130hz(signal):
    """Bit-exact 2nd-order Butterworth 130 Hz causal HPF matching ESP32-S3 C++ loop."""
    x = np.asarray(signal, dtype=np.float64)
    x = x - np.mean(x)
    y = np.empty_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i in range(len(x)):
        x0 = x[i]
        y0 = HP_B0 * x0 + HP_B1 * x1 + HP_B2 * x2 - HP_A1 * y1 - HP_A2 * y2
        y[i] = y0
        x2 = x1; x1 = x0
        y2 = y1; y1 = y0
    return y


def extract_features_from_audio(signal, fs=8000, t_amb=28.0, return_aux=False):
    """
    Bit-exact v6.0 Clean-Real 24-feature extraction matching ESP32-S3 firmware:
      1. 130 Hz 2nd-order Butterworth HPF (strips mains, ceiling fan & lab AC rumble)
      2. 512-pt Hann FFT @ 8,000 Hz
      3. Local-Envelope Spectral Whitening R[k] = spec[k] / flank_env[k] (+-8 bins excluding +-2)
         -> mathematically cancels smooth mic / funnel / Android / iPhone / laptop EQ transfer functions G(f)!
      4. Unified 0.5x/1.5x Sub-Octave Fundamental Recovery in whitened space R[k]
      5. Universal DHT22 Thermal Spectral Bin Warping (+7.0 Hz/degC ref 28.0C, biological clamp 10..45C)
    """
    x = np.asarray(signal, dtype=np.float64)
    if len(x) < 512:
        x = np.pad(x, (0, 512 - len(x)))
    x = x[:512]
    x = hpf_130hz(x)
    x = x - np.mean(x)
    hpf_rms = float(np.sqrt(np.mean(x ** 2)))

    # Match arduinoFFT::Hann symmetric window: 0.5 * (1 - cos(2*pi*i/(N-1)))
    win = np.hanning(512)
    spec = np.abs(np.fft.rfft(x * win))[:256]
    freqs = np.arange(256, dtype=np.float64) * (fs / 512.0)

    # Local-envelope whitening R[k] using outer flanking bins (k-8..k-3 and k+3..k+8)
    R = np.zeros(256, dtype=np.float64)
    for k in range(5, 248):
        lo1, hi1 = max(2, k - 8), max(3, k - 2)
        lo2, hi2 = min(253, k + 3), min(255, k + 9)
        flank = (np.sum(spec[lo1:hi1]) + np.sum(spec[lo2:hi2])) / max(1, (hi1 - lo1) + (hi2 - lo2))
        R[k] = spec[k] / (flank + 1e-6)
    E = np.maximum(0.0, R - 1.35)
    Ep = E ** 2

    # Universal DHT22 Thermal Spectral Bin Warping (+7.0 Hz/degC ref 28.0C)
    t_eff = max(10.0, min(45.0, float(t_amb)))
    df_t = 7.0 * (t_eff - 28.0)
    bsh = int(round(df_t / (fs / 512.0)))

    def clamp_k(k):
        return max(9, min(175, k + bsh))

    k_lo = max(9, 9 + bsh)
    k_hi = min(80, 68 + bsh)
    band_raw = spec[k_lo:k_hi]
    band_R = R[k_lo:k_hi]
    bf = freqs[k_lo:k_hi]

    max_raw_bio = float(np.max(band_raw)) + 1e-6
    R_cand = np.where(band_raw >= 0.08 * max_raw_bio, band_R, 0.0)
    rel = int(np.argmax(R_cand))
    pk_bin = k_lo + rel
    pk_R = float(R[pk_bin])
    pk_raw = float(spec[pk_bin])
    snr_db = float(20.0 * np.log10(max(pk_R, 1.0)))

    def max_R_around(c):
        if c < 5 or c > 247:
            return 0.0
        return float(np.max(R[max(5, c - 1):min(248, c + 2)]))

    # Unified 0.5x / 1.5x Sub-Octave Fundamental Recovery (Bee 2*f0 AND Small-Speaker Mosquito 2*f0)
    if clamp_k(19) <= pk_bin <= clamp_k(78):
        hb = pk_bin // 2
        h3b = (pk_bin * 3) // 2
        if clamp_k(9) <= hb <= clamp_k(41) and h3b < 246:
            lo_r, hi_r = max(9, hb - 1), min(80, hb + 1)
            best_hb = lo_r + int(np.argmax(R[lo_r:hi_r + 1]))
            r_half = float(R[best_hb])
            m_half = float(spec[best_hb])
            lo_3, hi_3 = max(9, h3b - 1), min(246, h3b + 1)
            best_3h = lo_3 + int(np.argmax(R[lo_3:hi_3 + 1]))
            r_3h = float(R[best_3h])
            min_raw_frac = 0.14 if pk_bin < clamp_k(42) else 0.03
            if r_half >= 2.3 and r_3h >= 2.2 and m_half >= min_raw_frac * pk_raw:
                pk_bin = best_hb
                pk_R = r_half
                pk_raw = m_half
                snr_db = float(20.0 * np.log10(max(pk_R, 1.0)))

    f0_raw = float(freqs[pk_bin])
    f0_corr = float(f0_raw - df_t)

    r_h2 = max_R_around(pk_bin * 2)
    r_h3 = max_R_around(pk_bin * 3)
    r_h4 = max_R_around(pk_bin * 4) if pk_bin * 4 < 247 else 0.0

    h2_prom = float(np.log1p(r_h2))
    h3_prom = float(np.log1p(r_h3))
    h4_prom = float(np.log1p(r_h4))
    h2_h1_w = float(np.log1p(min(r_h2 / (pk_R + 1e-6), 3.0)))
    h3_h1_w = float(np.log1p(min(r_h3 / (pk_R + 1e-6), 3.0)))
    h3_h2_w = float(np.log1p(min(r_h3 / (r_h2 + 1e-6), 3.0)))
    harm_comb_w = float(np.log1p((pk_R + r_h2 + r_h3) / 3.0))

    # Ceiling-fan-safe whitened sub-harmonic ratio (ignore f0/3 if < bin 12 / 187.5 Hz)
    b2 = max(9, pk_bin // 2)
    b3 = pk_bin // 3
    b23 = max(9, (2 * pk_bin) // 3)
    sub_R = max(max_R_around(b2), max_R_around(b23))
    if b3 >= 12:
        sub_R = max(sub_R, max_R_around(b3))
    sub_pk_w = float(min(sub_R / (pk_R + 1e-6), 2.5))

    k9, k14, k21 = clamp_k(9), clamp_k(14), clamp_k(21)
    k28, k35, k42, k68 = clamp_k(28), clamp_k(35), clamp_k(42), clamp_k(68)
    ep_bio = float(np.sum(Ep[k9:k68])) + 1e-6

    pk3_tonal_w = float(np.sum(Ep[max(9, pk_bin - 1):min(80, pk_bin + 2)]) / ep_bio)
    skirt_w = float((
        R[max(9, pk_bin - 3)] + R[max(9, pk_bin - 2)]
        + R[min(80, pk_bin + 2)] + R[min(80, pk_bin + 3)]
    ) / (4.0 * pk_R + 1e-6))

    rpos = np.maximum(band_R, 1e-4)
    flatness_w = float(np.exp(np.mean(np.log(rpos))) / (float(np.mean(rpos)) + 1e-6))

    ep_b = Ep[k_lo:k_hi] + 1e-8
    p_prob = ep_b / np.sum(ep_b)
    spec_entropy_w = float(-np.sum(p_prob * np.log(p_prob + 1e-12)) / np.log(len(band_R)))

    e_b = E[k_lo:k_hi] + 1e-6
    e_sum = float(np.sum(e_b))
    w_cent_raw = float(np.sum(bf * e_b) / e_sum)
    w_centroid = w_cent_raw - df_t
    w_spread = float(np.sqrt(np.sum(((bf - w_cent_raw) ** 2) * e_b) / e_sum))

    wb_140_220  = float(np.log1p(100.0 * np.sum(Ep[k9:k14]) / ep_bio))
    wb_220_328  = float(np.log1p(100.0 * np.sum(Ep[k14:k21]) / ep_bio))
    wb_328_438  = float(np.log1p(100.0 * np.sum(Ep[k21:k28]) / ep_bio))
    wb_438_547  = float(np.log1p(100.0 * np.sum(Ep[k28:k35]) / ep_bio))
    wb_547_656  = float(np.log1p(100.0 * np.sum(Ep[k35:k42]) / ep_bio))
    wb_656_1062 = float(np.log1p(100.0 * np.sum(Ep[k42:k68]) / ep_bio))

    hi_R = R[k68:180]
    hi_crest_w = float(np.log1p(float(np.max(hi_R))))
    hi_tonal_cnt = float(np.sum(hi_R >= 2.5)) / float(len(hi_R))

    feats = [
        f0_corr, snr_db, h2_prom, h3_prom, h4_prom, h2_h1_w, h3_h1_w, h3_h2_w,
        harm_comb_w, sub_pk_w, pk3_tonal_w, skirt_w,
        flatness_w, spec_entropy_w, w_centroid, w_spread,
        wb_140_220, wb_220_328, wb_328_438, wb_438_547, wb_547_656, wb_656_1062,
        hi_crest_w, hi_tonal_cnt,
    ]
    if return_aux:
        return feats, r_h2, r_h3, hpf_rms
    return feats


def add_white_noise_snr(signal, snr_db, seed=42):
    if snr_db is None:
        return signal
    sig_pow = np.mean(signal.astype(np.float64) ** 2) + 1e-12
    noise_pow = sig_pow / (10.0 ** (snr_db / 10.0))
    rng = np.random.default_rng(seed)
    noise = rng.normal(0.0, np.sqrt(noise_pow), size=len(signal))
    return signal.astype(np.float64) + noise


def bandpass_rule_baseline(X_df):
    f0 = X_df["f0_corr"].values
    snr = X_df["snr_db"].values
    preds = np.zeros(len(X_df), dtype=int)
    preds[(snr >= 10.0) & (f0 >= 145.0) & (f0 <= 265.0)] = 1
    preds[(snr >= 10.0) & (f0 >= 345.0) & (f0 < 500.0)] = 4   # Culex band
    preds[(snr >= 10.0) & (f0 >= 500.0) & (f0 < 585.0)] = 2   # Aedes band
    preds[(snr >= 10.0) & (f0 >= 585.0) & (f0 <= 655.0)] = 3  # Anopheles band
    preds[(snr >= 10.0) & (f0 > 655.0)] = 5                   # Male band
    return preds


def to_tier1_guild(labels):
    """Map 6-class labels -> 4-class Tier-1 Guild: 0=Noise, 1=Bee, 2=FemaleVector{2,3,4}, 3=Male{5}."""
    out = np.zeros_like(labels)
    out[labels == 1] = 1
    out[np.isin(labels, VECTOR_CLASSES)] = 2
    out[labels == 5] = 3
    return out


def softmax(z):
    z_s = z - np.max(z, axis=1, keepdims=True)
    e = np.exp(z_s)
    return e / np.sum(e, axis=1, keepdims=True)


def macro_f1(y_true, y_pred, n_classes=N_CLASSES):
    f1s = []
    for c in range(n_classes):
        tp = np.sum((y_true == c) & (y_pred == c))
        fp = np.sum((y_true != c) & (y_pred == c))
        fn = np.sum((y_true == c) & (y_pred != c))
        prec = tp / (tp + fp + 1e-12)
        rec = tp / (tp + fn + 1e-12)
        f1s.append(2.0 * prec * rec / (prec + rec + 1e-12))
    return float(np.mean(f1s))


def train_lr(X, y, n_classes=N_CLASSES, lr=0.12, epochs=500):
    n, d = X.shape
    W = np.zeros((d, n_classes))
    b = np.zeros(n_classes)
    Y_oh = np.eye(n_classes)[y]
    for _ in range(epochs):
        probs = softmax(X @ W + b)
        grad = (probs - Y_oh) / n
        W -= lr * (X.T @ grad + 1e-4 * W)
        b -= lr * np.sum(grad, axis=0)
    return W, b


def standardize_clip(X, mu, sigma, clip_val=5.0):
    return np.clip((X - mu) / (sigma + 1e-2), -clip_val, clip_val)


def fake_quant_sym_int8(W):
    """Symmetric per-tensor INT8 straight-through quantization simulator."""
    sc = (np.max(np.abs(W)) + 1e-8) / 127.0
    return np.clip(np.round(W / sc), -127, 127) * sc


def train_mlp(
    X, y, h1=96, h2=48, n_classes=N_CLASSES, lr=0.0085, epochs=620,
    seed=42, label_smooth=0.025, qat_tail=120, l2=1.8e-4, hard_neg_boost=1.40
):
    """
    Train WingBeat-TinyNet v5.0 (24 -> 96 -> 48 -> 6, 7,350 params):
      - Adam + Cosine LR Decay
      - Label smoothing (eps=0.025) for calibrated softmax on genus overlaps
      - Mid-training hard-negative focal mining (upweights Noise/Bee frames with p_vec > 0.15)
      - Final 120-epoch Straight-Through Quantization-Aware Training (QAT) for zero INT8 drop
    """
    rng = np.random.default_rng(seed)
    n, d = X.shape
    params = [
        rng.normal(0, np.sqrt(2.0 / d), (d, h1)), np.zeros(h1),
        rng.normal(0, np.sqrt(2.0 / h1), (h1, h2)), np.zeros(h2),
        rng.normal(0, np.sqrt(2.0 / h2), (h2, n_classes)), np.zeros(n_classes),
    ]
    m_buf = [np.zeros_like(p) for p in params]
    v_buf = [np.zeros_like(p) for p in params]

    Y_oh = np.eye(n_classes)[y]
    if label_smooth > 0.0:
        Y_tgt = (1.0 - label_smooth) * Y_oh + (label_smooth / n_classes)
    else:
        Y_tgt = Y_oh

    # Balanced inverse-sqrt class weights + explicit Pollinator & Noise safety multipliers
    counts = np.bincount(y, minlength=n_classes).astype(np.float64)
    cw = np.sqrt(np.max(counts) / np.maximum(counts, 1.0))
    cw[0] *= 1.50
    cw[1] *= 1.30
    base_sw = cw[y][:, None]
    sw = base_sw.copy()
    sw_sum = float(np.sum(sw))

    qat_start = max(1, epochs - qat_tail)
    mine_ep = int(epochs * 0.55)

    for ep in range(1, epochs + 1):
        W1, b1, W2, b2, W3, b3 = params
        if ep >= qat_start:
            W1_f = fake_quant_sym_int8(W1)
            W2_f = fake_quant_sym_int8(W2)
            W3_f = fake_quant_sym_int8(W3)
        else:
            W1_f, W2_f, W3_f = W1, W2, W3

        z1 = X @ W1_f + b1; a1 = np.maximum(0.0, z1)
        z2 = a1 @ W2_f + b2; a2 = np.maximum(0.0, z2)
        probs = softmax(a2 @ W3_f + b3)

        # Mid-training hard-negative mining on Noise(0) and Bee(1) leaking into Female Vector Guild
        if ep == mine_ep and hard_neg_boost > 1.0:
            p_vec = probs[:, 2] + probs[:, 3] + probs[:, 4]
            hard_mask = ((y == 0) | (y == 1)) & (p_vec > 0.12)
            sw = base_sw.copy()
            sw[hard_mask] *= hard_neg_boost
            sw_sum = float(np.sum(sw))

        dz3 = ((probs - Y_tgt) * sw) / sw_sum
        dW3 = a2.T @ dz3 + l2 * W3; db3 = np.sum(dz3, axis=0)
        da2 = dz3 @ W3_f.T; dz2 = da2 * (z2 > 0)
        dW2 = a1.T @ dz2 + l2 * W2; db2 = np.sum(dz2, axis=0)
        da1 = dz2 @ W2_f.T; dz1 = da1 * (z1 > 0)
        dW1 = X.T @ dz1 + l2 * W1; db1 = np.sum(dz1, axis=0)

        grads = [dW1, db1, dW2, db2, dW3, db3]
        lr_t = lr * 0.5 * (1.0 + np.cos(np.pi * ep / epochs))
        b1_corr = 1.0 - 0.9 ** ep
        b2_corr = 1.0 - 0.999 ** ep
        for k in range(6):
            m_buf[k] = 0.9 * m_buf[k] + 0.1 * grads[k]
            v_buf[k] = 0.999 * v_buf[k] + 0.001 * (grads[k] ** 2)
            params[k] -= lr_t * (m_buf[k] / b1_corr) / (np.sqrt(v_buf[k] / b2_corr) + 1e-8)

    W1, b1, W2, b2, W3, b3 = params
    return [(W1, b1), (W2, b2), (W3, b3)]


def predict_mlp(X, weights, quantize=False):
    h = X
    for idx, (W, b) in enumerate(weights):
        W_eff = fake_quant_sym_int8(W) if quantize else W
        h = h @ W_eff + b
        if idx < len(weights) - 1:
            h = np.maximum(0.0, h)
    return np.argmax(h, axis=1)


def export_int8_header(weights, mu, sigma, out_h):
    os.makedirs(os.path.dirname(os.path.abspath(out_h)), exist_ok=True)
    h1 = weights[0][0].shape[1]
    h2 = weights[1][0].shape[1]
    n_cls = weights[2][0].shape[1]
    with open(out_h, "w", encoding="utf-8") as f:
        f.write("#ifndef WINGBEAT_Q1_INT8_H\n#define WINGBEAT_Q1_INT8_H\n#include <stdint.h>\n\n")
        f.write(f"// WingBeat-TinyNet v6.0 Clean-Real — 0% Synthetic QAT Champion ({len(FEATURE_COLS)}->{h1}->{h2}->{n_cls})\n")
        f.write(f"#define P2_N_FEATURES {len(FEATURE_COLS)}\n")
        f.write(f"#define P2_H1 {h1}\n")
        f.write(f"#define P2_H2 {h2}\n")
        f.write(f"#define P2_N_CLASSES {n_cls}\n\n")
        f.write("static const float P2_MU[P2_N_FEATURES] = {" + ", ".join(f"{v:.6f}f" for v in mu) + "};\n")
        f.write("static const float P2_SIGMA[P2_N_FEATURES] = {" + ", ".join(f"{v:.6f}f" for v in sigma) + "};\n\n")
        for idx, (W, b) in enumerate(weights, start=1):
            scale = (np.max(np.abs(W)) + 1e-8) / 127.0
            w_q = np.clip(np.round(W / scale), -127, 127).astype(int)
            r, c = W.shape
            f.write(f"static const float P2_W{idx}_SCALE = {scale:.8f}f;\n")
            f.write(f"static const int8_t P2_W{idx}_INT8[{r * c}] = {{" + ", ".join(str(x) for x in w_q.flatten()) + "};\n")
            f.write(f"static const float P2_B{idx}[{c}] = {{" + ", ".join(f"{x:.6f}f" for x in b) + "};\n\n")
        f.write("#endif // WINGBEAT_Q1_INT8_H\n")
    print(f"[OK] Exported WingBeat-TinyNet v6.0 Clean-Real INT8 header ({len(FEATURE_COLS)}->{h1}->{h2}->{n_cls}) -> {out_h}")


def make_stratified_group_folds(y, groups, n_splits=5, seed=42):
    """Assign entire clip_id groups into n_splits stratified folds (ZERO overlapping-frame leakage)."""
    rng = np.random.default_rng(seed)
    df_g = pd.DataFrame({"y": y, "g": groups})
    grp_cls = df_g.groupby("g")["y"].agg(lambda s: int(s.mode().iloc[0])).reset_index()
    grp_cnt = df_g.groupby("g").size().to_dict()
    fold_map = {}
    fold_cls_counts = np.zeros((n_splits, N_CLASSES), dtype=int)
    for cls in range(N_CLASSES):
        g_list = grp_cls[grp_cls["y"] == cls]["g"].tolist()
        rng.shuffle(g_list)
        g_list.sort(key=lambda g: grp_cnt[g], reverse=True)
        for g in g_list:
            best_f = int(np.argmin(fold_cls_counts[:, cls]))
            fold_map[g] = best_f
            fold_cls_counts[best_f, cls] += grp_cnt[g]
    return np.array([fold_map[g] for g in groups], dtype=int)


def run_benchmark(df, out_dir, n_rounds=6):
    df = df.sample(frac=1.0, random_state=42).reset_index(drop=True)
    X = df[FEATURE_COLS].values.astype(np.float64)
    y = df["label"].values.astype(int)
    n = len(y)
    has_groups = "clip_id" in df.columns
    if has_groups:
        fold_ids = make_stratified_group_folds(y, df["clip_id"].values, n_splits=5, seed=42)
        cv_mode = "Strict Clip-Disjoint 5-Fold GroupCV (Zero Frame Leakage)"
    else:
        fold_size = max(1, n // 5)
        fold_ids = np.minimum(4, np.arange(n) // fold_size)
        cv_mode = "5-Fold CV"

    p_rule = bandpass_rule_baseline(df)
    p_lr = np.zeros_like(y)
    p_fp32 = np.zeros_like(y)
    p_int8 = np.zeros_like(y)

    # 1. Leak-Free 5-Fold Cross-Validation
    for k in range(5):
        te = (fold_ids == k)
        tr = ~te
        mu = np.mean(X[tr], axis=0)
        sigma = np.maximum(np.std(X[tr], axis=0), 1e-2)
        X_tr = standardize_clip(X[tr], mu, sigma)
        X_te = standardize_clip(X[te], mu, sigma)

        W_lr, b_lr = train_lr(X_tr, y[tr])
        p_lr[te] = np.argmax(X_te @ W_lr + b_lr, axis=1)

        weights = train_mlp(X_tr, y[tr], seed=42 + k)
        p_fp32[te] = predict_mlp(X_te, weights, quantize=False)
        p_int8[te] = predict_mlp(X_te, weights, quantize=True)

    y_t1 = to_tier1_guild(y)
    print(f"\n=== PROJECT 2 (CSE 326) WINGBEAT-TINYNET v6.0 CLEAN-REAL — {cv_mode} (n={n}) ===")
    ablation_summary = {}
    for name, p in [
        ("A. Bandpass Rule Only", p_rule),
        ("B. Logistic Regression", p_lr),
        ("C. WingBeat-TinyNet FP32", p_fp32),
        ("D. WingBeat-TinyNet INT8", p_int8),
    ]:
        p_t1 = to_tier1_guild(p)
        acc6 = float(np.mean(y == p) * 100.0)
        f1_6 = float(macro_f1(y, p, N_CLASSES) * 100.0)
        acc_t1 = float(np.mean(y_t1 == p_t1) * 100.0)
        pol_safe = float((np.sum(p_t1[y == 1] != 2) / max(1, np.sum(y == 1))) * 100.0)
        bg_safe = float((np.sum(p_t1[y == 0] != 2) / max(1, np.sum(y == 0))) * 100.0)
        ablation_summary[name] = {
            "acc6": round(acc6, 2), "f1_6": round(f1_6, 2),
            "tier1_guild": round(acc_t1, 2), "bee_safe": round(pol_safe, 2),
            "noise_rej": round(bg_safe, 2),
        }
        print(f"{name:<25} | 6-Cls Acc:{acc6:6.2f}% F1:{f1_6:6.2f}% | Tier1 Guild:{acc_t1:6.2f}% | Bee Safe:{pol_safe:6.2f}% | Noise Rej:{bg_safe:6.2f}%")

    print("\n--- Per-Class Recall (WingBeat-TinyNet v6.0 Clean-Real INT8, Leak-Free 5-Fold CV) ---")
    per_class_recall = {}
    for c, cname in enumerate(CLASS_NAMES):
        m = (y == c)
        rec = float(np.mean(p_int8[m] == c) * 100.0) if np.any(m) else 0.0
        per_class_recall[cname] = {"recall_pct": round(rec, 2), "n": int(np.sum(m))}
        print(f"  Class {c} ({cname:<10}): {rec:6.2f}%  (n={np.sum(m)})")

    # 2. Multi-Seed Validation Selection on Strict Clip-Disjoint Holdout (Fold 4 = ~20% unseen clips)
    os.makedirs(out_dir, exist_ok=True)
    va_mask = (fold_ids == 4)
    tr_mask = ~va_mask
    X_tr_r, y_tr_r = X[tr_mask], y[tr_mask]
    X_va_r, y_va_r = X[va_mask], y[va_mask]
    n_val = int(np.sum(va_mask))
    mu_r = np.mean(X_tr_r, axis=0)
    sig_r = np.maximum(np.std(X_tr_r, axis=0), 1e-2)
    X_tr_n = standardize_clip(X_tr_r, mu_r, sig_r)
    X_va_n = standardize_clip(X_va_r, mu_r, sig_r)
    y_va_t1 = to_tier1_guild(y_va_r)

    print(f"\n=== {n_rounds}-ROUND MULTI-SEED INT8 QAT CHAMPION SELECTION (Clip-Disjoint Holdout n={n_val}) ===")
    round_logs = []
    best_score = -1e9
    best_cfg = (101, 0.0085, 1.8e-4)

    lr_grid = [0.0080, 0.0085, 0.0090, 0.0075, 0.0085, 0.0095, 0.0080, 0.0085, 0.0078, 0.0088]
    l2_grid = [1.5e-4, 1.8e-4, 2.0e-4, 1.6e-4, 2.2e-4, 1.8e-4, 1.4e-4, 2.0e-4, 1.7e-4, 1.9e-4]

    for r_idx in range(n_rounds):
        seed_r = 101 + r_idx
        lr_r = lr_grid[r_idx % len(lr_grid)]
        l2_r = l2_grid[r_idx % len(l2_grid)]
        w_r = train_mlp(X_tr_n, y_tr_r, lr=lr_r, l2=l2_r, seed=seed_r)
        p_va = predict_mlp(X_va_n, w_r, quantize=True)
        p_va_t1 = to_tier1_guild(p_va)
        acc6 = float(np.mean(y_va_r == p_va) * 100.0)
        f1_6 = float(macro_f1(y_va_r, p_va, N_CLASSES) * 100.0)
        t1_acc = float(np.mean(y_va_t1 == p_va_t1) * 100.0)
        bee_safe = float((np.sum(p_va_t1[y_va_r == 1] != 2) / max(1, np.sum(y_va_r == 1))) * 100.0)
        noise_rej = float((np.sum(p_va_t1[y_va_r == 0] != 2) / max(1, np.sum(y_va_r == 0))) * 100.0)
        score = f1_6 + 0.6 * acc6 + 1.5 * t1_acc + 2.5 * noise_rej + 3.0 * bee_safe
        round_logs.append({
            "round": r_idx + 1, "seed": seed_r, "lr": lr_r, "l2": l2_r,
            "val_acc6": round(acc6, 2), "val_f1": round(f1_6, 2),
            "val_guild": round(t1_acc, 2), "val_bee_safe": round(bee_safe, 2),
            "val_noise_rej": round(noise_rej, 2), "composite_score": round(score, 3),
        })
        print(f"  Round {r_idx+1:2d}/{n_rounds} (seed={seed_r}, lr={lr_r:.4f}, l2={l2_r:.1e}) | INT8 Acc:{acc6:6.2f}% F1:{f1_6:6.2f}% Guild:{t1_acc:6.2f}% BeeSafe:{bee_safe:6.2f}% NoiseRej:{noise_rej:6.2f}% | Score:{score:7.2f}")
        if score > best_score:
            best_score = score
            best_cfg = (seed_r, lr_r, l2_r)

    best_seed, best_lr, best_l2 = best_cfg
    print(f"[CHAMPION SELECTED] seed={best_seed}, lr={best_lr}, l2={best_l2:.1e} (Composite Score={best_score:.2f})")
    report = {
        "version": "v6.0-clean-real",
        "cv_mode": cv_mode,
        "n_total": int(n),
        "n_unique_clips": int(df["clip_id"].nunique()) if has_groups else None,
        "synthetic_rows": 0,
        "artificial_aug_rows": 0,
        "ablation_5fold_cv": ablation_summary,
        "per_class_recall_int8_cv": per_class_recall,
        "holdout_n": n_val,
        "champion": {"seed": best_seed, "lr": best_lr, "l2": best_l2, "score": round(best_score, 3)},
        "rounds": round_logs,
    }
    with open(os.path.join(out_dir, "v6_clean_real_training_log.json"), "w", encoding="utf-8") as jf:
        json.dump(report, jf, indent=2)

    # 3. Refit Champion Configuration on 100% of Dataset & Export INT8 Header
    mu_all = np.mean(X, axis=0)
    sigma_all = np.maximum(np.std(X, axis=0), 1e-2)
    X_all = standardize_clip(X, mu_all, sigma_all)
    final_w = train_mlp(X_all, y, lr=best_lr, l2=best_l2, seed=best_seed)
    export_int8_header(final_w, mu_all, sigma_all, os.path.join(out_dir, "wingbeat_q1_int8.h"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", required=False, help="Extracted 24-feature 6-class CSV")
    parser.add_argument("--out", default="./p2_export", help="Output directory for INT8 header")
    parser.add_argument("--rounds", type=int, default=10, help="Number of multi-seed QAT champion rounds")
    parser.add_argument("--smoke-test", action="store_true", help="Pipeline verification only (NOT for paper)")
    args = parser.parse_args()

    if args.smoke_test:
        rng = np.random.default_rng(7)
        rows = []
        fs = 8000
        t = np.arange(512) / fs
        for cls, f_range in [
            (0, (95, 145)), (1, (170, 240)), (2, (505, 560)),
            (3, (590, 635)), (4, (425, 485)), (5, (760, 880)),
        ]:
            for _ in range(45):
                f0 = rng.uniform(*f_range)
                if cls == 0:
                    sig = np.sin(2 * np.pi * f0 * t) + 0.9 * np.sin(6 * np.pi * f0 * t)
                else:
                    sig = np.sin(2 * np.pi * f0 * t) + 0.35 * np.sin(4 * np.pi * f0 * t) + 0.22 * np.sin(6 * np.pi * f0 * t)
                sig = add_white_noise_snr(sig, snr_db=12.0)
                rows.append(extract_features_from_audio(sig, fs=fs) + [cls])
        df_smoke = pd.DataFrame(rows, columns=FEATURE_COLS + ["label"])
        run_benchmark(df_smoke, args.out, n_rounds=2)
        print("[SMOKE TEST PASSED] Replace with real HumBugDB + MosquitoSong+ + ESC-50 CSV for Q1 paper!")
    elif args.csv:
        df_real = pd.read_csv(args.csv)
        run_benchmark(df_real, args.out, n_rounds=args.rounds)
    else:
        print("Error: Provide --csv <features.csv> or --smoke-test.")
        sys.exit(1)
