# AGENTS.md

Firmware for an ESP8266 / ESP32 Spotify playback viewer rendered on a 1.77" TFT LCD (ST7735 / ST7735S, 128x160).

---

## 1. Build, Upload & Monitor

Uses **PlatformIO Core (`pio`)**.

```bash
# Build default environment (NodeMCU ESP8266)
pio run

# Build ESP32 environment
pio run -e esp32dev

# Upload to device
pio run -t upload                     # ESP8266
pio run -e esp32dev -t upload         # ESP32

# Serial monitor (115200 baud)
pio device monitor -b 115200

# Clean build artifacts
pio run -t clean
```

There is no automated test runner in this repository; verification is performed via `pio run` (compilation and memory budget check) and serial logs.

---

## 2. Hardware & TFT Configuration Quirks

- **Pin definition source of truth:** All TFT pins and display controller parameters are set via `-D` flags in `platformio.ini`, **not** in TFT_eSPI's `User_Setup.h` (`USER_SETUP_LOADED=1`). Never edit TFT_eSPI library headers.
- **NodeMCU ESP8266 pins (HSPI):**
  - SCK: `D5` (GPIO14), MOSI: `D7` (GPIO13), CS: `D8` (GPIO15), DC/A0: `D3` (GPIO0), RST: `D4` (GPIO2).
  - CPU is clocked at 160MHz (`board_build.f_cpu = 160000000L`) to handle TLS/SSL and JPEG decoding.
- **ESP32 pins (VSPI):**
  - SCK: `GPIO18`, MOSI: `GPIO23`, CS: `GPIO15`, DC: `GPIO2`, RST: `GPIO4`.
  - Flash usage is ~97% under default partition table; watch binary size when adding libraries or features.

---

## 3. Configuration & Secrets

- Credentials live in `include/secrets.h` (`WIFI_SSID`, `WIFI_PASSWORD`, `SPOTIFY_CLIENT_ID`, `SPOTIFY_CLIENT_SECRET`, `SPOTIFY_REFRESH_TOKEN`).
- `include/secrets.h` is gitignored. If missing, `include/config.h` falls back to `include/secrets.h.example` with a compiler warning.
- Never commit `include/secrets.h` or real OAuth credentials.

---

## 4. Architecture & Rendering Pipeline

- **Entry point (`src/main.cpp`):**
  - Boot sequence: initializes TFT, connects Wi-Fi, requests initial OAuth token, enters idle screen.
  - Non-blocking loop: periodic Wi-Fi check (10s), 1-second local progress interpolation (`LOCAL_TICK_INTERVAL_MS`), and dynamic Spotify polling (2.5s while playing, 5s when idle).
- **Display UI (`src/display_ui.cpp`, `src/display_ui.h`, `include/config.h`):**
  - Resolution: 128x160 px, rotation 0 (`TFT_ROTATION`).
  - Colors are 16-bit RGB565 macros in `config.h` (e.g. `COLOR_SPOTIFY_GREEN 0x1DB9`, `COLOR_BG_DARK 0x10A2`).
  - Vertical layout:
    - `Y: 0..127`: Album art area (128x128 px, decoded from 300x300 JPEG at 1/2 scale with center crop for high-definition rendering, with fallback to LittleFS on low RAM).
    - `Y: 128..129`: Integrated 2 px thin progress bar across the 128 px width.
    - `Y: 130..159`: Footer area with Song title (Y:134, max 20 chars) and artist (Y:147, max 20 chars).
    - Playback control buttons removed to optimize space for the larger album art.
- **Spotify & JPEG service (`src/spotify_service.cpp`, `src/spotify_service.h`):**
  - Uses `WiFiClientSecure` with `setInsecure()` (no root CA certs) to avoid memory exhaustion and cert expiration.
  - ESP8266 BearSSL buffers are explicitly constrained: `(1024, 512)` for token requests, `(2048, 512)` for GET player/image.
  - JSON parsing uses ArduinoJson v7 filter masks to deserialize only required fields directly from `WiFiClient* stream`.
  - Album art downloads stream into a temporary `malloc()` buffer and render to screen via `TJpgDec.drawJpg()`; buffer is freed immediately. `TJpgDec.setSwapBytes(true)` is required for correct TFT_eSPI color rendition.
