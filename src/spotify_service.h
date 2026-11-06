#pragma once

#include <Arduino.h>

struct SpotifyTrack {
    bool isPlaying = false;
    String id = "";
    String title = "";
    String artist = "";
    String album = "";
    String imageUrl = "";
    uint32_t progressMs = 0;
    uint32_t durationMs = 0;
    bool hasData = false;
};

class SpotifyService {
public:
    SpotifyService();
    bool refreshAccessToken();
    bool getCurrentlyPlaying(SpotifyTrack &track);
    bool downloadAlbumArt(const String &url, int16_t x, int16_t y);

    bool hasValidToken() const { return _accessToken.length() > 0; }

private:
    String _accessToken;
    unsigned long _tokenExpiresAt;
    
    // Método helper para peticiones seguras
    bool sendTokenRequest(String &newToken, uint32_t &expiresIn);
};
