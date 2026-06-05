import serial
import wave
import struct
import time

# ---- Settings ----
PORT        = "/dev/cu.usbmodem101"      # Windows: COM3, COM4 etc. | Linux/Mac: /dev/ttyUSB0 or /dev/ttyACM0
BAUD        = 921600
SAMPLE_RATE = 8000
CHANNELS    = 1
BIT_DEPTH   = 8
RECORD_SECS = 5            # How many seconds to record
OUTPUT_FILE = "output.wav"
# ------------------

bytes_to_read = SAMPLE_RATE * CHANNELS * (BIT_DEPTH // 8) * RECORD_SECS

print(f"Opening port {PORT}...")
ser = serial.Serial(PORT, BAUD, timeout=2)
time.sleep(1)              # Let ESP32 boot and settle
ser.reset_input_buffer()   # Flush any junk bytes from startup

print(f"Recording {RECORD_SECS} seconds...")
audio_data = b""
while len(audio_data) < bytes_to_read:
    chunk = ser.read(1024)
    audio_data += chunk
    remaining = bytes_to_read - len(audio_data)
    print(f"  Captured {len(audio_data)}/{bytes_to_read} bytes", end="\r")

ser.close()

# Write .wav file
with wave.open(OUTPUT_FILE, 'wb') as wf:
    wf.setnchannels(CHANNELS)
    wf.setsampwidth(BIT_DEPTH // 8)
    wf.setframerate(SAMPLE_RATE)
    wf.writeframes(audio_data[:bytes_to_read])

print(f"\nSaved to {OUTPUT_FILE}")