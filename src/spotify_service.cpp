#include "spotify_service.h"
#include "config.h"
#include "display_ui.h"
#include <ArduinoJson.h>
#include <TJpg_Decoder.h>
#include <LittleFS.h>

#if defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESP8266HTTPClient.h>
    #include <WiFiClientSecureBearSSL.h>
#elif defined(ESP32)
    #include <WiFi.h>
    #include <HTTPClient.h>
    #include <WiFiClientSecure.h>
#endif

SpotifyService::SpotifyService() : _tokenExpiresAt(0) {}

bool SpotifyService::sendTokenRequest(String &newToken, uint32_t &expiresIn) {
#if defined(ESP8266)
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();
    client->setBufferSizes(1024, 512); // Buffer SSL compacto para ahorrar RAM en ESP8266
    HTTPClient https;
    if (!https.begin(*client, "https://accounts.spotify.com/api/token")) {
        return false;
    }
#else
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    if (!https.begin(client, "https://accounts.spotify.com/api/token")) {
        return false;
    }
#endif

    https.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "grant_type=refresh_token&refresh_token=";
    postData += SPOTIFY_REFRESH_TOKEN;
    postData += "&client_id=";
    postData += SPOTIFY_CLIENT_ID;
    postData += "&client_secret=";
    postData += SPOTIFY_CLIENT_SECRET;

    int httpCode = https.POST(postData);
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[Spotify] Error al renovar token. HTTP: %d\n", httpCode);
        https.end();
        return false;
    }

    WiFiClient *stream = https.getStreamPtr();
    JsonDocument filter;
    filter["access_token"] = true;
    filter["expires_in"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, *stream, DeserializationOption::Filter(filter));
    https.end();

    if (err) {
        Serial.printf("[Spotify] Error parseando respuesta token: %s\n", err.c_str());
        return false;
    }

    newToken = doc["access_token"].as<String>();
    expiresIn = doc["expires_in"] | 3600;
    return (newToken.length() > 0);
}

bool SpotifyService::refreshAccessToken() {
    Serial.println(F("[Spotify] Renovando Access Token..."));
    String newToken;
    uint32_t expiresIn = 3600;

    if (sendTokenRequest(newToken, expiresIn)) {
        _accessToken = newToken;
        // Restar 120 seg para renovar preventivamente antes de que expire
        _tokenExpiresAt = millis() + ((expiresIn > 120 ? expiresIn - 120 : expiresIn) * 1000UL);
        Serial.println(F("[Spotify] Access Token obtenido con exito."));
        return true;
    }

    return false;
}

bool SpotifyService::getCurrentlyPlaying(SpotifyTrack &track) {
    if (!hasValidToken() || millis() >= _tokenExpiresAt) {
        if (!refreshAccessToken()) {
            return false;
        }
    }

#if defined(ESP8266)
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();
    client->setBufferSizes(2048, 512);
    HTTPClient https;
    if (!https.begin(*client, "https://api.spotify.com/v1/me/player/currently-playing")) {
        return false;
    }
#else
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    if (!https.begin(client, "https://api.spotify.com/v1/me/player/currently-playing")) {
        return false;
    }
#endif

    String authHeader = "Bearer " + _accessToken;
    https.addHeader("Authorization", authHeader.c_str());

    int httpCode = https.GET();

    // 204 No Content: No hay nada reproduciéndose
    if (httpCode == 204) {
        track.isPlaying = false;
        track.hasData = false;
        https.end();
        return true;
    }

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[Spotify] GET currently-playing HTTP: %d\n", httpCode);
        if (httpCode == 401) {
            _accessToken = ""; // Forzar refresco en la siguiente vuelta
        }
        https.end();
        return false;
    }

    // Filtrar solo las propiedades exactas para minimizar uso de memoria JSON
    JsonDocument filter;
    filter["is_playing"] = true;
    filter["progress_ms"] = true;
    filter["item"]["id"] = true;
    filter["item"]["name"] = true;
    filter["item"]["duration_ms"] = true;
    filter["item"]["artists"][0]["name"] = true;
    filter["item"]["album"]["name"] = true;
    filter["item"]["album"]["images"][0]["url"] = true;
    filter["item"]["album"]["images"][1]["url"] = true;
    filter["item"]["album"]["images"][2]["url"] = true;

    WiFiClient *stream = https.getStreamPtr();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, *stream, DeserializationOption::Filter(filter));
    https.end();

    if (err) {
        Serial.printf("[Spotify] Error parseando currently-playing: %s\n", err.c_str());
        return false;
    }

    track.hasData = true;
    track.isPlaying = doc["is_playing"] | false;
    track.progressMs = doc["progress_ms"] | 0;

    JsonObject item = doc["item"];
    if (!item.isNull()) {
        track.id = item["id"].as<String>();
        track.title = item["name"].as<String>();
        track.durationMs = item["duration_ms"] | 0;

        JsonArray artists = item["artists"];
        if (artists.size() > 0) {
            track.artist = artists[0]["name"].as<String>();
        }

        // Seleccionar la imagen mediana (300x300, índice 1) para alta nitidez visual
        JsonArray images = item["album"]["images"];
        if (images.size() > 0) {
            // El índice 1 suele ser de 300x300 píxeles. Si no existe, usar la primera disponible
            int targetIdx = (images.size() >= 2) ? 1 : 0;
            track.imageUrl = images[targetIdx]["url"].as<String>();
        }
    }

    return true;
}

bool SpotifyService::downloadAlbumArt(const String &url, int16_t x, int16_t y) {
    if (url.length() == 0) return false;

#if defined(ESP8266)
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();
    client->setBufferSizes(2048, 512);
    HTTPClient https;
    if (!https.begin(*client, url)) {
        return false;
    }
#else
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    if (!https.begin(client, url)) {
        return false;
    }
#endif

    int httpCode = https.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[Spotify] Error descargando portada: %d\n", httpCode);
        https.end();
        return false;
    }

    int len = https.getSize();
    if (len <= 0) {
        https.end();
        return false;
    }

    WiFiClient *stream = https.getStreamPtr();

    // 1. Intentar descargar en buffer RAM si hay memoria disponible (rápido)
    uint8_t *jpgBuffer = (uint8_t *)malloc(len);
    if (jpgBuffer) {
        int bytesRead = stream->readBytes(jpgBuffer, len);
        https.end();

        if (bytesRead == len) {
            uint16_t origW = 0, origH = 0;
            TJpgDec.getJpgSize(&origW, &origH, jpgBuffer, len);
            DisplayUI::setupJpgRender(origW, origH);
            TJpgDec.drawJpg(0, 0, jpgBuffer, len);
            free(jpgBuffer);
            return true;
        }
        free(jpgBuffer);
        return false;
    }

    // 2. Si no hubo RAM contigua suficiente (en ESP8266), descargar a LittleFS en bloques de 512B
    Serial.println(F("[Spotify] RAM contigua insuficiente, usando LittleFS para portada HD..."));
    LittleFS.begin();
    File f = LittleFS.open("/art.jpg", "w");
    if (!f) {
        Serial.println(F("[Spotify] Error creando archivo LittleFS"));
        https.end();
        return false;
    }

    uint8_t chunk[512];
    int remaining = len;
    while (remaining > 0 && https.connected()) {
        size_t availableBytes = stream->available();
        if (availableBytes > 0) {
            size_t bytesToRead = (availableBytes < sizeof(chunk)) ? availableBytes : sizeof(chunk);
            if (bytesToRead > (size_t)remaining) bytesToRead = remaining;
            int readCount = stream->readBytes(chunk, bytesToRead);
            if (readCount > 0) {
                f.write(chunk, readCount);
                remaining -= readCount;
            }
        } else {
            delay(5);
        }
    }
    f.close();
    https.end();

    if (remaining == 0) {
        uint16_t origW = 0, origH = 0;
        TJpgDec.getFsJpgSize(&origW, &origH, "/art.jpg", LittleFS);
        DisplayUI::setupJpgRender(origW, origH);
        TJpgDec.drawFsJpg(0, 0, "/art.jpg", LittleFS);
        LittleFS.remove("/art.jpg");
        return true;
    }

    LittleFS.remove("/art.jpg");
    return false;
}
