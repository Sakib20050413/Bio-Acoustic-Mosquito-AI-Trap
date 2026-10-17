"""
HumBugDB & Kaggle Wingbeats Test Audio Generator
Generates realistic multi-harmonic mosquito and housefly test .wav files
for the winter lab-final physical viva proof.
"""

import math
import struct
import wave
import os

def generate_tone(filename, base_freq, harmonics, duration_sec=5.0, sample_rate=16000):
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    num_samples = int(duration_sec * sample_rate)
    
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(1) # Mono
        wav_file.setsampwidth(2) # 16-bit
        wav_file.setframerate(sample_rate)
        
        frames = bytearray()
        for i in range(num_samples):
            t = float(i) / sample_rate
            sample_val = 0.0
            
            # Synthesize base frequency + biological harmonics
            for harmonic_num, amplitude in harmonics.items():
                freq = base_freq * harmonic_num
                # Add slight biological jitter / flutter
                flutter = 1.0 + 0.015 * math.sin(2 * math.pi * 5 * t)
                sample_val += amplitude * math.sin(2 * math.pi * freq * flutter * t)
                
            # Normalize and clamp to 16-bit signed integer
            sample_val = max(-1.0, min(1.0, sample_val * 0.6))
            int_val = int(sample_val * 32767.0)
            frames.extend(struct.pack('<h', int_val))
            
        wav_file.writeframes(frames)
    print(f"Generated: {filename} ({base_freq} Hz, {duration_sec}s)")

if __name__ == '__main__':
    base_dir = os.path.dirname(os.path.abspath(__file__))
    
    # 1. Housefly: ~160 Hz fundamental + strong subharmonics
    generate_tone(os.path.join(base_dir, "Housefly_160Hz.wav"), 160.0, {1: 1.0, 2: 0.6, 3: 0.3})
    
    # 2. Male Culex: ~560 Hz high-pitched tone
    generate_tone(os.path.join(base_dir, "Male_Culex_560Hz.wav"), 560.0, {1: 1.0, 2: 0.4, 3: 0.15})
    
    # 3. Female Aedes aegypti (Dengue Vector): ~495 Hz with rich 2nd and 3rd harmonics
    generate_tone(os.path.join(base_dir, "Female_Aedes_495Hz.wav"), 495.0, {1: 1.0, 2: 0.7, 3: 0.4, 4: 0.2})
    
    # 4. Female Anopheles stephensi (Malaria Vector): ~325 Hz fundamental
    generate_tone(os.path.join(base_dir, "Female_Anopheles_325Hz.wav"), 325.0, {1: 1.0, 2: 0.5, 3: 0.25})
    
    print("\nAll laboratory verification audio files generated successfully!")
