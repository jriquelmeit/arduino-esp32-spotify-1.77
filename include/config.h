#pragma once

#include <Arduino.h>

// Si existe secrets.h se incluye, sino se usan los valores por defecto/ejemplo
#if __has_include("secrets.h")
    #include "secrets.h"
#else
    #warning "No se encontro secrets.h. Usando secrets.h.example. Copia secrets.h.example a secrets.h con tus credenciales."
    #include "secrets.h.example"
#endif

// ==========================================
// CONFIGURACIÓN DE PANTALLA Y RENDERIZADO
// ==========================================
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       160
#define TFT_ROTATION        0

// Paleta de colores estilo Spotify (RGB565)
#define COLOR_SPOTIFY_GREEN 0x1DB9 // #1DB954 aprox en RGB565 -> 0x1DB9 o 0x1DCB
#define COLOR_BG_DARK       0x10A2 // Fondo gris muy oscuro #121212
#define COLOR_TEXT_WHITE    0xFFFF // Blanco
#define COLOR_TEXT_MUTED    0xAD55 // Gris suave #A7A7A7
#define COLOR_BAR_BG        0x39E7 // Gris oscuro para barra de fondo
#define COLOR_HEART_GREEN   0x1DB9

// ==========================================
// INTERVALOS DE CONSULTA Y TIEMPO
// ==========================================
#define SPOTIFY_POLL_INTERVAL_PLAYING_MS    2500UL // Cada 2.5 seg al reproducir
#define SPOTIFY_POLL_INTERVAL_IDLE_MS       5000UL // Cada 5 seg cuando está pausado
#define LOCAL_TICK_INTERVAL_MS              1000UL // Cada 1 seg avanza la barra localmente
#define WIFI_RECONNECT_INTERVAL_MS          10000UL
