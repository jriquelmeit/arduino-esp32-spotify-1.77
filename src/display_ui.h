#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "spotify_service.h"

class DisplayUI {
public:
    DisplayUI();
    void begin();
    void showBootScreen(const char* status);
    void updateBootProgress(const char* message, uint8_t dots);
    void drawIdleScreen();
    void drawHeader(bool wifiConnected);
    void drawTrack(const SpotifyTrack &track, bool forceFullRedraw);
    void updateProgress(uint32_t progressMs, uint32_t durationMs);
    void updatePlaybackControls(bool isPlaying);

private:
    TFT_eSPI _tft;
    TFT_eSprite _progressSprite;
    String _lastRenderedTrackId;
    bool _lastPlayingState;

    void formatTime(uint32_t ms, char *buffer, size_t bufSize);
};
