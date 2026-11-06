#include "display_ui.h"
#include "config.h"
#include "spotify_icons.h"
#include <TJpg_Decoder.h>

// Callback global para que TJpg_Decoder envíe bloques a TFT_eSPI
static TFT_eSPI *pTft = nullptr;

bool tftOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (y >= SCREEN_HEIGHT) return 0;
    if (pTft) {
        pTft->pushImage(x, y, w, h, bitmap);
    }
    return 1;
}

DisplayUI::DisplayUI() : _tft(), _progressSprite(&_tft), _lastRenderedTrackId(""), _lastPlayingState(false) {}

void DisplayUI::begin() {
    _tft.init();
    _tft.setRotation(TFT_ROTATION);
    _tft.fillScreen(COLOR_BG_DARK);
    pTft = &_tft;

    // Configurar decodificador JPEG con swap de bytes para TFT SPI
    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback(tftOutputCallback);
    TJpgDec.setSwapBytes(true);

    // Crear sprite en RAM para el progreso y cronómetro (128x14 píxeles = ~3.5 KB)
    _progressSprite.setColorDepth(16);
    _progressSprite.createSprite(128, 14);
}

void DisplayUI::showBootScreen(const char* status) {
    _tft.fillScreen(COLOR_BG_DARK);
    
    // Logo Spotify central
    _tft.drawBitmap(58, 40, epd_bitmap_spotify_logo, 12, 12, COLOR_SPOTIFY_GREEN);
    
    _tft.setTextColor(COLOR_TEXT_WHITE, COLOR_BG_DARK);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString("Spotify Mini", 64, 65, 2);

    _tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG_DARK);
    _tft.drawString(status, 64, 90, 1);
}

void DisplayUI::updateBootProgress(const char* message, uint8_t dots) {
    String msg = String(message);
    for (uint8_t i = 0; i < (dots % 4); i++) {
        msg += ".";
    }
    _tft.fillRect(0, 85, 128, 20, COLOR_BG_DARK);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(msg, 64, 95, 1);
}

void DisplayUI::drawHeader(bool wifiConnected) {
    _tft.fillRect(0, 0, 128, 16, COLOR_BG_DARK);

    // Logo Spotify izquierda
    _tft.drawBitmap(6, 2, epd_bitmap_spotify_logo, 12, 12, COLOR_SPOTIFY_GREEN);

    // Texto Spotify
    _tft.setTextColor(COLOR_TEXT_WHITE, COLOR_BG_DARK);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString("Spotify", 22, 4, 1);

    // Ícono Wi-Fi derecha
    uint16_t wifiColor = wifiConnected ? COLOR_SPOTIFY_GREEN : COLOR_TEXT_MUTED;
    _tft.drawBitmap(112, 4, epd_bitmap_wifi, 10, 8, wifiColor);
}

void DisplayUI::drawIdleScreen() {
    drawHeader(true);

    // Rectángulo central vacío para carátula
    _tft.drawRoundRect(32, 20, 64, 64, 4, COLOR_BAR_BG);
    _tft.fillRect(33, 21, 62, 62, COLOR_BG_DARK);

    _tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG_DARK);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString("Sin reproduccion", 64, 52, 1);

    _tft.drawString("Esperando pista...", 64, 95, 1);

    // Limpiar barra de progreso
    _progressSprite.fillSprite(COLOR_BG_DARK);
    _progressSprite.pushSprite(0, 103);

    // Controles en gris
    updatePlaybackControls(false);

    _lastRenderedTrackId = "";
}

void DisplayUI::formatTime(uint32_t ms, char *buffer, size_t bufSize) {
    uint32_t totalSec = ms / 1000;
    uint32_t minutes = totalSec / 60;
    uint32_t seconds = totalSec % 60;
    snprintf(buffer, bufSize, "%u:%02u", minutes, seconds);
}

void DisplayUI::updateProgress(uint32_t progressMs, uint32_t durationMs) {
    if (durationMs == 0) durationMs = 1;
    if (progressMs > durationMs) progressMs = durationMs;

    _progressSprite.fillSprite(COLOR_BG_DARK);

    // Formatear tiempos
    char curTime[10];
    char totTime[10];
    formatTime(progressMs, curTime, sizeof(curTime));
    formatTime(durationMs, totTime, sizeof(totTime));

    _progressSprite.setTextColor(COLOR_TEXT_MUTED, COLOR_BG_DARK);
    _progressSprite.setTextDatum(TL_DATUM);
    _progressSprite.drawString(curTime, 4, 3, 1);

    _progressSprite.setTextDatum(TR_DATUM);
    _progressSprite.drawString(totTime, 124, 3, 1);

    // Dimensiones de la barra de progreso
    const int barX = 36;
    const int barY = 6;
    const int barW = 56;
    const int barH = 2;

    _progressSprite.fillRect(barX, barY, barW, barH, COLOR_BAR_BG);

    int progressW = (int)((float)progressMs / (float)durationMs * barW);
    if (progressW > 0) {
        _progressSprite.fillRect(barX, barY, progressW, barH, COLOR_SPOTIFY_GREEN);
    }

    // Puntero circular
    int knobX = barX + progressW;
    _progressSprite.fillCircle(knobX, barY + 1, 2, COLOR_TEXT_WHITE);

    // Renderizar sprite en la pantalla de inmediato (sin parpadeo)
    _progressSprite.pushSprite(0, 103);
}

void DisplayUI::updatePlaybackControls(bool isPlaying) {
    // Fila inferior de controles (Y: 116 a 126)
    int y = 117;

    // Shuffle
    _tft.drawBitmap(8, y + 2, epd_bitmap_shuffle, 10, 8, COLOR_TEXT_MUTED);

    // Prev
    _tft.drawBitmap(28, y + 2, epd_bitmap_prev, 8, 8, COLOR_TEXT_WHITE);

    // Play/Pause central con botón circular verde
    _tft.fillCircle(64, y + 5, 8, COLOR_SPOTIFY_GREEN);
    if (isPlaying) {
        _tft.drawBitmap(61, y + 1, epd_bitmap_pause, 6, 8, COLOR_BG_DARK);
    } else {
        _tft.drawBitmap(62, y + 1, epd_bitmap_play, 6, 8, COLOR_BG_DARK);
    }

    // Next
    _tft.drawBitmap(92, y + 2, epd_bitmap_next, 8, 8, COLOR_TEXT_WHITE);

    // Heart / Like
    _tft.drawBitmap(110, y + 2, epd_bitmap_heart, 9, 8, COLOR_HEART_GREEN);
}

void DisplayUI::drawTrack(const SpotifyTrack &track, bool forceFullRedraw) {
    bool trackChanged = (_lastRenderedTrackId != track.id) || forceFullRedraw;

    if (trackChanged) {
        _lastRenderedTrackId = track.id;
        drawHeader(true);

        // Limpiar área de texto de canción y artista (Y: 85 a 102)
        _tft.fillRect(0, 85, 128, 18, COLOR_BG_DARK);

        // Título de canción (centrado, truncado a 16 caracteres para no desbordar)
        String displayTitle = track.title;
        if (displayTitle.length() > 16) {
            displayTitle = displayTitle.substring(0, 15) + ".";
        }
        _tft.setTextColor(COLOR_TEXT_WHITE, COLOR_BG_DARK);
        _tft.setTextDatum(TC_DATUM);
        _tft.drawString(displayTitle, 64, 85, 1);

        // Artista (centrado, gris tenue)
        String displayArtist = track.artist;
        if (displayArtist.length() > 20) {
            displayArtist = displayArtist.substring(0, 19) + ".";
        }
        _tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG_DARK);
        _tft.setTextDatum(TC_DATUM);
        _tft.drawString(displayArtist, 64, 94, 1);
    }

    // Actualizar controles si cambió el estado de reproducción
    if (_lastPlayingState != track.isPlaying || trackChanged) {
        _lastPlayingState = track.isPlaying;
        updatePlaybackControls(track.isPlaying);
    }

    // Actualizar barra de progreso
    updateProgress(track.progressMs, track.durationMs);
}
