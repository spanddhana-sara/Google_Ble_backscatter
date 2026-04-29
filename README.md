# Google Find My Backscatter

A university research project demonstrating **Google Find My Device Network (FMDN)** compatible BLE advertisements using two approaches:
1. **Passive BLE Backscatter** on BeagleBone Black (no BLE radio required)
2. **Active BLE Advertisement** on ESP32 / ESP32-C3

---

## What This Project Does

Google's Find My Device Network allows Android phones to crowdsource the location of nearby Bluetooth trackers. This project demonstrates that a valid FMDN-compatible BLE advertisement can be generated and transmitted using:

- A **BeagleBone Black** with its onboard PRU (Programmable Real-time Unit) — backscattering an external 2.4 GHz carrier signal to synthesize BLE packets **without any BLE radio chip**
- An **ESP32 / ESP32-C3** using standard active BLE advertisement for comparison

EID (Ephemeral Identifier) rotation is implemented using HMAC-SHA256, matching the rotation schedule of real Google trackers (~17 minutes / 1024 seconds).

---

## Repository Structure

```
.
├── beaglebone/
│   ├── ble.c                  # FMDN packet builder + EID rotation + BLE PHY layer
│   ├── blebackscatter.c       # Main PRU backscatter loop
│   ├── blebackscatter.h       # Structs, constants, function declarations
│   ├── pru.c / pru.h          # PRU setup and memory management
│   └── Makefile               # Build system
│
├── esp32/
│   └── main.c                 # ESP32 / ESP32-C3 active BLE FMDN advertisement
│
└── README.md
```

---

## FMDN Packet Structure

```
Byte    Value       Description
----    -----       -----------
0       0x02        AD Length
1       0x01        AD Type: Flags
2       0x06        LE General Discoverable | BR/EDR Not Supported
3       0x19        AD Length (25 bytes follow)
4       0x16        AD Type: Service Data - 16-bit UUID
5       0xAA        Google FMDN UUID (0xFEAA, little-endian)
6       0xFE        Google FMDN UUID (cont.)
7       0x40        FMDN Frame Type (0x40 normal / 0x41 tracking protection)
8-27    <EID>       20-byte Ephemeral Identifier (rotates every ~17 min)
28      0x00        Hashed flags
29-30   0x00 0x00   Padding
```

---

## EID Rotation

Real Google trackers rotate their EID every `2^rotation_exponent` seconds (typically exponent = 10, giving 1024 seconds ≈ 17 minutes). This project implements the same schedule:

```
EID = first 20 bytes of HMAC-SHA256(beacon_key, rotation_exponent || quantized_timestamp)
```

The beacon key is generated once using OpenSSL's `RAND_bytes()` and persisted to `beacon_key.bin` so the EID sequence survives reboots.

---

## BeagleBone Black — Build & Run

### Requirements

- BeagleBone Black running Debian
- External 2.4 GHz carrier emitter connected to BBB GPIO
- PRU firmware (`fsk.bin`) loaded
- OpenSSL: `sudo apt-get install libssl-dev`
- prussdrv library

### Build

```bash
cd beaglebone/
make
```

### Run

```bash
sudo ./backscatter
```

### Expected Output

```
[FMDN] Loaded existing beacon key from beacon_key.bin
[FMDN] Beacon Key: 3a f2 91 ...
MAC Address: 10 11 12 13 14 15
[FMDN] adv_data (31 bytes): 02 01 06 19 16 aa fe 40 59 13 ...
PDU length without CRC: 39
PDU length with CRC: 42
Executing PRU program
...
[FMDN] === EID ROTATION ===
[FMDN] New EID: b3 4a 12 ...
```

---

## ESP32 / ESP32-C3 — Build & Run

### Requirements

- ESP-IDF v5.3
- ESP32 or ESP32-C3 target

### Build & Flash

```bash
cd esp32/
idf.py set-target esp32c3   # or esp32
idf.py build
idf.py flash monitor
```

### Configuration

Edit `EID_HEX` in `main.c` with your 20-byte EID (40 hex characters):

```c
const char *eid_string = "5913715d278927cc947106e874066d926a283630";
```

---

## Verifying the Advertisement

Use **nRF Connect** (Android/iOS):

1. Open nRF Connect → Scanner
2. Filter by UUID `0xFEAA`
3. You should see the FMDN advertisement with your EID bytes

---

## Limitations & Future Work

### 1. Google Device Registration
Real FMDN trackers must be provisioned by Google — this requires becoming an official Find My Device partner and receiving a cryptographically derived beacon key from Google's servers. Without this, the advertisement is structurally valid but **Google's network will not generate location reports** for the device.

**Planned:** Investigate Google's FMDN provisioning protocol to obtain a valid beacon key, similar to the OpenHaystack reverse engineering effort for Apple's Find My network.

### 2. Location Recovery
Even with a valid advertisement, recovering location reports requires access to Google's resolver API which is not publicly documented.

**Planned:** Reverse engineer the FMDN location report API to retrieve crowdsourced location data from Google's servers.

### 3. Hashed Flags (Byte 28)
The hashed flags byte is currently `0x00`. In a real tracker this is a truncated HMAC over the EID + status flags. Strict FMDN validators may reject packets with an incorrect hashed flags value.

**Planned:** Implement the hashed flags computation per the FMDN specification.

---

## References

- Google Find My Tools Repo (https://github.com/leonboe1/GoogleFindMyTools)
- Bluetooth Core Specification 5.x — Advertising PDU format

---

## Authors
Spanddhana Sara

