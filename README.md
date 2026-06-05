# VoiceMesh
Voice streaming over mesh radio networks using ESP32-C6 or nRF52840.

## Tested protocols
- WiFi UDP (8-bit PCM, 4-bit ADPCM)


## Hardware
- ESP32-C6 (sender + receiver)
- INMP441 I2S microphone
- MAX98357A I2S amplifier
- 4Ω 3W speaker
- nRF24L01+ PA LNA
- Reyax RYLR896 LoRa

## Codecs tested
- Raw 8-bit PCM @ 8kHz
- 4-bit delta ADPCM @ 8kHz
- 4-bit ADPCM @ 8kHz