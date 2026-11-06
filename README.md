# Spotify Mini Player TFT 1.77" (ST7735 128x160)

Visor de reproducción en tiempo real para **Spotify** utilizando un microcontrolador **ESP8266 (NodeMCU)** o **ESP32** conectado a una pantalla **TFT LCD 1.77" SPI ST7735 (128x160)**.

Renderiza la carátula oficial del álbum (64x64 px), título de la canción, artista, barra de progreso interactiva con tiempo transcurrido/total, y controles de reproducción (Shuffle, Prev, Play/Pause, Next, Like) con actualización fluida mediante Sprites y decodificación JPEG directa a pantalla con `TFT_eSPI` y `TJpg_Decoder`.

---

## Hardware Soportado

- **Placa:** NodeMCU V2/V3 (ESP8266 @ 160MHz) o ESP32 (WROOM / NodeMCU-32S).
- **Pantalla:** TFT LCD 1.77 pulgadas SPI (Controlador ST7735 / ST7735S, resolución 128x160 píxeles).

---

## Conexión de Pines (Pinout)

### Mapeo para NodeMCU ESP8266 (Lolin V3 / V2)

| Pin Pantalla TFT 1.77" | Pin NodeMCU (ESP8266) | Pin GPIO | Descripción |
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

---

### Paso 1: Crear una App en Spotify Developer Dashboard y obtener Client ID / Secret
1. Ve al [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) e inicia sesión con tu cuenta de Spotify.
2. Haz clic en el botón **Create App** (arriba a la derecha).
3. Completa el formulario:
   - **App name:** `ESP32 Spotify Display` (o el nombre que gustes).
   - **App description:** `Visor de pantalla TFT para Spotify`.
   - **Redirect URIs:** Escribe exactamente: `http://localhost:8888/callback` y presiona el botón **Add** para que quede en la lista.
   - **Which API/SDKs are you planning to use?:** Selecciona la casilla **Web API**.
   - Marca la casilla aceptando los términos de servicio para desarrolladores.
   - Haz clic en **Save**.
4. Una vez creada la app, entra a **Settings**:
   - En la pestaña **Basic Information** verás tu **Client ID**. Cópialo.
   - Justo debajo, haz clic en **View client secret** para ver tu **Client Secret**. Cópialo.

---

### Paso 2: Generar el `SPOTIFY_REFRESH_TOKEN`

El `Refresh Token` es un token permanente que autoriza al ESP a leer el estado de tu reproductor en segundo plano sin pedirte login cada hora.

Puedes obtenerlo fácilmente con cualquiera de estos dos métodos:

#### Método Rápido A (Navegador + cURL en Terminal)

1. **Obtener el código de autorización:**
   Copia la siguiente URL, **reemplaza `TU_CLIENT_ID`** por tu Client ID real y ábrela en tu navegador:
   ```text
   https://accounts.spotify.com/authorize?response_type=code&client_id=TU_CLIENT_ID&scope=user-read-playback-state%20user-read-currently-playing&redirect_uri=http://localhost:8888/callback
   ```
2. Inicia sesión con tu cuenta de Spotify y presiona **Aceptar / Agree**.
3. El navegador te redirigirá a una página que probablemente dirá *"No se puede conectar"* a `http://localhost:8888/callback?code=AQ...` (esto es normal porque no hay un servidor local corriendo).
4. **Copia el código de la barra de direcciones:** 
   En la URL de tu navegador verás algo como:
   `http://localhost:8888/callback?code=AQD...un_codigo_largo...`
   Copia todo el texto que está **después de `code=`** (y antes de cualquier `#` o `&` si existiera). Ese es tu `AUTHORIZATION_CODE`.
   *(Nota: este código expira en 5 minutos, úsalo de inmediato en el paso siguiente).*

5. **Intercambiar el código por el Refresh Token:**
   Abre tu terminal y ejecuta el siguiente comando reemplazando `TU_CLIENT_ID`, `TU_CLIENT_SECRET` y el código `TU_AUTHORIZATION_CODE`:
   ```bash
   curl -X POST https://accounts.spotify.com/api/token \
        -d grant_type=authorization_code \
        -d client_id="TU_CLIENT_ID" \
        -d client_secret="TU_CLIENT_SECRET" \
        -d code="TU_AUTHORIZATION_CODE" \
        -d redirect_uri="http://localhost:8888/callback"
   ```

6. **Copiar el Refresh Token:**
   La respuesta en la terminal será un JSON similar a este:
   ```json
   {
      "access_token": "BQC...",
      "token_type": "Bearer",
      "expires_in": 3600,
      "refresh_token": "AQB...ESTE_ES_EL_VALOR_A_COPIAR...",
      "scope": "user-read-currently-playing user-read-playback-state"
   }
   ```
   Copia el valor de `"refresh_token"`. ¡Este token es permanente y no caduca!

---

#### Método Alternativo B (Generador Web Asistido)

Si prefieres no usar la terminal cURL:
1. Agrega temporalmente en los **Redirect URIs** de tu app de Spotify en el dashboard la URI: `https://developer.spotify.com/documentation/web-api/tutorials/code-flow` o una herramienta comunitaria confiable de token generation como [Spotify Token Generator](https://get-spotify-refresh-token.vercel.app/).
2. Ingresa tu `Client ID`, `Client Secret` y marca los scopes `user-read-playback-state` y `user-read-currently-playing`.
3. Haz clic en autorizar y la herramienta te entregará el `refresh_token` listo para copiar.

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
