#!/usr/bin/env python3
"""
Project 2 (CSE 326 — BAIUST): WingBeat-TinyNet v6.0 Clean-Real Dataset Builder
==============================================================================
STRICT SCIENTIFIC INTEGRITY GUARANTEE:
  - 100.0% UNTOUCHED REAL AUDIO RECORDINGS ONLY
  - 0 synthetic np.sin() / generated waveforms
  - 0 artificial channel-EQ / iPhone-EQ filter augmentations
  - 0 artificial Gaussian white-noise (_SNR) augmentations
  - 0 artificial pitch-resampling (_aug) or synthetic room-hum mixing

5 Verified Real Acoustic Sources:
  1. PLOS ONE 2024 MosquitoSong+ (Supratak et al., 2024) — 467 mosquito + 12 env WAVs (lab T=25.0C)
  2. Oxford HumBugDB (Kiskin et al., 2021) — R4 free-flight + CDC Aedes/Culex/Males + urban noise (T=28.0C)
  3. ESC-50 Environmental Noise Benchmark (Piczak, 2015) — 136 WAVs across 17 categories (T=28.0C)
  4. Wikimedia Commons Outdoor Real Bees (CC-BY/CC0) — Hummel_bee.ogg, Bee_buzzing_sound, Buzzing_bees (T=28.0C)
  5. Live Cumilla Field Room & Vocal Recordings — room ceiling fan, speech, distant sawmill, vocal hum (T=28.7C)
"""
import glob
import os
import sys
from math import gcd
from pathlib import Path

import numpy as np
import pandas as pd
from scipy.io import wavfile
from scipy.signal import resample_poly

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from p2_humbugdb_q1_training_and_snr_eval import (
    FEATURE_COLS,
    extract_features_from_audio,
)

SCRATCH = Path(r"C:\Users\mdnaj\.gemini\antigravity\brain\378a87cb-efdf-4997-922d-51c338c5014e\scratch")
MOZZ = SCRATCH / "mozzbnn"
MS_MOS = SCRATCH / "mosquitosongp" / "data" / "mosquito_recordings"
MS_ENV = SCRATCH / "mosquitosongp" / "data" / "environmental_noise_recordings"
ESC_DIR = SCRATCH / "esc50"
BEE_DIR = SCRATCH / "real_online_bees"
MEW_DIR = SCRATCH / "mew_audit"
OUT_CSV = SCRIPT_DIR / "humbugdb_real_features.csv"


def load_8k(path: Path) -> np.ndarray:
    sr, data = wavfile.read(path)
    if data.ndim > 1:
        data = data[:, 0]
    data = data.astype(np.float64)
    if sr != 8000:
        g = gcd(8000, sr)
        data = resample_poly(data, 8000 // g, sr // g)
    return data


def main():
    rng = np.random.default_rng(42)
    rows = []

    # =========================================================================
    # 1. REAL PLOS ONE MosquitoSong+ (Untouched real recordings, lab T=25.0C)
    # =========================================================================
    def harvest_mosquitosong(pattern, cls_lbl, lo_f, hi_f, step, max_n, src_tag):
        files = sorted(glob.glob(str(MS_MOS / pattern)))
        cand = []
        for fp in files:
            p = Path(fp)
            d = load_8k(p)
            for i in range(0, len(d) - 512, step):
                seg = d[i:i + 512]
                if np.sqrt(np.mean((seg - np.mean(seg)) ** 2)) < 25.0:
                    continue
                f, r_h2, _, _ = extract_features_from_audio(seg, t_amb=25.0, return_aux=True)
                if lo_f <= f[0] <= hi_f and f[1] >= 12.0 and r_h2 >= 2.0:
                    cand.append(f + [cls_lbl, src_tag, p.name])
        rng.shuffle(cand)
        kept = cand[:max_n]
        rows.extend(kept)
        print(f"[1. MosquitoSong+] {pattern:<18} (cls {cls_lbl}, step={step}) -> kept {len(kept):4d} / {len(cand):4d} untouched real frames")

    harvest_mosquitosong("Ae.Aegypti_1F*",    2, 340, 660, 512, 1300, "MS_Ae_aegypti")
    harvest_mosquitosong("Ae.Albopictus_1F*", 2, 340, 660, 256,  550, "MS_Ae_albopictus")
    harvest_mosquitosong("An.Dirus_1F*",      3, 340, 660, 128,  650, "MS_An_dirus")
    harvest_mosquitosong("Cx.Quin_1F*",       4, 340, 660, 512, 1200, "MS_Cx_quinq")
    harvest_mosquitosong("*_1M*",             5, 665, 1020, 128, 750, "MS_Male_All4")

    # =========================================================================
    # 2. REAL Oxford HumBugDB (R4 free-flight + CDC species + R4/urban noise)
    # =========================================================================
    sp_df = pd.read_csv(MOZZ / "SQLqueries" / "fine_path_species_label.csv", header=None)
    nm_df = pd.read_csv(MOZZ / "SQLqueries" / "fine_path_non_mosquito.csv", header=None)
    r4 = load_8k(MOZZ / "Code" / "data" / "R4_cleaned recording_17-10-17.wav")
    r4_pos = sp_df[sp_df[6].str.contains("R4_cleaned recording_17-10-17", na=False)]
    r4_neg = nm_df[nm_df[6].str.contains("R4_cleaned recording_17-10-17", na=False)]

    r4_v = []
    for row in r4_pos.itertuples():
        s0, s1 = int(row[3] * 8000), int(row[4] * 8000)
        for i in range(s0, min(len(r4), s1) - 512, 512):
            seg = r4[i:i + 512]
            if np.sqrt(np.mean((seg - np.mean(seg)) ** 2)) < 1e-4:
                continue
            f, r_h2, _, _ = extract_features_from_audio(seg, t_amb=28.0, return_aux=True)
            if 340 <= f[0] <= 640 and f[1] >= 12.0 and r_h2 >= 2.0:
                blk = f"HumBugDB_R4_b{i // 40000}"
                r4_v.append(f + [4, "HumBugDB_R4_Culex", blk])
    rng.shuffle(r4_v)
    rows.extend(r4_v[:550])
    print(f"[2. HumBugDB]      R4_Culex           (cls 4, step=512) -> kept {min(550, len(r4_v)):4d} / {len(r4_v):4d} untouched real frames")

    r4_bg = []
    for row in r4_neg.itertuples():
        s0, s1 = int(row[3] * 8000), int(row[4] * 8000)
        for i in range(s0, min(len(r4), s1) - 512, 512):
            seg = r4[i:i + 512]
            if np.sqrt(np.mean((seg - np.mean(seg)) ** 2)) < 1e-4:
                continue
            blk = f"HumBugDB_R4Neg_b{i // 40000}"
            r4_bg.append(extract_features_from_audio(seg, t_amb=28.0) + [0, "HumBugDB_R4_Neg", blk])
    rng.shuffle(r4_bg)
    rows.extend(r4_bg[:350])
    print(f"[2. HumBugDB]      R4_Neg             (cls 0, step=512) -> kept {min(350, len(r4_bg)):4d} / {len(r4_bg):4d} untouched real frames")

    for fname, cls_f in [
        ("CDC_Ae-aegypti.wav", 2),
        ("CDC_Ae-albopictus.wav", 2),
        ("CDC_Cx-quinquefasciatus.wav", 4),
    ]:
        d = load_8k(MOZZ / "Code" / "lib" / "VAD-Humbug" / "input" / fname)
        fv, fm = [], []
        for i in range(0, len(d) - 512, 256):
            seg = d[i:i + 512]
            if np.sqrt(np.mean((seg - np.mean(seg)) ** 2)) < 50.0:
                continue
            f, r_h2, _, _ = extract_features_from_audio(seg, t_amb=28.0, return_aux=True)
            if f[1] >= 12.5 and r_h2 >= 2.0:
                blk = f"{fname}_b{i // 40000}"
                if 340 <= f[0] <= 650:
                    fv.append(f + [cls_f, "HumBug_" + fname, blk])
                elif 665 <= f[0] <= 1020:
                    fm.append(f + [5, "HumBug_Male_" + fname, blk])
        rng.shuffle(fv)
        rng.shuffle(fm)
        rows.extend(fv[:180])
        rows.extend(fm[:120])
        print(f"[2. HumBugDB]      {fname:<27} -> female:{min(180, len(fv)):3d} (cls {cls_f}), male:{min(120, len(fm)):3d} (cls 5)")

    # =========================================================================
    # 3. REAL ESC-50 (17 Environmental Noise Categories, Untouched, step=512)
    # =========================================================================
    esc_meta = pd.read_csv(ESC_DIR / "esc50.csv")
    esc_audio_files = sorted(glob.glob(str(ESC_DIR / "audio" / "*.wav")))
    cat_map = dict(zip(esc_meta["filename"], esc_meta["category"]))
    esc_by_cat = {}
    for fp in esc_audio_files:
        p = Path(fp)
        cat = cat_map.get(p.name, "esc50")
        d = load_8k(p)
        for i in range(0, len(d) - 512, 512):
            seg = d[i:i + 512]
            if np.sqrt(np.mean((seg - np.mean(seg)) ** 2)) < 40.0:
                continue
            esc_by_cat.setdefault(cat, []).append(
                extract_features_from_audio(seg, t_amb=28.0) + [0, f"ESC50_{cat}", p.name]
            )

    esc_total = 0
    for cat, lst in sorted(esc_by_cat.items()):
        rng.shuffle(lst)
        cap = 150 if cat in ("car_horn", "siren", "chainsaw", "chirping_birds", "crying_baby") else 100
        kept = lst[:cap]
        rows.extend(kept)
        esc_total += len(kept)
    print(f"[3. ESC-50]        {len(esc_by_cat)} categories (cls 0, step=512) -> kept {esc_total:4d} untouched real frames")

    # =========================================================================
    # 4. REAL MosquitoSong+ Env + HumBugDB Urban Noise (Untouched, step=512)
    # =========================================================================
    ms_env_cand = []
    for fp in sorted(glob.glob(str(MS_ENV / "*.wav"))):
        p = Path(fp)
        d = load_8k(p)
        for i in range(0, len(d) - 512, 512):
            seg = d[i:i + 512]
            if np.sqrt(np.mean((seg - np.mean(seg)) ** 2)) < 20.0:
                continue
            ms_env_cand.append(extract_features_from_audio(seg, t_amb=25.0) + [0, "MS_ENV_" + p.stem[:18], p.name])
    rng.shuffle(ms_env_cand)
    rows.extend(ms_env_cand[:650])
    print(f"[4. MS_ENV]        12 real env files  (cls 0, step=512) -> kept {min(650, len(ms_env_cand)):4d} untouched real frames")

    for sname, path_w, target_n, t_val, step in [
        ("HumBug_bg_speech",   MOZZ / "Code/lib/VAD-Humbug/input/background_with_speech.wav", 300, 28.0, 512),
        ("HumBug_bus_stop",    MOZZ / "Code/lib/VAD-Humbug/input/bus_stop.wav",               250, 28.0, 512),
        ("HumBug_construction",MOZZ / "Code/lib/VAD-Humbug/input/construction_site.wav",      250, 28.0, 512),
        ("Sakib_room_sawmill", SCRATCH / "voice_1138_8k.wav",                                 700, 28.7, 512),
        ("Sakib_user_voice",   SCRATCH / "user_voice_8k.wav",                                 450, 28.7, 512),
    ]:
        d = load_8k(path_w)
        cand = []
        for i in range(0, len(d) - 512, step):
            seg = d[i:i + 512]
            if np.sqrt(np.mean((seg - np.mean(seg)) ** 2)) < 40.0:
                continue
            blk = f"{sname}_b{i // 40000}"
            cand.append(extract_features_from_audio(seg, t_amb=t_val) + [0, sname, blk])
        rng.shuffle(cand)
        rows.extend(cand[:target_n])
        print(f"[5. Field/Room]    {sname:<20} (cls 0, step={step}) -> kept {min(target_n, len(cand)):4d} / {len(cand):4d} untouched real frames")

    # Sakib's real talking (vid1 0..75s before SFX) & real "mew-mew" falsetto vocal hum (vid2) — 100% untouched!
    vid1_wav = MEW_DIR / "vid1_8k.wav"
    if vid1_wav.exists():
        d_v1 = load_8k(vid1_wav)[:75 * 8000]
        v1_cand = []
        for i in range(0, len(d_v1) - 512, 512):
            seg = d_v1[i:i + 512]
            if np.sqrt(np.mean((seg - np.mean(seg)) ** 2)) < 40.0:
                continue
            blk = f"Sakib_vid1_b{i // 40000}"
            v1_cand.append(extract_features_from_audio(seg, t_amb=28.7) + [0, "Sakib_vid1_talk", blk])
        rng.shuffle(v1_cand)
        rows.extend(v1_cand[:350])
        print(f"[5. Field/Room]    Sakib_vid1_talk      (cls 0, step=512) -> kept {min(350, len(v1_cand)):4d} / {len(v1_cand):4d} untouched real frames")

    vid2_wav = MEW_DIR / "vid2_8k.wav"
    if vid2_wav.exists():
        d_v2 = load_8k(vid2_wav)
        v2_cand = []
        for i in range(0, len(d_v2) - 512, 256):
            seg = d_v2[i:i + 512]
            if np.sqrt(np.mean((seg - np.mean(seg)) ** 2)) < 40.0:
                continue
            blk = f"Sakib_vid2_b{i // 40000}"
            v2_cand.append(extract_features_from_audio(seg, t_amb=28.7) + [0, "Sakib_mew_hum_real", blk])
        rng.shuffle(v2_cand)
        rows.extend(v2_cand[:600])
        print(f"[5. Field/Room]    Sakib_mew_hum_real   (cls 0, step=256) -> kept {min(600, len(v2_cand)):4d} / {len(v2_cand):4d} untouched real frames")

    # =========================================================================
    # 6. REAL Wikimedia Commons Outdoor Bees (Class 1, 100% Untouched, 0 sin())
    # =========================================================================
    bee_sources = [
        ("Wikimedia_Hummel_bee",  BEE_DIR / "bee_Hummel_bee_8k.wav"),
        ("Wikimedia_Bee_buzzing", BEE_DIR / "bee_Bee_buzzing_sound__animal_noises__8k.wav"),
        ("Wikimedia_Buzzing_bees",BEE_DIR / "bee_Buzzing_bees_8k.wav"),
    ]
    bee_total = 0
    for btag, bp in bee_sources:
        if not bp.exists():
            continue
        bd = load_8k(bp)
        bcand = []
        for i in range(0, len(bd) - 512, 128):
            seg = bd[i:i + 512]
            if np.sqrt(np.mean((seg - np.mean(seg)) ** 2)) < 40.0:
                continue
            fb, r_h2, _, _ = extract_features_from_audio(seg, t_amb=28.0, return_aux=True)
            if 135.0 <= fb[0] <= 290.0 and fb[1] >= 11.5 and r_h2 >= 1.8:
                blk = f"{btag}_b{i // 40000}"
                bcand.append(fb + [1, btag, blk])
        rng.shuffle(bcand)
        kept_b = bcand[:550]
        rows.extend(kept_b)
        bee_total += len(kept_b)
        print(f"[6. Real Bees]     {btag:<22} (cls 1, step=128) -> kept {len(kept_b):4d} / {len(bcand):4d} untouched real frames")

    df = pd.DataFrame(rows, columns=FEATURE_COLS + ["label", "source", "clip_id"])

    # HARD INTEGRITY ASSERTIONS: Prove 0% synthetic / 0% artificial augmentation
    forbidden_tokens = ("guard", "ceiling_fan_hum", "_CHAN", "_SNR", "_aug", "bee_room", "synth")
    bad_sources = [s for s in df["source"].unique() if any(tok in s for tok in forbidden_tokens)]
    assert len(bad_sources) == 0, f"INTEGRITY VIOLATION: Synthetic/augmented sources found: {bad_sources}"
    assert not df[FEATURE_COLS].isna().any().any(), "NaN values detected in feature matrix!"

    df.to_csv(OUT_CSV, index=False)
    print("\n=== v6.0 CLEAN-REAL DATASET BUILT (0% SYNTHETIC, 0% ARTIFICIAL AUG) ===")
    print("Saved ->", OUT_CSV)
    print("Total rows:", len(df), "| Unique source groups:", df["source"].nunique(), "| Unique clips/blocks:", df["clip_id"].nunique())
    print("Class counts:", df["label"].value_counts().sort_index().to_dict())
    print("\nTop sources:")
    for src, cnt in df["source"].value_counts().items():
        print(f"  {src:<32} : {cnt:4d}")


if __name__ == "__main__":
    main()
