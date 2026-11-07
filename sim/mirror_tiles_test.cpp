// Host regression: the 16x4 RGB565 BLE protocol must reproduce exact pixels
// for every tile, including the three-pixel-high last row.
#include "../core/mirror_tiles.h"
#include <cstdio>
#include <cstring>
#include <vector>

static int digit(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

int main() {
  std::vector<gfx::Color> origin(gfx::W * gfx::H), decoded(gfx::W * gfx::H, 0);
  for (int y = 0; y < gfx::H; ++y)
    for (int x = 0; x < gfx::W; ++x)
      origin[y * gfx::W + x] = (gfx::Color)((x * 193 + y * 97 + (x ^ y) * 51) & 0xffff);

  for (int id = 0; id < mirror_tiles::COUNT; ++id) {
    char line[mirror_tiles::MAX_LINE];
    int len = mirror_tiles::encode(origin.data(), id, line, sizeof line);
    if (len < 0 || len != (int)strlen(line) || len > 192) return 1;
    int tile = -1, consumed = 0;
    if (sscanf(line, "F|%d|%n", &tile, &consumed) != 1 || tile != id) return 2;
    const char* base64 = line + consumed;
    std::vector<uint8_t> bytes;
    unsigned acc = 0; int bits = 0;
    for (int i = 0; base64[i]; ++i) {
      if (base64[i] == '=') break;
      int v = digit(base64[i]);
      if (v < 0) return 3;
      acc = (acc << 6) | (unsigned)v;
      bits += 6;
      if (bits >= 8) {
        bits -= 8;
        bytes.push_back((uint8_t)(acc >> bits));
      }
    }
    int x0 = mirror_tiles::tileX(id), y0 = mirror_tiles::tileY(id);
    int w = mirror_tiles::tileWidth(id), h = mirror_tiles::tileHeight(id);
    if ((int)bytes.size() != w * h * 2) return 4;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        int i = (y * w + x) * 2;
        decoded[(y0 + y) * gfx::W + x0 + x] =
            (gfx::Color)(bytes[i] | (bytes[i + 1] << 8));
      }
    if (!mirror_tiles::fingerprint(origin.data(), id)) return 5;
  }
  if (origin != decoded) return 6;
  char shortBuf[32];
  if (mirror_tiles::encode(origin.data(), 0, shortBuf, sizeof shortBuf) != -1) return 7;
  if (mirror_tiles::encode(origin.data(), -1, shortBuf, sizeof shortBuf) != -1) return 8;
  if (mirror_tiles::COUNT != 510 || mirror_tiles::tileHeight(509) != 3) return 9;

  origin[0] ^= 1;
  if (mirror_tiles::fingerprint(origin.data(), 0) == mirror_tiles::fingerprint(decoded.data(), 0)) return 10;
  if (mirror_tiles::fingerprint(origin.data(), 1) != mirror_tiles::fingerprint(decoded.data(), 1)) return 11;
  // V2 wire format: every tile belongs to an explicit immutable snapshot.
  char v2[mirror_tiles::MAX_LINE];
  if (mirror_tiles::encodeFrame(decoded.data(), 65535, 509, v2, sizeof v2) < 0) return 12;
  if (strncmp(v2, "T|65535|509|", 12) || strlen(v2) > 195) return 13;
  auto snapshot = decoded;
  const uint32_t before = mirror_tiles::checksum(snapshot.data());
  decoded[0] ^= 0x5a3c;
  if (mirror_tiles::checksum(snapshot.data()) != before) return 14;
  if (mirror_tiles::checksum(decoded.data()) == before) return 15;
  // A moved pet's new framebuffer may change during BLE transmission,
  // but the frozen snapshot and its checksum MUST NOT change.
  if (mirror_tiles::fingerprint(snapshot.data(), 0) ==
      mirror_tiles::fingerprint(decoded.data(), 0)) return 16;
  std::puts("OK: v2 tagged tiles, immutable snapshots, whole-frame checksum.");
  std::puts("OK: exact 240x135 RGB565; 510 independently updated tiles; <193 bytes per tile.");
  return 0;
}
