# Spotify Mini Player TFT 1.44" (ST7735 128x128)

Visor de reproducción en tiempo real para **Spotify** utilizando un microcontrolador **ESP8266 (NodeMCU)** o **ESP32** conectado a una pantalla **TFT LCD 1.44" SPI ST7735 (128x128)**.

Renderiza la carátula oficial del álbum (64x64 px), título de la canción, artista, barra de progreso interactiva con tiempo transcurrido/total, y controles de reproducción (Shuffle, Prev, Play/Pause, Next, Like) con actualización fluida mediante Sprites y decodificación JPEG directa a pantalla con `TFT_eSPI` y `TJpg_Decoder`.

---

## Hardware Soportado

- **Placa:** NodeMCU V2/V3 (ESP8266 @ 160MHz) o ESP32 (WROOM / NodeMCU-32S).
- **Pantalla:** TFT LCD 1.44 pulgadas SPI (Controlador ST7735, resolución 128x128 píxeles).

---

## Conexión de Pines (Pinout)

### Mapeo para NodeMCU ESP8266 (Lolin V3 / V2)

| Pin Pantalla TFT 1.44" | Pin NodeMCU (ESP8266) | Pin GPIO | Descripción |
| :--- | :--- | :--- | :--- |
| **GND** | `GND` | `GND` | Tierra |
| **VCC** | `VIN` (5V) o `3V3` | - | Alimentación |
| **SCK / SCL** | `D5` | `GPIO14` | SPI Hardware Clock (HSPI SCK) |
| **SDA / MOSI** | `D7` | `GPIO13` | SPI Hardware Data (HSPI MOSI) |
| **RES / RESET** | `D4` | `GPIO2` | Reset de pantalla |
| **A0 / DC** | `D3` | `GPIO0` | Data / Command |
| **CS** | `D8` | `GPIO15` | Chip Select |
| **LED / BLK** | `3V3` | - | Retroiluminación (Backlight) |

*(Para **ESP32**, ver la sección `[env:esp32dev]` en `platformio.ini`: CS=`GPIO15`, DC=`GPIO2`, RST=`GPIO4`, MOSI=`GPIO23`, SCK=`GPIO18`)*.

---

## Guía Paso a Paso: Cómo obtener las credenciales de Spotify

Para que el dispositivo consulte qué canción estás escuchando en tu cuenta de Spotify, necesitas obtener tres credenciales:
1. `SPOTIFY_CLIENT_ID`
2. `SPOTIFY_CLIENT_SECRET`
3. `SPOTIFY_REFRESH_TOKEN`

### Paso 1: Crear una App en Spotify Developer Dashboard
1. Ve al [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) e inicia sesión con tu cuenta de Spotify.
2. Haz clic en **Create App**.
3. Completa los campos:
   - **App name:** `ESP32 Spotify Display` (o el nombre que prefieras).
   - **App description:** `Visor de pantalla TFT para Spotify`.
   - **Redirect URIs:** Agrega exactamente: `http://localhost:8888/callback` y presiona **Add**.
   - Marca la casilla de aceptación de los Términos de Servicio y haz clic en **Save**.
4. En la página de tu app, entra a **Settings**:
   - Copia tu **Client ID**.
   - Haz clic en **View client secret** y copia tu **Client Secret**.

---

### Paso 2: Generar el `SPOTIFY_REFRESH_TOKEN`

El refresh token le da permiso al ESP para leer el estado de tu reproductor. Para obtenerlo:

#### Opción A (Mediante tu navegador y curl):
1. Reemplaza `TU_CLIENT_ID` en la siguiente URL y ábrela en tu navegador web:
   ```text
   https://accounts.spotify.com/authorize?response_type=code&client_id=TU_CLIENT_ID&scope=user-read-playback-state%20user-read-currently-playing&redirect_uri=http://localhost:8888/callback
   ```
2. Inicia sesión y autoriza la aplicación.
3. El navegador intentará cargar `http://localhost:8888/callback?code=AQ...` (la página puede decir "no se puede conectar", no importa). Copia todo el valor después de `code=` en la barra de direcciones (hasta antes de cualquier `&` si hubiera). Ese es tu `AUTHORIZATION_CODE`.
4. Abre una terminal y ejecuta el siguiente comando reemplazando los valores:
   ```bash
   curl -d client_id="TU_CLIENT_ID" \
        -d client_secret="TU_CLIENT_SECRET" \
        -d grant_type="authorization_code" \
        -d code="TU_AUTHORIZATION_CODE" \
        -d redirect_uri="http://localhost:8888/callback" \
        https://accounts.spotify.com/api/token
   ```
5. En la respuesta JSON que recibirás, copia el valor de `"refresh_token"`. Ese token no caduca a menos que revoques el acceso en tu cuenta.

---

## Configuración del Proyecto

1. Clona o entra a la carpeta del proyecto:
   ```bash
   cd arduino-esp32-spotify-1.44
   ```
2. Crea tu archivo privado `include/secrets.h` a partir de la plantilla:
   ```bash
   cp include/secrets.h.example include/secrets.h
   ```
3. Abre `include/secrets.h` y coloca tus datos reales:
   ```cpp
   #define WIFI_SSID           "Tu_Red_WiFi"
   #define WIFI_PASSWORD       "Tu_Password"

   #define SPOTIFY_CLIENT_ID       "tu_client_id"
   #define SPOTIFY_CLIENT_SECRET   "tu_client_secret"
   #define SPOTIFY_REFRESH_TOKEN   "tu_refresh_token"
   ```

*(El archivo `secrets.h` está en el `.gitignore` por lo que tus credenciales nunca se subirán a GitHub).*

---

## Compilación y Subida (PlatformIO)

Compilar y subir a NodeMCU ESP8266:
```bash
pio run -t upload
```

Abrir el monitor serie para ver diagnósticos:
```bash
pio device monitor -b 115200
```
