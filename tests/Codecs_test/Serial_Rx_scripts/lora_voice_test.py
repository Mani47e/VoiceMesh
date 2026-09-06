import serial
import numpy as np
import sounddevice as sd
import wave
import os
from datetime import datetime

# ── Config ────────────────────────────────────────
PORT        = "/dev/cu.usbserial-0001"          # Change to your port
BAUD        = 115200
SAMPLE_RATE = 8000
SAVE_FOLDER = "received_audio" # Folder to save files

# ── ADPCM tables ──────────────────────────────────
INDEX_TABLE = [-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8]
STEP_TABLE  = [
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
    50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,
    253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,
    1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,
    3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,
    10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,
    27086,29794,32767
]

def adpcm_decode(adpcm_bytes):
    prevsample = 0
    previndex  = 0
    samples    = []
    for byte in adpcm_bytes:
        for nibble in [byte & 0x0F, (byte >> 4) & 0x0F]:
            step = STEP_TABLE[previndex]
            dq   = step >> 3
            if nibble & 4: dq += step
            if nibble & 2: dq += step >> 1
            if nibble & 1: dq += step >> 2
            prevsample += -dq if (nibble & 8) else dq
            prevsample  = max(-32768, min(32767, prevsample))
            previndex   = max(0, min(88, previndex + INDEX_TABLE[nibble & 7]))
            samples.append(prevsample)
    return np.array(samples, dtype=np.int16)

def wait_for_packet(ser):
    print("Waiting for audio packet...")
    while True:
        b = ser.read(1)
        if b == b'\xaa':
            if ser.read(1) == b'\xbb':
                break
    length_bytes = ser.read(2)
    length = (length_bytes[0] << 8) | length_bytes[1]
    print(f"Receiving {length} ADPCM bytes...")
    data = b''
    while len(data) < length:
        data += ser.read(length - len(data))
    ser.read(2)  # END marker
    print(f"Packet complete. ({length} bytes)")
    return data

def save_wav(samples, filename):
    with wave.open(filename, 'w') as wf:
        wf.setnchannels(1)        # Mono
        wf.setsampwidth(2)        # 16-bit = 2 bytes
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(samples.tobytes())
    print(f"Saved: {filename}")

def play_audio(samples):
    audio = samples.astype(np.float32) / 32768.0
    print("Playing...")
    sd.play(audio, samplerate=SAMPLE_RATE)
    sd.wait()
    print("Done.\n")

# ── Main ──────────────────────────────────────────
def main():
    # Create save folder if it doesn't exist
    os.makedirs(SAVE_FOLDER, exist_ok=True)

    ser = serial.Serial(PORT, BAUD, timeout=10)
    print(f"Connected to {PORT} at {BAUD} baud.")
    print(f"Audio files will be saved to: ./{SAVE_FOLDER}/")
    print("Waiting for voice packets...\n")

    clip_number = 1

    while True:
        try:
            adpcm_data = wait_for_packet(ser)
            samples    = adpcm_decode(adpcm_data)

            # Generate filename with timestamp
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename  = os.path.join(SAVE_FOLDER, f"clip_{clip_number:03d}_{timestamp}.wav")

            # Save then play
            save_wav(samples, filename)
            play_audio(samples)

            clip_number += 1

        except KeyboardInterrupt:
            print(f"\nExiting. {clip_number - 1} clip(s) saved in ./{SAVE_FOLDER}/")
            break
        except Exception as e:
            print(f"Error: {e}")
            continue

    ser.close()

if __name__ == "__main__":
    main()