# VoiceMesh

**Low-bitrate audio streaming and multi-hop voice transport over constrained wireless mesh networks.**

VoiceMesh explores real-time (and near real-time) digital voice communication across constrained embedded radio links—such as Nordic Enhanced ShockBurst (ESB), IEEE 802.15.4 (Contiki-NG / Zigbee), LoRa, and lightweight WiFi UDP sockets—running on low-power microcontrollers like the **Nordic nRF52840** and **Espressif ESP32-C6**.

Unlike conventional Bluetooth Classic (A2DP/HFP) or WiFi VoIP stacks that carry substantial memory, latency, and connection-state overhead, VoiceMesh focuses on connectionless packet streaming, low-latency audio compression, and decentralized multi-hop packet relaying.

---

## Architecture Overview

```
                      +---------------------------------------+
                      |           INMP441 MEMS Mic            |
                      +---------------------------------------+
                                          | (I2S DMA Rx)
                                          v
                      +---------------------------------------+
                      |        Audio Preprocessing & Gain      |
                      |    (Peak Detect, Scaling, Decimation) |
                      +---------------------------------------+
                                          |
                                          v
                      +---------------------------------------+
                      |           Audio Compression           |
                      |   (PCM16 / ADPCM / μ-law / Codec2)    |
                      +---------------------------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
| Multi-Hop Mesh Network Layer                                                      |
|   - Packet framing: [src_id | dest_id | last_hop_id | ttl | seq | audio_payload]  |
|   - Duplicate suppression cache (seq + src_id ring)                               |
|   - Hop forwarding & TTL decrementing                                             |
|   - Dynamic PTT routing: Direct unicast vs. multi-hop relay vs. broadcast         |
+-----------------------------------------------------------------------------------+
                                          |
                +-------------------------+-------------------------+
                |                         |                         |
                v                         v                         v
     +--------------------+    +--------------------+    +--------------------+
     | Nordic ESB (2Mbps) |    | 802.15.4 / Contiki |    |  LoRa / WiFi UDP   |
     |   (nRF52840 Dongle)|    |  (NullNet / 6Lo)   |    |    (ESP32-C6)      |
     +--------------------+    +--------------------+    +--------------------+
                |                         |                         |
                +-------------------------+-------------------------+
                                          |
                                          v
                      +---------------------------------------+
                      |         Jitter Buffer / RingBuf       |
                      +---------------------------------------+
                                          |
                                          v
                      +---------------------------------------+
                      |        Audio Decoder & Expander       |
                      +---------------------------------------+
                                          | (I2S DMA Tx)
                                          v
                      +---------------------------------------+
                      |      MAX98357A I2S Class-D Amp        |
                      |            4Ω 3W Speaker              |
                      +---------------------------------------+
```

---

## Repository Structure

```
VoiceMesh/
├── esb_voice_node/                 # [Flagship] Zephyr-based multi-hop ESB voice node
│   ├── CMakeLists.txt              # Zephyr build configuration
│   ├── prj.conf                    # Kernel, I2S, ESB, GPIO configuration
│   ├── boards/                     # Devicetree overlays (I2S pins, buttons, LEDs)
│   └── src/
│       ├── main.c                  # System init, button-based PTT routing & loop
│       ├── app/                    # State machine & application context
│       ├── audio/                  # Full-duplex Zephyr I2S driver & memory slabs
│       ├── codec/                  # Audio scaling, gain control, sample formatting
│       ├── network/                # Multi-hop packet structure & deduplication
│       ├── ringbuffer/             # Lockless audio jitter ringbuffer
│       └── transport/              # Nordic Enhanced ShockBurst (ESB) transceiver
├── contiki/                        # Contiki-NG OS mesh experiments
│   ├── 00nullnet-test/             # 802.15.4 link-layer broadcast tests using NullNet
│   └── audio-loopback/             # Low-level nrfx_i2s audio streaming under Contiki
├── contiki_nrf52840/               # Standalone Contiki-NG build tree for nRF52840 Dongle
└── tests/                          # Radio, codec, and peripheral prototyping suites
    ├── Codecs_test/                # Standalone codec implementations & audio tests
    │   ├── 4bit_delta_ADPCM/       # 4-bit Delta-ADPCM encode/decode sketches
    │   ├── mic_2bitADPCM/          # 2-bit ADPCM extreme-low-bitrate experiments
    │   ├── mic_audio_8khz_8bit/    # Raw 8 kHz 8-bit PCM microphone capture
    │   ├── opus_test/              # Opus encoder/decoder testing
    │   └── Serial_Rx_scripts/      # Python utilities for serial audio capture & playback
    ├── esp32-C6/
    │   ├── Arduino/
    │   │   ├── Lora/               # Reyax RYLR896 LoRa voice transmission tests
    │   │   ├── Zigbee/             # IEEE 802.15.4 / Zigbee point-to-point tests
    │   │   ├── wifi_2_way/         # Full-duplex WiFi socket communication
    │   │   └── wifi_Audio_stream/  # WiFi UDP streaming (8-bit PCM & 4-bit ADPCM)
    │   └── ESP-IDF/
    │       └── Loopback_no_Codec/  # ESP-IDF I2S duplex driver with integrated Codec2
    └── nrf52840/                   # Modular Zephyr tests for nRF52840
        ├── blinky/                 # Baseline board bringup
        ├── Display/                # SSD1306 I2C OLED display driver test
        ├── ESB/                    # Basic Enhanced ShockBurst send/receive test
        ├── ESB_2way/               # Bidirectional half-duplex ESB data links
        ├── ESB_Audio/              # Split Tx / Rx nodes for audio streaming
        ├── ESB_Tx/                 # Dedicated ESB audio transmitter node
        └── i2s_test/               # I2S loopback testing with μ-law companding
```

---

## Flagship Implementation: `esb_voice_node`

Located in [`esb_voice_node/`](./esb_voice_node), this is the primary production prototype running on **Zephyr RTOS / nRF Connect SDK** for the **nRF52840 Dongle** (`pca10059`).

### Key Features:
- **Zero-Overhead Mesh Relaying**: Nodes forward packets based on TTL; packets destined for other nodes are automatically relayed.
- **Hardware PTT Target Selection**:
  - `sw0` (Built-in Dongle Button): Transmits Broadcast (`0xFF`) with TTL = 1.
  - `sw1` (External Button 1): Direct Unicast to Target 1.
  - `sw2` (External Button 2): Multi-hop Unicast to Target 2 (TTL = 2).
- **Benchtop Attenuation Simulation**: Includes a software distance filter macro (`SIMULATE_BENCH_DISTANCE`) allowing multi-hop mesh topologies (e.g., `Node A <-> Node B (Relay) <-> Node C`) to be tested on a single workbench.
- **Relay Activity Indicator**: Drives on-board LED (`LD1` / P0.06) whenever the node acts as an intermediate relay hop.
- **Deduplication Engine**: Fixed-capacity ring buffer tracking `(src_id, seq)` pairs to suppress packet broadcast storms.

### Network Frame Format:
```c
struct net_hdr {
    uint8_t  src_id;       /* Originator node ID */
    uint8_t  dest_id;      /* Destination node ID (0xFF for broadcast) */
    uint8_t  last_hop_id;  /* ID of immediate transmitter for this hop */
    uint8_t  ttl;          /* Remaining hops (decremented at each relay) */
    uint16_t seq;          /* Unique packet sequence number */
} __attribute__((packed));

struct net_packet {
    struct net_hdr hdr;
    uint8_t audio[128];    /* 128 bytes raw audio payload per chunk */
} __attribute__((packed));
```

---

## Hardware & Pinout Reference

### Nordic nRF52840 Dongle (`nrf52840dongle_nrf52840`)

Configured via [`esb_voice_node/boards/nrf52840dongle_nrf52840.overlay`](./esb_voice_node/boards/nrf52840dongle_nrf52840.overlay):

| Peripheral | Signal | nRF52840 Pin | Function / Description |
|---|---|---|---|
| **I2S0 Audio** | `SCK` | **P0.02** | Bit Clock (Continuous Master Clock) |
| | `LRCK` / `WS` | **P0.29** | Word Select / Frame Clock (8 kHz) |
| | `SDIN` | **P0.31** | Serial Data In (INMP441 Microphone) |
| | `SDOUT` | **P0.17** | Serial Data Out (MAX98357A Amp) |
| **Buttons (PTT)** | `sw0` | **P1.06** | Built-in Button (Broadcast PTT) |
| | `sw1` | **P0.13** | External Button 1 (Target 1 Direct PTT) |
| | `sw2` | **P0.15** | External Button 2 (Target 2 Multi-Hop PTT) |
| **Status LEDs** | `led0` | **P0.06** | Relay Active Indicator LED |
| **OLED (Optional)**| `SDA` | **P0.22** | I2C1 Data (SSD1306 128x64 display) |
| | `SCL` | **P0.24** | I2C1 Clock (SSD1306 128x64 display) |

> **Note**: `uart0` and `spi1` are explicitly disabled in the devicetree overlay to avoid pin conflicts with `i2s0`.

### Espressif ESP32-C6

Configured in [`tests/esp32-C6/ESP-IDF/Loopback_no_Codec/`](./tests/esp32-C6/ESP-IDF/Loopback_no_Codec/):

| Signal | ESP32-C6 GPIO | Connected To |
|---|---|---|
| `BCLK` | **GPIO 5** | I2S Bit Clock |
| `WS` | **GPIO 4** | I2S Word Select |
| `DIN` | **GPIO 6** | Microphone Data In (INMP441) |
| `DOUT` | **GPIO 7** | Amplifier Data Out (MAX98357A) |

---

## Codecs & Audio Compression Comparison

| Codec | Bitrate @ 8 kHz | Bandwidth Required | Compression Ratio | Target Medium |
|---|---|---|---|---|
| **Raw PCM16** | 128 kbps | 16,000 B/s | 1:1 (Baseline) | High-throughput Nordic ESB / WiFi |
| **Raw PCM8** | 64 kbps | 8,000 B/s | 2:1 | Standard Nordic ESB / WiFi |
| **μ-law (G.711)** | 64 kbps | 8,000 B/s | 2:1 (Log companding) | ESB & point-to-point radio |
| **4-bit ADPCM / Delta** | 32 kbps | 4,000 B/s | 4:1 | 802.15.4 (Contiki-NG / Zigbee) |
| **2-bit ADPCM** | 16 kbps | 2,000 B/s | 8:1 | Constrained 802.15.4 links |
| **Codec2** | 0.7 – 3.2 kbps | 88 – 400 B/s | ~40:1 to ~180:1 | Ultra-narrowband LoRa mesh links |
| **Opus** | 6 – 20 kbps | Scalable | Variable | Low-latency voice over higher-layer mesh |

---

## Getting Started & Build Instructions

### 1. Building `esb_voice_node` (Zephyr / nRF Connect SDK)

Prerequisites: [nRF Connect SDK v2.5.0+](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/installation.html) and `west`.

```bash
cd esb_voice_node

# Configure MY_NODE_ID in src/main.c (1 for Node A, 2 for Node B, 3 for Node C)

# Build for the nRF52840 Dongle
west build -b nrf52840dongle_nrf52840

# Flash onto the dongle via DFU bootloader (press reset button on dongle)
nrfutil dfu usb-serial -pkg build/zephyr/zephyr.zip -p /dev/tty.usbmodemXXXX
```

### 2. Building Contiki-NG Nodes

Prerequisites: [Contiki-NG toolchain](https://docs.contiki-ng.org/) and `arm-none-eabi-gcc`.

```bash
cd contiki/00nullnet-test

# Compile for nRF52840 Dongle
make TARGET=nrf52840 BOARD=dongle

# Package into DFU zip and flash using nrfutil
nrfutil pkg generate --hw-version 52 --sd-req 0x00 --application build/nrf/nrf52840/dongle/main.hex build/nrf/nrf52840/dongle/dongle-dfu-image.zip
nrfutil dfu usb-serial -pkg build/nrf/nrf52840/dongle/dongle-dfu-image.zip -p /dev/tty.usbmodemXXXX
```

### 3. Building ESP32-C6 Firmware

#### Using ESP-IDF:
```bash
cd tests/esp32-C6/ESP-IDF/Loopback_no_Codec
idf.py set-target esp32c6
idf.py build
idf.py flash monitor
```

#### Using Arduino / PlatformIO:
Open any sketch in `tests/esp32-C6/Arduino/` using Arduino IDE or PlatformIO with the `esp32` board definitions installed.

### 4. Running Python Audio Diagnostics

To monitor, dump, or decode incoming serial audio streams on your computer:
```bash
cd tests/Codecs_test/Serial_Rx_scripts
python3 -m venv .venv
source .venv/bin/activate
pip install pyserial pyaudio numpy

# Example: test 4-bit ADPCM streaming from serial port
python3 4bit_ADPCM.py
```

---

## Current Roadmap

- [x] Full-duplex I2S audio drivers on nRF52840 (Zephyr) and ESP32-C6 (ESP-IDF).
- [x] Multi-hop mesh routing logic with TTL decrementing and loop detection on Nordic ESB.
- [x] Codec2 vocoder integration on ESP32-C6 for low-bandwidth links.
- [x] Hardware PTT targeting (Unicast vs. Broadcast) on nRF52840 Dongle.
- [x] Contiki-NG 802.15.4 / NullNet and low-level I2S loopback testing.
- [ ] Adaptive jitter buffer to handle out-of-order and dropped packets across multi-hop paths.
- [ ] Dynamic routing protocol (e.g. AODV-Lite or RPL) replacing static hop tables.
- [ ] End-to-end voice streaming over long-range LoRa using Codec2 (1200 / 2400 bps).
- [ ] Hardware cryptographic encryption for voice packets (AES-128 via nRF CryptoCell / ESP32-C6 hardware crypto engine).

---

## Contributing & License

Contributions, issues, and experiment PRs are welcome. Please ensure that PRs modifying firmware retain hardware overlay compatibility and keep build output files out of git tracking.
