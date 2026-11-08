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
    void drawTrack(const SpotifyTrack &track, bool forceFullRedraw);
    void updateProgress(uint32_t progressMs, uint32_t durationMs);
    static void setupJpgRender(uint16_t origW, uint16_t origH);

private:
    TFT_eSPI _tft;
    String _lastRenderedTrackId;
    int _lastProgressW;
};
