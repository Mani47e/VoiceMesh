import serial
import wave
import time

PORT        = "/dev/cu.usbmodem101"
BAUD        = 921600

SAMPLE_RATE = 8000
RECORD_SECS = 5

OUTPUT_FILE = "output.wav"

# ADPCM parameters
predictor = 128

# 8kHz * 1 byte/sample * 5 sec
pcm_samples_needed = SAMPLE_RATE * RECORD_SECS

# Compressed size is half
compressed_bytes_needed = pcm_samples_needed // 2

print(f"Opening port {PORT}...")
ser = serial.Serial(PORT, BAUD, timeout=2)

time.sleep(1)
ser.reset_input_buffer()

print(f"Recording {RECORD_SECS} seconds...")

compressed = b''

while len(compressed) < compressed_bytes_needed:
    chunk = ser.read(1024)
    compressed += chunk

    print(
        f"Captured {len(compressed)}/{compressed_bytes_needed} bytes",
        end="\r"
    )

ser.close()

print("\nDecoding...")

pcm = bytearray()

for byte in compressed:

    high = (byte >> 4) & 0x0F
    low  = byte & 0x0F

    for code in (high, low):

        # Convert 4-bit signed
        if code & 0x08:
            diff = code - 16
        else:
            diff = code

        predictor += diff

        if predictor < 0:
            predictor = 0
        elif predictor > 255:
            predictor = 255

        pcm.append(predictor)

with wave.open(OUTPUT_FILE, "wb") as wf:
    wf.setnchannels(1)
    wf.setsampwidth(1)      # 8-bit PCM
    wf.setframerate(SAMPLE_RATE)
    wf.writeframes(pcm)

print(f"Saved to {OUTPUT_FILE}")