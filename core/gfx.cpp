#include "gfx.h"

namespace gfx {

Color blend(Color d, Color s, int a) {
  if (a >= 255) return s;
  if (a <= 0) return d;
  // 565 -> раздвинутые каналы, одно умножение на всё
  uint32_t a5 = (uint32_t)(a + 4) >> 3;  // 0..32
  uint32_t dd = (d | ((uint32_t)d << 16)) & 0x07E0F81F;
  uint32_t ss = (s | ((uint32_t)s << 16)) & 0x07E0F81F;
  uint32_t r = (dd + (((ss - dd) * a5) >> 5)) & 0x07E0F81F;
  return (Color)(r | (r >> 16));
}

Color mix(Color a, Color b, int t) { return blend(a, b, t); }

Color shade(Color c, int k) {
  int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  if (k <= 256) {
    r = r * k >> 8; g = g * k >> 8; b = b * k >> 8;
  } else {
    int t = k - 256;  // к белому
    if (t > 256) t = 256;
    r += (31 - r) * t >> 8; g += (63 - g) * t >> 8; b += (31 - b) * t >> 8;
  }
  return (Color)((r << 11) | (g << 5) | b);
}

void Canvas::clip(int x0, int y0, int x1, int y1) {
  cx0 = clampi(x0, 0, W); cy0 = clampi(y0, 0, H);
  cx1 = clampi(x1, 0, W); cy1 = clampi(y1, 0, H);
}

void Canvas::clear(Color c) {
  uint32_t v = c | ((uint32_t)c << 16);
  uint32_t* p = (uint32_t*)px;
  for (int i = 0; i < W * H / 2; i++) p[i] = v;
}

void Canvas::pixel(int x, int y, Color c, int a) { put(x, y, c, a); }

void Canvas::fillRect(int x, int y, int w, int h, Color c, int a) {
  int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
  int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
  if (a <= 0) return;
  for (int yy = y0; yy < y1; yy++) {
    Color* row = px + yy * W;
    if (a >= 255)
      for (int xx = x0; xx < x1; xx++) row[xx] = c;
    else
      for (int xx = x0; xx < x1; xx++) row[xx] = blend(row[xx], c, a);
  }
}

void Canvas::gradientV(int x, int y, int w, int h, Color top, Color bottom) {
  for (int i = 0; i < h; i++) fillRect(x, y + i, w, 1, mix(top, bottom, h > 1 ? i * 255 / (h - 1) : 0));
}

void Canvas::rect(int x, int y, int w, int h, Color c, int a) {
  hline(x, y, w, c, a); hline(x, y + h - 1, w, c, a);
  vline(x, y + 1, h - 2, c, a); vline(x + w - 1, y + 1, h - 2, c, a);
}

// --- сглаженные фигуры через функции расстояния ---

static inline int cover(float d) {  // d: расстояние до края (внутри < 0)
  float c = 0.5f - d;
  if (c <= 0) return 0;
  if (c >= 1) return 256;
  return (int)(c * 256);
}

static inline float ellipseDist(float px, float py, float rx, float ry) {
  if (rx < 0.5f) rx = 0.5f;
  if (ry < 0.5f) ry = 0.5f;
  float k0x = px / rx, k0y = py / ry;
  float k1x = px / (rx * rx), k1y = py / (ry * ry);
  float k0 = fsqrt(k0x * k0x + k0y * k0y);
  float k1 = fsqrt(k1x * k1x + k1y * k1y);
  if (k1 < 1e-5f) return -(rx < ry ? rx : ry);
  return k0 * (k0 - 1.0f) / k1;
}

void Canvas::ellipseGrad(float cx, float cy, float rx, float ry, Color top, Color bottom, int a) {
  int x0 = (int)(cx - rx - 1), x1 = (int)(cx + rx + 2);
  int y0 = (int)(cy - ry - 1), y1 = (int)(cy + ry + 2);
  if (x0 < cx0) x0 = cx0;
  if (y0 < cy0) y0 = cy0;
  if (x1 > cx1) x1 = cx1;
  if (y1 > cy1) y1 = cy1;
  bool grad = top != bottom;
  for (int y = y0; y < y1; y++) {
    float py = y + 0.5f - cy;
    Color c = grad ? mix(top, bottom, clampi((int)((py + ry) * 255 / (2 * ry)), 0, 255)) : top;
    for (int x = x0; x < x1; x++) {
      float d = ellipseDist(x + 0.5f - cx, py, rx, ry);
      int cv = cover(d);
      if (cv) put(x, y, c, a * cv >> 8);
    }
  }
}

void Canvas::ellipse(float cx, float cy, float rx, float ry, Color c, int a) { ellipseGrad(cx, cy, rx, ry, c, c, a); }

void Canvas::ring(float cx, float cy, float r, float t, Color c, int a) {
  int x0 = (int)(cx - r - t - 1), x1 = (int)(cx + r + t + 2);
  int y0 = (int)(cy - r - t - 1), y1 = (int)(cy + r + t + 2);
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      float d = fabs_(fsqrt(dx * dx + dy * dy) - r) - t * 0.5f;
      int cv = cover(d);
      if (cv) put(x, y, c, a * cv >> 8);
    }
}

static float atan2a(float y, float x) {  // приближённый atan2 в радианах
  float ax = fabs_(x), ay = fabs_(y);
  float mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax;
  if (mx < 1e-6f) return 0;
  float t = mn / mx, s = t * t;
  float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * t + t;
  if (ay > ax) r = 1.57079637f - r;
  if (x < 0) r = 3.14159274f - r;
  if (y < 0) r = -r;
  return r;
}

void Canvas::arc(float cx, float cy, float r, float t, float a0, float a1, Color c, int a) {
  // углы в радианах, 0 = вправо, по часовой (ось y вниз)
  int x0 = (int)(cx - r - t - 1), x1 = (int)(cx + r + t + 2);
  int y0 = (int)(cy - r - t - 1), y1 = (int)(cy + r + t + 2);
  const float PI2 = 6.2831853f;
  while (a0 < 0) { a0 += PI2; a1 += PI2; }
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      float d = fabs_(fsqrt(dx * dx + dy * dy) - r) - t * 0.5f;
      int cv = cover(d);
      if (!cv) continue;
      float an = atan2a(dy, dx);
      while (an < a0) an += PI2;
      if (an > a1) continue;
      put(x, y, c, a * cv >> 8);
    }
}

static inline float rrDist(float px, float py, float hw, float hh, float r) {
  float qx = fabs_(px) - hw + r, qy = fabs_(py) - hh + r;
  float mx = qx > 0 ? qx : 0, my = qy > 0 ? qy : 0;
  float out = fsqrt(mx * mx + my * my);
  float in = qx > qy ? qx : qy;
  if (in > 0) in = 0;
  return out + in - r;
}

void Canvas::roundRectGrad(float x, float y, float w, float h, float r, Color top, Color bottom, int a) {
  float hw = w / 2, hh = h / 2, cx = x + hw, cy = y + hh;
  if (r > hw) r = hw;
  if (r > hh) r = hh;
  int x0 = (int)x - 1, x1 = (int)(x + w) + 2, y0 = (int)y - 1, y1 = (int)(y + h) + 2;
  if (x0 < cx0) x0 = cx0;
  if (y0 < cy0) y0 = cy0;
  if (x1 > cx1) x1 = cx1;
  if (y1 > cy1) y1 = cy1;
  for (int yy = y0; yy < y1; yy++) {
    float py = yy + 0.5f - cy;
    Color c = top == bottom ? top : mix(top, bottom, clampi((int)((py + hh) * 255 / h), 0, 255));
    // середина строки без углов — быстро
    bool mid = fabs_(py) < hh - r - 1;
    for (int xx = x0; xx < x1; xx++) {
      float px_ = xx + 0.5f - cx;
      int cv;
      if (mid && fabs_(px_) < hw - 1) cv = 256;
      else cv = cover(rrDist(px_, py, hw, hh, r));
      if (cv) put(xx, yy, c, a * cv >> 8);
    }
  }
}

void Canvas::roundRect(float x, float y, float w, float h, float r, Color c, int a) { roundRectGrad(x, y, w, h, r, c, c, a); }

void Canvas::line(float x0, float y0, float x1, float y1, float t, Color c, int a) {
  float minx = (x0 < x1 ? x0 : x1) - t, maxx = (x0 > x1 ? x0 : x1) + t;
  float miny = (y0 < y1 ? y0 : y1) - t, maxy = (y0 > y1 ? y0 : y1) + t;
  float dx = x1 - x0, dy = y1 - y0, ll = dx * dx + dy * dy;
  for (int y = (int)miny; y <= (int)maxy + 1; y++)
    for (int x = (int)minx; x <= (int)maxx + 1; x++) {
      float px_ = x + 0.5f - x0, py = y + 0.5f - y0;
      float h = ll > 0 ? clampf((px_ * dx + py * dy) / ll, 0, 1) : 0;
      float ex = px_ - dx * h, ey = py - dy * h;
      float d = fsqrt(ex * ex + ey * ey) - t * 0.5f;
      int cv = cover(d);
      if (cv) put(x, y, c, a * cv >> 8);
    }
}

void Canvas::triangle(float x0, float y0, float x1, float y1, float x2, float y2, Color c, int a) {
  float minx = x0, maxx = x0, miny = y0, maxy = y0;
  float xs[3] = {x0, x1, x2}, ys[3] = {y0, y1, y2};
  for (int i = 1; i < 3; i++) {
    if (xs[i] < minx) minx = xs[i];
    if (xs[i] > maxx) maxx = xs[i];
    if (ys[i] < miny) miny = ys[i];
    if (ys[i] > maxy) maxy = ys[i];
  }
  float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
  float sgn = area < 0 ? -1.f : 1.f;
  for (int y = (int)miny - 1; y <= (int)maxy + 1; y++)
    for (int x = (int)minx - 1; x <= (int)maxx + 1; x++) {
      float px_ = x + 0.5f, py = y + 0.5f, d = -1e9f;
      for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        float ex = xs[j] - xs[i], ey = ys[j] - ys[i];
        float len = fsqrt(ex * ex + ey * ey);
        if (len < 1e-4f) continue;
        float e = -sgn * ((px_ - xs[i]) * ey - (py - ys[i]) * ex) / len;
        e = -e;
        if (e > d) d = e;
      }
      int cv = cover(d);
      if (cv) put(x, y, c, a * cv >> 8);
    }
}

// --- текст ---

static uint32_t nextCp(const char*& s) {
  uint8_t c = (uint8_t)*s++;
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0) { uint32_t r = (c & 0x1F) << 6; r |= (*s++ & 0x3F); return r; }
  if ((c & 0xF0) == 0xE0) {
    uint32_t r = (c & 0x0F) << 12; r |= (*s++ & 0x3F) << 6; r |= (*s++ & 0x3F); return r;
  }
  s += 3;
  return '?';
}

static const Glyph* find(const Font& f, uint32_t cp) {
  int lo = 0, hi = f.count - 1;
  while (lo <= hi) {
    int m = (lo + hi) / 2;
    if (f.glyphs[m].cp == cp) return &f.glyphs[m];
    if (f.glyphs[m].cp < cp) lo = m + 1; else hi = m - 1;
  }
  return cp == '?' ? nullptr : find(f, '?');
}

int Canvas::width(const Font& f, const char* s) {
  int w = 0;
  while (*s) {
    const Glyph* g = find(f, nextCp(s));
    if (g) w += g->adv;
  }
  return w;
}

void (*textOverflow)(const char* s, int x0, int x1) = nullptr;

int Canvas::text(const Font& f, int x, int y, const char* s, Color c, int a, Align al) {
  if (al == CENTER) x -= width(f, s) / 2;
  else if (al == RIGHT) x -= width(f, s);
  if (textOverflow && a > 0 && (x < 0 || x + width(f, s) > W)) textOverflow(s, x, x + width(f, s));
  int start = x;
  while (*s) {
    const Glyph* g = find(f, nextCp(s));
    if (!g) continue;
    int stride = (g->w + 1) / 2;
    const uint8_t* b = f.bits + g->off;
    for (int yy = 0; yy < g->h; yy++)
      for (int xx = 0; xx < g->w; xx++) {
        uint8_t v = b[yy * stride + xx / 2];
        int al4 = (xx & 1) ? (v & 15) : (v >> 4);
        if (al4) put(x + g->x + xx, y + g->y + yy, c, a * al4 * 17 >> 8);
      }
    x += g->adv;
  }
  return x - start;
}

int Canvas::textFit(int x, int y, const char* s, Color c, int maxW, Align al) {
  if (width(font_big, s) <= maxW) return text(font_big, x, y, s, c, 255, al);
  if (width(font_ui, s) <= maxW) return text(font_ui, x, y + 3, s, c, 255, al);
  return text(font_small, x, y + 5, s, c, 255, al);
}

int Canvas::textShadow(const Font& f, int x, int y, const char* s, Color c, Color sh, Align al) {
  text(f, x + 1, y + 1, s, sh, 160, al);
  return text(f, x, y, s, c, 255, al);
}

// --- эффекты ---

void Canvas::dim(int k) {
  for (int i = 0; i < W * H; i++) px[i] = shade(px[i], k);
}

void Canvas::tint(Color c, int a) {
  for (int i = 0; i < W * H; i++) px[i] = blend(px[i], c, a);
}

void Canvas::shiftRow(int y, int dx) {
  if (y < 0 || y >= H || dx == 0) return;
  Color* r = px + y * W;
  if (dx > 0) {
    for (int x = W - 1; x >= dx; x--) r[x] = r[x - dx];
  } else {
    for (int x = 0; x < W + dx; x++) r[x] = r[x - dx];
  }
}

void Canvas::vignette(int strength) {
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      int dx = (x - W / 2) * 256 / (W / 2), dy = (y - H / 2) * 256 / (H / 2);
      int d = (dx * dx + dy * dy) >> 8;  // 0..512
      int k = 256 - (d * strength >> 9);
      if (k < 256) px[y * W + x] = shade(px[y * W + x], k < 0 ? 0 : k);
    }
}

void Canvas::scanlines(int a) {
  for (int y = 0; y < H; y += 2) fillRect(0, y, W, 1, 0, a);
}

// --- математика ---

float fatan2(float y, float x) { return atan2a(y, x); }

float fsin(float x) {
  const float PI = 3.14159265f, TP = 6.2831853f;
  x = x - TP * (float)(int)(x / TP);
  if (x < -PI) x += TP;
  if (x > PI) x -= TP;
  // парабола + уточнение, ошибка < 0.001
  float y = 1.27323954f * x - 0.405284735f * x * fabs_(x);
  return 0.225f * (y * fabs_(y) - y) + y;
}

int isin(int deg) { return (int)(fsin(deg * 0.01745329f) * 256); }

float ease(float t) {
  t = clampf(t, 0, 1);
  return t * t * (3 - 2 * t);
}

float bounce(float t) {
  t = clampf(t, 0, 1);
  if (t < 1 / 2.75f) return 7.5625f * t * t;
  if (t < 2 / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
  if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
  t -= 2.625f / 2.75f;
  return 7.5625f * t * t + 0.984375f;
}

float elastic(float t) {
  t = clampf(t, 0, 1);
  if (t == 0 || t == 1) return t;
  // 2^(-10t) через повторное умножение
  float p = 1;
  float e = 10 * t;
  int n = (int)e;
  for (int i = 0; i < n; i++) p *= 0.5f;
  p *= 1 - (e - n) * 0.29289f;
  return p * fsin((t * 10 - 0.75f) * 2.0943951f) + 1;
}

}  // namespace gfx

#ifdef __wasm__
extern "C" {
void* memset(void* d, int c, size_t n) {
  uint8_t* p = (uint8_t*)d;
  while (n--) *p++ = (uint8_t)c;
  return d;
}
void* memcpy(void* d, const void* s, size_t n) {
  uint8_t* p = (uint8_t*)d;
  const uint8_t* q = (const uint8_t*)s;
  while (n--) *p++ = *q++;
  return d;
}
}
#endif
