#include "display_ui.h"
#include "config.h"
#include "spotify_icons.h"
#include <TJpg_Decoder.h>

struct JpgRenderConfig {
    int16_t cropOffsetX = 0;
    int16_t cropOffsetY = 0;
    bool upscale2x = false;
};
static JpgRenderConfig g_renderConfig;

// Callback global para que TJpg_Decoder envíe bloques a TFT_eSPI
static TFT_eSPI *pTft = nullptr;

void DisplayUI::setupJpgRender(uint16_t origW, uint16_t origH) {
    if (origW >= 500) {
        TJpgDec.setJpgScale(4);
        g_renderConfig.cropOffsetX = ((int16_t)(origW / 4) - 128) / 2;
        g_renderConfig.cropOffsetY = ((int16_t)(origH / 4) - 128) / 2;
        g_renderConfig.upscale2x = false;
    } else if (origW >= 200) {
        // Imagen estándar de 300x300 decodificada a escala 1/2 -> 150x150 píxeles nítidos
        TJpgDec.setJpgScale(2);
        g_renderConfig.cropOffsetX = ((int16_t)(origW / 2) - 128) / 2; // (150 - 128) / 2 = 11 px
        g_renderConfig.cropOffsetY = ((int16_t)(origH / 2) - 128) / 2;
        g_renderConfig.upscale2x = false;
    } else {
        // Fallback para imágenes pequeñas (ej. 64x64)
        TJpgDec.setJpgScale(1);
        g_renderConfig.cropOffsetX = 0;
        g_renderConfig.cropOffsetY = 0;
        g_renderConfig.upscale2x = true;
    }
}

bool tftOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!pTft) return 0;

    if (g_renderConfig.upscale2x) {
        // Escalar 2x duplicando píxeles solo para portadas de baja resolución (64x64)
        int16_t destX = x * 2;
        int16_t destY = y * 2;
        uint16_t destW = w * 2;

        uint16_t scaledLine[32];
        if (destW > 32) destW = 32;

        for (uint16_t row = 0; row < h; row++) {
            for (uint16_t col = 0; col < w && (col * 2 + 1) < 32; col++) {
                uint16_t pixel = bitmap[row * w + col];
                scaledLine[col * 2]     = pixel;
                scaledLine[col * 2 + 1] = pixel;
            }

            int16_t screenY1 = destY + (row * 2);
            int16_t screenY2 = screenY1 + 1;

            if (screenY1 < 128) pTft->pushImage(destX, screenY1, destW, 1, scaledLine);
            if (screenY2 < 128) pTft->pushImage(destX, screenY2, destW, 1, scaledLine);
        }
    } else {
        // Renderizado nativo de alta resolución con recorte centrado (150x150 -> 128x128)
        int16_t cropX = g_renderConfig.cropOffsetX;
        int16_t cropY = g_renderConfig.cropOffsetY;

        for (uint16_t row = 0; row < h; row++) {
            int16_t imgY = y + row;
            if (imgY < cropY || imgY >= (cropY + 128)) {
                continue; // Fuera de los 128 px verticales
            }
            int16_t screenY = imgY - cropY;

            int16_t visibleStart = (x > cropX) ? x : cropX;
            int16_t visibleEnd   = ((x + w - 1) < (cropX + 127)) ? (x + w - 1) : (cropX + 127);

            if (visibleStart <= visibleEnd) {
                uint16_t count = visibleEnd - visibleStart + 1;
                uint16_t offsetInBlock = visibleStart - x;
                int16_t screenX = visibleStart - cropX;

                pTft->pushImage(screenX, screenY, count, 1, &bitmap[row * w + offsetInBlock]);
            }
        }
    }
    return 1;
}

DisplayUI::DisplayUI() : _tft(), _lastRenderedTrackId(""), _lastProgressW(-1) {}

void DisplayUI::begin() {
    _tft.init();
    _tft.setRotation(TFT_ROTATION);
    _tft.fillScreen(COLOR_BG_DARK);
    pTft = &_tft;

    // Configurar decodificador JPEG con swap de bytes para TFT SPI
    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback(tftOutputCallback);
    TJpgDec.setSwapBytes(true);
}

void DisplayUI::showBootScreen(const char* status) {
    _tft.fillScreen(COLOR_BG_DARK);
    
    // Logo Spotify central
    _tft.drawBitmap(58, 50, epd_bitmap_spotify_logo, 12, 12, COLOR_SPOTIFY_GREEN);
    
    _tft.setTextColor(COLOR_TEXT_WHITE, COLOR_BG_DARK);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString("Spotify Mini", 64, 75, 2);

    _tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG_DARK);
    _tft.drawString(status, 64, 105, 1);
}

void DisplayUI::updateBootProgress(const char* message, uint8_t dots) {
    String msg = String(message);
    for (uint8_t i = 0; i < (dots % 4); i++) {
        msg += ".";
    }
    _tft.fillRect(0, 100, 128, 20, COLOR_BG_DARK);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(msg, 64, 110, 1);
}

void DisplayUI::drawIdleScreen() {
    _tft.fillScreen(COLOR_BG_DARK);

    // Cabecera sutil en idle (Y: 10)
    _tft.drawBitmap(8, 8, epd_bitmap_spotify_logo, 12, 12, COLOR_SPOTIFY_GREEN);
    _tft.setTextColor(COLOR_TEXT_WHITE, COLOR_BG_DARK);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString("Spotify", 24, 10, 1);
    _tft.drawBitmap(110, 10, epd_bitmap_wifi, 10, 8, COLOR_SPOTIFY_GREEN);

    // Marco central en el área de la portada (Y: 28..118)
    _tft.drawRoundRect(16, 28, 96, 90, 4, COLOR_BAR_BG);
    _tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG_DARK);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString("Sin reproduccion", 64, 73, 1);

    // Barra de progreso vacía en Y: 128..129
    _tft.fillRect(0, 128, SCREEN_WIDTH, 2, COLOR_BAR_BG);
    _lastProgressW = -1;

    // Footer inferior (Y: 130..159)
    _tft.fillRect(0, 130, SCREEN_WIDTH, 30, COLOR_BG_DARK);
    _tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG_DARK);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString("Esperando musica...", 64, 145, 1);

    _lastRenderedTrackId = "";
}

void DisplayUI::updateProgress(uint32_t progressMs, uint32_t durationMs) {
    if (durationMs == 0) durationMs = 1;
    if (progressMs > durationMs) progressMs = durationMs;

    int progressW = (int)(((uint64_t)progressMs * SCREEN_WIDTH) / durationMs);
    if (progressW > SCREEN_WIDTH) progressW = SCREEN_WIDTH;

    if (progressW != _lastProgressW) {
        if (progressW > 0) {
            _tft.fillRect(0, 128, progressW, 2, COLOR_SPOTIFY_GREEN);
        }
        if (progressW < SCREEN_WIDTH) {
            _tft.fillRect(progressW, 128, SCREEN_WIDTH - progressW, 2, COLOR_BAR_BG);
        }
        _lastProgressW = progressW;
    }
}

void DisplayUI::drawTrack(const SpotifyTrack &track, bool forceFullRedraw) {
    bool trackChanged = (_lastRenderedTrackId != track.id) || forceFullRedraw;

    if (trackChanged) {
        _lastRenderedTrackId = track.id;

        // Limpiar área de carátula (Y: 0 a 127) mientras descarga la nueva imagen
        _tft.fillRect(0, 0, SCREEN_WIDTH, 128, COLOR_BG_DARK);

        // Limpiar área de footer (Y: 130 a 160)
        _tft.fillRect(0, 130, SCREEN_WIDTH, 30, COLOR_BG_DARK);

        // Título de canción centrado en footer (Y: 134, truncado a 20 caracteres)
        String displayTitle = track.title;
        if (displayTitle.length() > 20) {
            displayTitle = displayTitle.substring(0, 18) + "..";
        }
        _tft.setTextColor(COLOR_TEXT_WHITE, COLOR_BG_DARK);
        _tft.setTextDatum(TC_DATUM);
        _tft.drawString(displayTitle, 64, 134, 1);

        // Artista centrado en footer (Y: 147, truncado a 20 caracteres)
        String displayArtist = track.artist;
        if (displayArtist.length() > 20) {
            displayArtist = displayArtist.substring(0, 18) + "..";
        }
        _tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG_DARK);
        _tft.setTextDatum(TC_DATUM);
        _tft.drawString(displayArtist, 64, 147, 1);

        // Forzar redibujo de la barra de progreso
        _lastProgressW = -1;
    }

    // Actualizar barra de progreso delgada (Y: 128..129)
    updateProgress(track.progressMs, track.durationMs);
}
