#include <Arduino.h>

#if defined(ESP8266)
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <WiFi.h>
#endif

#include "config.h"
#include "spotify_service.h"
#include "display_ui.h"

static DisplayUI ui;
static SpotifyService spotify;
static SpotifyTrack currentTrack;

static unsigned long lastPollTime = 0;
static unsigned long lastLocalTick = 0;
static unsigned long lastWifiCheck = 0;
static String currentAlbumArtUrl = "";
static bool isIdle = true;

void connectWiFi() {
    Serial.println(F("\n========================================"));
    Serial.printf("Conectando a Wi-Fi: %s\n", WIFI_SSID);
    Serial.println(F("========================================"));

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint8_t dots = 0;
    unsigned long startAttempt = millis();

    while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt < 25000)) {
        delay(400);
        Serial.print(F("."));
        ui.updateBootProgress("Conectando Wi-Fi", dots++);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("\n[Wi-Fi] Conectado exitosamente!"));
        Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
        ui.updateBootProgress("Wi-Fi Conectado", 0);
    } else {
        Serial.println(F("\n[Wi-Fi] Tiempo de espera agotado. Continuando en segundo plano..."));
        ui.updateBootProgress("Sin conexion Wi-Fi", 0);
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println(F("\n=============================================="));
    Serial.println(F("   Spotify Mini Display 1.44\" ST7735 (128x128)"));
    Serial.println(F("=============================================="));

    // 1. Inicializar pantalla TFT y sprites
    ui.begin();
    ui.showBootScreen("Iniciando...");

    // 2. Conectar a red Wi-Fi
    connectWiFi();

    // 3. Obtener token inicial de Spotify si hay conexión
    if (WiFi.status() == WL_CONNECTED) {
        ui.updateBootProgress("Spotify Login", 0);
        if (spotify.refreshAccessToken()) {
            ui.updateBootProgress("Spotify Listo", 0);
        } else {
            ui.updateBootProgress("Error Token Spotify", 0);
        }
        delay(800);
    }

    ui.drawIdleScreen();
}

void loop() {
    unsigned long now = millis();

    // 1. Verificación periódica de Wi-Fi
    if (now - lastWifiCheck >= WIFI_RECONNECT_INTERVAL_MS) {
        lastWifiCheck = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println(F("[Wi-Fi] Reconectando..."));
            WiFi.reconnect();
        }
    }

    // 2. Avance suave del segundero / progreso local (cada 1 segundo si está reproduciendo)
    if (currentTrack.isPlaying && (now - lastLocalTick >= LOCAL_TICK_INTERVAL_MS)) {
        lastLocalTick = now;
        if (currentTrack.progressMs + 1000 <= currentTrack.durationMs) {
            currentTrack.progressMs += 1000;
        }
        ui.updateProgress(currentTrack.progressMs, currentTrack.durationMs);
    }

    // 3. Consulta periódica a Spotify API
    unsigned long pollInterval = currentTrack.isPlaying ? 
                                 SPOTIFY_POLL_INTERVAL_PLAYING_MS : 
                                 SPOTIFY_POLL_INTERVAL_IDLE_MS;

    if (now - lastPollTime >= pollInterval) {
        lastPollTime = now;

        if (WiFi.status() == WL_CONNECTED) {
            SpotifyTrack newTrack;
            if (spotify.getCurrentlyPlaying(newTrack)) {
                if (newTrack.hasData) {
                    bool isNewSong = (currentTrack.id != newTrack.id);
                    currentTrack = newTrack;
                    isIdle = false;

                    // Dibujar textos, barra y controles
                    ui.drawTrack(currentTrack, isNewSong);

                    // Descargar y mostrar carátula si cambió la canción
                    if (isNewSong && currentTrack.imageUrl.length() > 0) {
                        Serial.printf("[Spotify] Descargando portada: %s\n", currentTrack.imageUrl.c_str());
                        spotify.downloadAlbumArt(currentTrack.imageUrl, 32, 22);
                    }
                } else {
                    // No hay reproducción activa (idle)
                    if (!isIdle) {
                        isIdle = true;
                        currentTrack.id = "";
                        currentTrack.isPlaying = false;
                        ui.drawIdleScreen();
                    }
                }
            }
        }
    }

    delay(20);
}
