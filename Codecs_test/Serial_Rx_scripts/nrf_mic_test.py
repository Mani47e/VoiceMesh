import serial
import wave
import time

PORT        = "/dev/cu.usbmodem1101"  # receiver's port
BAUD        = 115200
SAMPLE_RATE = 8000
RECORD_SECS = 5
OUTPUT_FILE = "output.wav"

bytes_to_read = SAMPLE_RATE * RECORD_SECS  # 8-bit = 1 byte per sample

print(f"Opening {PORT}...")
ser = serial.Serial(PORT, BAUD, timeout=2)
time.sleep(1)
ser.reset_input_buffer()

print(f"Recording {RECORD_SECS} seconds...")
audio_data = b""
while len(audio_data) < bytes_to_read:
    chunk = ser.read(512)
    audio_data += chunk
    print(f"  {len(audio_data)}/{bytes_to_read} bytes", end="\r")

ser.close()

with wave.open(OUTPUT_FILE, 'wb') as wf:
    wf.setnchannels(1)
    wf.setsampwidth(1)           # 8-bit = 1 byte per sample
    wf.setframerate(SAMPLE_RATE)
    wf.writeframes(audio_data[:bytes_to_read])

print(f"\nSaved to {OUTPUT_FILE}")