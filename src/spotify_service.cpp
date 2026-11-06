#include "spotify_service.h"
#include "config.h"
#include <ArduinoJson.h>
#include <TJpg_Decoder.h>

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

        // Seleccionar la imagen más pequeña (64x64) típicamente en el índice 2 o último
        JsonArray images = item["album"]["images"];
        if (images.size() > 0) {
            // El último elemento suele ser el de 64x64 píxeles
            int lastIdx = images.size() - 1;
            track.imageUrl = images[lastIdx]["url"].as<String>();
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

    // Usar buffer dinámico o stream directo con TJpg_Decoder
    WiFiClient *stream = https.getStreamPtr();
    uint8_t *jpgBuffer = (uint8_t *)malloc(len);
    if (!jpgBuffer) {
        Serial.println(F("[Spotify] No hay RAM suficiente para buffer JPEG"));
        https.end();
        return false;
    }

    int bytesRead = stream->readBytes(jpgBuffer, len);
    https.end();

    if (bytesRead == len) {
        // TJpg_Decoder renderiza directamente a la pantalla TFT_eSPI
        TJpgDec.drawJpg(x, y, jpgBuffer, len);
        free(jpgBuffer);
        return true;
    }

    free(jpgBuffer);
    return false;
}
