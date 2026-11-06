// Графика: кадр 240x135 RGB565 в памяти, сглаженные фигуры, текст, эффекты.
// Без стандартной библиотеки — собирается и под nRF52, и в WebAssembly.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace gfx {

constexpr int W = 240;
constexpr int H = 135;

typedef uint16_t Color;

constexpr Color rgb(int r, int g, int b) {
  return (Color)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr Color hex(uint32_t v) { return rgb((v >> 16) & 255, (v >> 8) & 255, v & 255); }

Color blend(Color dst, Color src, int a);  // a: 0..255
Color mix(Color a, Color b, int t);         // t: 0..255 (0 = a)
Color shade(Color c, int k);                // k: 256 = как есть, <256 темнее, >256 светлее

struct Glyph {
  uint16_t cp;
  uint8_t w, h;
  int8_t x, y;
  uint8_t adv;
  uint32_t off;
};
struct Font {
  const Glyph* glyphs;
  const uint8_t* bits;
  uint16_t count;
  uint8_t height, ascent;
};
extern const Font font_small, font_ui, font_big, font_mono;

enum Align { LEFT, CENTER, RIGHT };

// проверка вёрстки на ПК: вызывается, если текст вылез за край экрана
extern void (*textOverflow)(const char* s, int x0, int x1);

class Canvas {
 public:
  Color* px;
  explicit Canvas(Color* buf) : px(buf) { noClip(); }

  void clip(int x0, int y0, int x1, int y1);
  void noClip() { cx0 = 0; cy0 = 0; cx1 = W; cy1 = H; }

  void clear(Color c);
  void pixel(int x, int y, Color c, int a = 255);
  void fillRect(int x, int y, int w, int h, Color c, int a = 255);
  void gradientV(int x, int y, int w, int h, Color top, Color bottom);
  void hline(int x, int y, int w, Color c, int a = 255) { fillRect(x, y, w, 1, c, a); }
  void vline(int x, int y, int h, Color c, int a = 255) { fillRect(x, y, 1, h, c, a); }
  void rect(int x, int y, int w, int h, Color c, int a = 255);

  // сглаженные фигуры; координаты дробные
  void ellipse(float cx, float cy, float rx, float ry, Color c, int a = 255);
  void ellipseGrad(float cx, float cy, float rx, float ry, Color top, Color bottom, int a = 255);
  void circle(float cx, float cy, float r, Color c, int a = 255) { ellipse(cx, cy, r, r, c, a); }
  void ring(float cx, float cy, float r, float t, Color c, int a = 255);
  void arc(float cx, float cy, float r, float t, float a0, float a1, Color c, int a = 255);
  void roundRect(float x, float y, float w, float h, float r, Color c, int a = 255);
  void roundRectGrad(float x, float y, float w, float h, float r, Color top, Color bottom, int a = 255);
  void line(float x0, float y0, float x1, float y1, float t, Color c, int a = 255);
  void triangle(float x0, float y0, float x1, float y1, float x2, float y2, Color c, int a = 255);

  // текст UTF-8
  int text(const Font& f, int x, int y, const char* s, Color c, int a = 255, Align al = LEFT);
  int textShadow(const Font& f, int x, int y, const char* s, Color c, Color sh, Align al = LEFT);
  // крупно, а если не влезает в maxW — шрифтом поменьше (с поправкой по высоте)
  int textFit(int x, int y, const char* s, Color c, int maxW, Align al = CENTER);
  static int width(const Font& f, const char* s);

  // эффекты
  void dim(int k);                                // умножить яркость кадра на k/256
  void tint(Color c, int a);                      // залить всё цветом с прозрачностью
  void shiftRow(int y, int dx);                   // сдвиг строки (глитч)
  void vignette(int strength);
  void scanlines(int a);

 private:
  int cx0, cy0, cx1, cy1;
  inline void put(int x, int y, Color c, int a) {
    if (x < cx0 || y < cy0 || x >= cx1 || y >= cy1 || a <= 0) return;
    Color& d = px[y * W + x];
    d = a >= 255 ? c : blend(d, c, a);
  }
  friend struct Sdf;
};

// утилиты
int isin(int deg);  // sin * 256, угол в градусах
inline int icos(int deg) { return isin(deg + 90); }
float fsin(float rad);
float fatan2(float y, float x);  // радианы
inline float fcos(float rad) { return fsin(rad + 1.5707963f); }
inline float fabs_(float v) { return v < 0 ? -v : v; }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
inline float fsqrt(float v) { return __builtin_sqrtf(v); }
float ease(float t);         // плавный вход-выход 0..1
float bounce(float t);       // отскок 0..1
float elastic(float t);      // пружина 0..1

}  // namespace gfx

#ifdef __wasm__
extern "C" {
void* memset(void* d, int c, size_t n);
void* memcpy(void* d, const void* s, size_t n);
}
#else
#include <string.h>
#endif
