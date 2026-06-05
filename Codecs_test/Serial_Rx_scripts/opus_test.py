import serial
import struct
import pyaudio
import opuslib
import wave

SERIAL_PORT = "/dev/cu.usbmodem101"  # change to your port
BAUD        = 921600
SAMPLE_RATE = 8000
FRAME_SIZE  = 160
DURATION    = 5
OUTPUT_FILE = "recorded.wav"

ser     = serial.Serial(SERIAL_PORT, BAUD, timeout=1)
decoder = opuslib.Decoder(SAMPLE_RATE, 1)
p       = pyaudio.PyAudio()
stream  = p.open(format=pyaudio.paInt16, channels=1,
                 rate=SAMPLE_RATE, output=True)

wf = wave.open(OUTPUT_FILE, 'wb')
wf.setnchannels(1)
wf.setsampwidth(2)
wf.setframerate(SAMPLE_RATE)

total_samples = 0
target_samples = SAMPLE_RATE * DURATION  # 8000 * 5 = 40000

print(f"Recording for {DURATION} seconds...")

while total_samples < target_samples:
    header = ser.read(2)
    if len(header) < 2:
        continue
    length = struct.unpack('>H', header)[0]
    if length == 0 or length > 512:
        continue

    packet = ser.read(length)
    if len(packet) < length:
        continue

    pcm = decoder.decode(bytes(packet), FRAME_SIZE)
    stream.write(pcm)
    wf.writeframes(pcm)
    total_samples += FRAME_SIZE

print(f"Done! Saved to {OUTPUT_FILE}")

stream.stop_stream()
stream.close()
p.terminate()
wf.close()
ser.close()