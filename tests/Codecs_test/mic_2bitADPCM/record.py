import serial
import wave
import time
import struct

# ---- Settings ----
PORT        = "/dev/cu.usbmodem101"
BAUD        = 115200
SAMPLE_RATE = 8000
RECORD_SECS = 5
OUTPUT_FILE = "output.wav"

# 2-bit ADPCM at 8000 Hz = 2000 bytes/sec
bytes_to_read = SAMPLE_RATE * RECORD_SECS // 4  # 4 samples per byte

# ---- 2-bit ADPCM decoder tables ----
index_table = [-1, 2, -1, 2]
step_table  = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
]

def decode_adpcm2(data):
    predicted  = 0
    step_index = 0
    pcm_samples = []

    for byte in data:
        # Extract four 2-bit nibbles from each byte
        for shift in [0, 2, 4, 6]:
            nibble = (byte >> shift) & 0x03
            step   = step_table[step_index]

            if nibble & 1:
                diff = step + (step >> 2)   # large step
            else:
                diff = step >> 2             # small step

            if nibble & 2:
                predicted -= diff            # negative
            else:
                predicted += diff            # positive

            # Clamp to 16-bit range
            predicted = max(-32768, min(32767, predicted))

            step_index = max(0, min(88,
                step_index + index_table[nibble]))

            pcm_samples.append(predicted)

    return pcm_samples

# ---- Main ----
print(f"Opening {PORT} at {BAUD} baud...")
ser = serial.Serial(PORT, BAUD, timeout=2)
time.sleep(1)
ser.reset_input_buffer()

print(f"Recording {RECORD_SECS} seconds...")
raw_data = b""
while len(raw_data) < bytes_to_read:
    chunk = ser.read(512)
    raw_data += chunk
    print(f"  {len(raw_data)}/{bytes_to_read} bytes", end="\r")

ser.close()

print("\nDecoding ADPCM...")
pcm = decode_adpcm2(raw_data[:bytes_to_read])

# Pack decoded samples as 16-bit signed integers
pcm_bytes = struct.pack(f"<{len(pcm)}h", *pcm)

print(f"Writing {OUTPUT_FILE}...")
with wave.open(OUTPUT_FILE, 'wb') as wf:
    wf.setnchannels(1)
    wf.setsampwidth(2)          # 16-bit output WAV (decoded from 2-bit ADPCM)
    wf.setframerate(SAMPLE_RATE)
    wf.writeframes(pcm_bytes)

print(f"Done. {len(pcm)} samples, {len(pcm_bytes)} bytes saved.")