// Lossless RGB565 tiles transmitted over Nordic UART. This wire format stays
// independent of firmware sprite/layout revisions, so Android needs no new APK.
// Protocol: F|<tile index>|<Base64 RGB565 LE>, 16x4 pixel tiles.
#pragma once
#include "gfx.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

namespace mirror_tiles {
static constexpr int TILE_W = 16;
static constexpr int TILE_H = 4;
static constexpr int COLS = (gfx::W + TILE_W - 1) / TILE_W;
static constexpr int ROWS = (gfx::H + TILE_H - 1) / TILE_H;
static constexpr int COUNT = COLS * ROWS;
static constexpr int MAX_BYTES = TILE_W * TILE_H * 2;
static constexpr int MAX_LINE = 200;

inline int tileX(int id) { return (id % COLS) * TILE_W; }
inline int tileY(int id) { return (id / COLS) * TILE_H; }
inline int tileWidth(int id) {
  int rest = gfx::W - tileX(id);
  return rest < TILE_W ? rest : TILE_W;
}
inline int tileHeight(int id) {
  int rest = gfx::H - tileY(id);
  return rest < TILE_H ? rest : TILE_H;
}
inline uint32_t fingerprint(const gfx::Color* pixels, int id) {
  uint32_t hash = 2166136261u;
  const int x0 = tileX(id), y0 = tileY(id), w = tileWidth(id), h = tileHeight(id);
  for (int y = 0; y < h; ++y) {
    const gfx::Color* row = pixels + (y0 + y) * gfx::W + x0;
    for (int x = 0; x < w; ++x) {
      hash = (hash ^ (uint8_t)row[x]) * 16777619u;
      hash = (hash ^ (uint8_t)(row[x] >> 8)) * 16777619u;
    }
  }
  return hash;
}
inline int encode(const gfx::Color* pixels, int id, char* out, size_t cap) {
  if (!pixels || !out || id < 0 || id >= COUNT || cap < MAX_LINE) return -1;
  const int x0 = tileX(id), y0 = tileY(id), w = tileWidth(id), h = tileHeight(id);
  const int prefix = snprintf(out, cap, "F|%d|", id);
  if (prefix < 0 || prefix >= (int)cap) return -1;
  static constexpr char TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  uint8_t raw[MAX_BYTES];
  int count = 0;
  for (int y = 0; y < h; ++y) {
    const gfx::Color* row = pixels + (y0 + y) * gfx::W + x0;
    for (int x = 0; x < w; ++x) {
      const uint16_t c = row[x];
      raw[count++] = (uint8_t)c;
      raw[count++] = (uint8_t)(c >> 8);
    }
  }
  int k = prefix;
  for (int i = 0; i < count; i += 3) {
    const int a = raw[i], b = i + 1 < count ? raw[i + 1] : 0, c = i + 2 < count ? raw[i + 2] : 0;
    if (k + 5 > (int)cap) return -1;
    out[k++] = TABLE[a >> 2];
    out[k++] = TABLE[((a & 3) << 4) | (b >> 4)];
    out[k++] = i + 1 < count ? TABLE[((b & 15) << 2) | (c >> 6)] : '=';
    out[k++] = i + 2 < count ? TABLE[c & 63] : '=';
  }
  out[k] = 0;
  return k;
}
}
