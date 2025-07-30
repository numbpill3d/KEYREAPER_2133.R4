# KEYREAPER_2133.R4

**KEYREAPER 2133.R4** is a covert, browser-controlled RFID/NFC reader and writer built on the **Arduino Uno R4 WiFi** with a **PN532 I2C shield**. It creates its own WiFi network and serves an embedded web interface for scanning, writing, and interrogating RFID/NFC tags in the field — no laptop required beyond initial connection.

> A stealth tool for tag inspection, manipulation, and dumping — no cloud, no external dependencies.

---

## 🛠 Hardware

- ✅ Arduino Uno R4 WiFi (Renesas RA4M1 + u-blox NINA-W102)
- ✅ Adafruit PN532 RFID/NFC Shield (13.56 MHz, I2C mode)
- ✅ USB-C power supply or battery bank
- ❌ No SD card required
- ❌ No internet required

---

## ⚙️ Features

- 🔍 **Reads RFID/NFC tags** (ISO14443A/B, MIFARE, FeliCa)
- 🔑 **Brute-force key discovery** using common key dictionary
- 📦 **Block dumping** of up to 32 data blocks per tag
- ✍️ **Web-based tag writing** via POST request
- 🧠 **History buffer** (last 20 UIDs + metadata)
- 🌐 **Self-hosted web UI over local AP**
- 🧪 **Deep scan** + **aggressive protocol mode**
- 🔌 **Plug and play** — just power it on and connect

---

## 🌐 Network Mode

When powered on, KEYREAPER creates its own WiFi network:

- **SSID:** `KEYREAPER_2133.R4` (default)
- **Password:** `21332133` (editable in code)
- **IP Address:** `192.168.4.1`
- **Port:** `80`

Use your phone or laptop to connect to the device’s WiFi, then visit:

http://192.168.4.1


---

## 📱 Web Interface

Control the device fully through a mobile or desktop browser:

### Endpoints:
- `/api/scan` — Single scan
- `/api/toggle` — Continuous scan mode
- `/api/aggressive` — Multi-protocol scan toggle
- `/api/history` — Card log JSON
- `/api/write` — POST hex block payloads

---

## 🧾 History Format

Each scanned tag logs:
- UID
- Type (ISO A/B/FeliCa)
- Manufacturer ID
- Key count cracked
- Block data (if dumped)
- Timestamp

---

## 🚀 Setup Instructions

1. Clone the repo:
   ```bash
   git clone https://github.com/numbpill3d/KEYREAPER_2133.R4.git


2. Open in Arduino IDE

Board: Arduino Uno R4 WiFi
Port: Auto-detect

3. Edit main.ino to change SSID/password if needed

4. Upload the sketch

5. Power the device → connect to its WiFi from your phone

6. Visit http://192.168.4.1 in your browser

## 🔐 Write Mode

To write to a tag:

- Use the write section in the UI

- Must be authenticated with a cracked key

- Supports writing raw hex blocks (plaintext or via POST)

⚠️ Legal
This project is for educational and authorized use only.
Do not use on systems or tags you do not own or have explicit permission to test.

🧠 Author
by numbpill3d AKA voidrane
Device theory + field engineering and components sourced + provided by Xenotrek

📃 License
MIT — mod it, fork it, flash it.

📑 [Contributing Guidelines](./CONTRIBUTING.md)

## ⚠️ Compiler Warnings

In Arduino IDE, set:
> **File → Preferences → Compiler Warnings → All**

This enables full warning output during build to help catch bugs and maintain quality.

## 🧹 Linting

We use [`cpplint`](https://github.com/cpplint/cpplint) to maintain code quality.

To run it:


pip install cpplint
cpplint src/*.cpp src/*.h


## 🐞 Bug Reporting

Found a bug or unexpected behavior?  
Please open a new issue here:  
👉 [https://github.com/numbpill3d/KEYREAPER_2133.R4/issues/new?assignees=&labels=bug&template=bug_report.md&title=%5BBUG%5D+](https://github.com/numbpill3d/KEYREAPER_2133.R4/issues/new?assignees=&labels=bug&template=bug_report.md&title=%5BBUG%5D+)

## 🐞 Report Bugs / Suggest Features

Found a bug or want to suggest a feature?

→ [Open an Issue on GitHub](https://github.com/numbpill3d/KEYREAPER_2133.R4/issues)

## 🧪 Running the Test Suite

This project uses [AUnit](https://github.com/bxparks/AUnit) for automated testing.

### 📦 Dependencies:
- Arduino IDE or PlatformIO
- AUnit library

### 🛠️ Run Tests (Arduino IDE):

1. Open `test/test_main.cpp` in Arduino IDE.
2. Select your board: Arduino Uno R4 WiFi.
3. Upload the sketch.
4. Open Serial Monitor to view test results.

### 🧪 Run Tests (PlatformIO):

pio test -e uno_r4_wifi

Make sure your platformio.ini defines the test environment.


---

## ✅ Step 3: Use FLOSS License

Since AUnit is **Apache 2.0 licensed**, you're good — this satisfies the **FLOSS requirement**.

---

## ✅ Optional: Add a CI File (GitHub Actions)

Later, if you want to automate this fully:

yaml
# .github/workflows/test.yml
name: Arduino Tests

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install PlatformIO
        run: pip install platformio
      - name: Run Unit Tests
        run: pio test

[![OpenSSF Best Practices](https://www.bestpractices.dev/projects/10978/badge)](https://www.bestpractices.dev/projects/10978)
