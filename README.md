# VoiceMesh

Voice streaming over mesh radio networks, built on the ESP32-C6 and nRF52840.

VoiceMesh explores low-bitrate audio streaming across wireless mesh links, aiming for real-time (or near real-time) voice transport on constrained embedded hardware rather than traditional WiFi/BLE audio streaming.

## Status

Early-stage / experimental. Codecs and radio links are being prototyped and benchmarked independently before being combined into a full mesh pipeline.

## Repository layout

| Path | Description |
|---|---|
| [`Codecs_test/`](./Codecs_test) | Standalone codec experiments (PCM / ADPCM encode-decode benchmarks) |
| [`esp32-C6/`](./esp32-C6) | ESP32-C6 firmware for the sender/receiver voice nodes |
| [`nrf52840/`](./nrf52840) | nRF52840 firmware and radio experiments |

## Tested protocols

- WiFi UDP — 8-bit PCM
- WiFi UDP — 4-bit ADPCM

## Codecs tested

- Raw 8-bit PCM @ 8 kHz
- 4-bit delta ADPCM @ 8 kHz
- 4-bit ADPCM @ 8 kHz

## Hardware

- ESP32-C6 (used as both sender and receiver)
- INMP441 — I2S MEMS microphone
- MAX98357A — I2S class-D amplifier
- 4Ω 3W speaker
- nRF24L01+ PA/LNA — 2.4 GHz radio module
- Reyax RYLR896 — LoRa module

## Getting started

1. Clone the repo:
   ```bash
   git clone https://github.com/Mani47e/VoiceMesh.git
   cd VoiceMesh
   ```
2. Pick a target directory (`esp32-C6/` or `nrf52840/`) and follow its build setup (ESP-IDF for the ESP32-C6, nRF Connect SDK / Zephyr for the nRF52840).
3. Wire up the I2S microphone/amplifier and the radio module per the hardware list above.
4. Flash the sender and receiver nodes and start streaming.

> Build/flash steps are hardware-target specific — see the README inside each target folder for exact commands, once added.

## Roadmap ideas

- Expand mesh routing beyond point-to-point WiFi UDP links
- Bring LoRa and nRF24L01+ paths up to the same codec test coverage as WiFi
- Package the codec benchmarks in `Codecs_test/` into a reusable library

## Contributing

Issues and PRs are welcome — this is an active experimentation repo, so expect the structure to shift as codecs and radio backends are compared.

## License

No license has been specified yet. Add a `LICENSE` file to clarify how others can use this code.
