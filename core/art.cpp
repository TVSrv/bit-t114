#include "art.h"
#include "artdata.h"

namespace pet {
using namespace gfx;

const char* formName(Form f) {
  static const char* n[] = {"Компиляция", "Hello World", "Джун", "Мидл", "Фрилансер", "Сеньор",
                            "Архитектор", "Хакер", "Легаси", "Нейросеть", "Призрак"};
  return f < FORM_COUNT ? n[f] : "?";
}

Color formColor(Form f) {
  switch (f) {
    case BABY: return hex(0x5fd4e6);
    case JUNIOR: return hex(0x8fd16a);
    case MIDDLE: return hex(0x6aaef5);
    case FREELANCER: return hex(0xf0a35e);
    case SENIOR: return hex(0xb98ce8);
    case ARCHITECT: return hex(0x4fc9a9);
    case HACKER: return hex(0x554b91);
    case LEGACY: return hex(0x9a9483);
    case AI: return hex(0xd352ca);
    case GHOST: return hex(0xe8ecf4);
    default: return hex(0xe5c07b);
  }
}

// Высота от пола до макушки (без антенн и нимбов) — для облачков и подписей.
float formHeight(Form f) {
  if (f >= FORM_COUNT) return 40;
  const artdata::Sprite& sp = artdata::FORM[f];
  return (float)(sp.h - sp.head);
}

// --- части тела ---

static void eye(Canvas& c, float x, float y, float s, const Pose& p, Color iris, bool glowEye, int side, int alpha) {
  float bl = p.blink;
  Color stroke = glowEye ? iris : pal::ink;
  if ((p.eyes == E_NORMAL || p.eyes == E_ANGRY) && bl < 0.65f) {
    float outer = x + side * 4.9f * s, inner = x - side * 3.9f * s;
    float top = y - (p.eyes == E_ANGRY ? 4.0f : 3.3f) * s;
    float innerY = y - 0.6f * s, bottom = y + (2.9f - bl * 2.3f) * s;
    Color fill = glowEye ? iris : pal::white;
    c.triangle(outer, top, inner, innerY, x - side * s, bottom, fill, alpha);
    c.triangle(outer, top, x - side * s, bottom, x + side * 3.8f * s, y + 1.3f * s, fill, alpha);
    float gaze = clampf(p.lookX, -1, 1) * 1.05f * s;
    c.circle(x + gaze, y + 0.45f * s, 1.22f * s, pal::ink, alpha);
    c.line(outer, top, inner, innerY, 1.0f * s, pal::ink, alpha);
    return;
  }
  switch (p.eyes) {
    case E_HAPPY:  // ^ ^
      c.arc(x, y + 1.2f * s, 2.6f * s, 1.5f * s, 3.4f, 6.0f, stroke, alpha);
      return;
    case E_CLOSED:
      c.arc(x, y - 0.8f * s, 2.6f * s, 1.4f * s, 0.35f, 2.8f, stroke, alpha);
      return;
    case E_DIZZY:
      c.ring(x, y, 2.4f * s, 1.1f * s, pal::ink);
      c.circle(x + 0.6f * s, y - 0.4f * s, 0.9f * s, pal::ink);
      return;
    case E_HEART:
      drawHeart(c, x, y, 1.5f * s, pal::red);
      return;
    default: break;
  }
  float ry = 3.3f * s * (1 - bl * 0.92f);
  if (p.eyes == E_WIDE) ry *= 1.2f;
  float rx = 2.7f * s * (p.eyes == E_WIDE ? 1.15f : 1);
  if (ry < 0.7f) {
    c.line(x - rx, y, x + rx, y, 1.2f * s, stroke, alpha);
    return;
  }
  if (glowEye) {
    c.ellipse(x, y, rx + 2.5f, ry + 2.5f, iris, 60);
    c.ellipse(x, y, rx, ry, iris);
    c.ellipse(x, y, rx * 0.45f, ry * 0.45f, pal::white, 200);
    return;
  }
  c.ellipse(x, y, rx, ry, pal::white);
  float px = x + clampf(p.lookX, -1, 1) * rx * 0.4f;
  float py = y + clampf(p.lookY, -1, 1) * ry * 0.35f + ry * 0.12f;
  float pr = 1.9f * s;
  c.clip((int)(x - rx), (int)(y - ry), (int)(x + rx + 1), (int)(y + ry + 1));
  c.ellipse(px, py, pr, pr * (ry / (3.3f * s)) + 0.2f, iris);
  c.ellipse(px, py, pr * 0.55f, pr * 0.55f * (ry / (3.3f * s)) + 0.1f, pal::ink);
  c.noClip();
  c.circle(px + 0.8f * s, py - 1.0f * s, 0.75f * s, pal::white, 230);
  if (p.eyes == E_SAD) c.line(x - rx, y - ry - 1.2f * s, x + rx * 0.7f, y - ry - 2.2f * s, 1.1f * s, pal::ink);
  if (p.eyes == E_ANGRY) c.line(x - rx, y - ry - 2.2f * s, x + rx, y - ry - 0.6f * s, 1.3f * s, pal::ink);
}

static void mouth(Canvas& c, float x, float y, float s, const Pose& p) {
  switch (p.mouth) {
    case M_SMILE: c.arc(x, y - 1.6f * s, 2.6f * s, 1.1f * s, 0.5f, 2.64f, pal::ink); break;
    case M_GRIN:
      c.clip(0, (int)(y - 0.2f * s), W, H);
      c.ellipse(x, y - 0.2f * s, 3.4f * s, 3.0f * s, pal::ink);
      c.ellipse(x, y + 2.0f * s, 2.0f * s, 1.1f * s, pal::pink);
      c.noClip();
      break;
    case M_OPEN:
      c.ellipse(x, y + 0.5f * s, 2.3f * s, 2.6f * s, pal::ink);
      c.ellipse(x, y + 1.7f * s, 1.5f * s, 1.0f * s, pal::pink);
      break;
    case M_CHEW: {
      float k = (fsin(p.t * 0.025f) + 1) * 0.5f;
      c.ellipse(x, y + 0.5f * s, 2.4f * s, (0.6f + 1.6f * k) * s, pal::ink);
      break;
    }
    case M_FLAT: c.line(x - 2.2f * s, y, x + 2.2f * s, y, 1.1f * s, pal::ink); break;
    case M_SAD: c.arc(x, y + 2.2f * s, 2.4f * s, 1.1f * s, 3.7f, 5.7f, pal::ink); break;
    case M_O: c.ring(x, y + 0.5f * s, 1.4f * s, 1.0f * s, pal::ink); break;
    case M_CAT:
      c.arc(x - 1.3f * s, y - 0.6f * s, 1.3f * s, 1.0f * s, 0.2f, 2.9f, pal::ink);
      c.arc(x + 1.3f * s, y - 0.6f * s, 1.3f * s, 1.0f * s, 0.2f, 2.9f, pal::ink);
      break;
    case M_WAVY:
      for (int i = 0; i < 4; i++) {
        float x0 = x - 3 * s + i * 1.5f * s;
        c.line(x0, y + (i & 1 ? 0.8f : -0.8f) * s, x0 + 1.5f * s, y + (i & 1 ? -0.8f : 0.8f) * s, 1.0f * s, pal::ink);
      }
      break;
  }
}

void drawPropellerCap(Canvas& c, float x, float y, float s, float spin, int alpha) {
  c.roundRect(x - 10 * s, y - 2 * s, 20 * s, 8 * s, 4 * s, pal::ink, alpha);
  c.triangle(x - 9 * s, y + 4 * s, x, y - 4 * s, x + 9 * s, y + 4 * s, pal::red, alpha);
  c.triangle(x, y - 4 * s, x + 9 * s, y + 4 * s, x + 2 * s, y + 4 * s, hex(0xc13d58), alpha);
  c.line(x - 9 * s, y + 4 * s, x + 9 * s, y + 4 * s, 1.4f * s, hex(0x822b48), alpha);
  c.line(x, y - 4 * s, x, y - 8 * s, 1.4f * s, pal::yellow, alpha);
  c.ellipse(x, y - 8 * s, (2 + 7 * fabs_(spin)) * s, 1.6f * s, pal::yellow, alpha);
}

// Спрайт из арт-пака: масштаб по осям, отражение, прозрачность и седина.
struct Blit {
  float x0, y0, sx, sy;  // левый верхний угол и размер пикселя спрайта
  bool flip;
  const artdata::Sprite* sp;
  float X(float u) const { return x0 + (flip ? sp->w - u : u) * sx; }
  float Y(float v) const { return y0 + v * sy; }
};

static bool inBox(const uint8_t* b, int u, int v) { return b[2] && u >= b[0] && u <= b[2] && v >= b[1] && v <= b[3]; }

// part: 0 — тело, 1/2 — левый и правый кулак, 3/4 — левая и правая нога (в координатах спрайта).
static int partOf(const artdata::Sprite& sp, int u, int v) {
  for (int k = 0; k < 2; k++) if (inBox(sp.arm[k], u, v)) return 1 + k;
  for (int k = 0; k < 2; k++) if (inBox(sp.leg[k], u, v)) return 3 + k;
  return 0;
}

// Каждый пиксель спрайта — прямоугольник от края до края, поэтому при любом масштабе нет щелей.
// lift — подъём частей в пикселях экрана (кулаки машут, ноги шагают).
static Blit blit(Canvas& c, const artdata::Sprite& sp, float cx, float bottom, float sx, float sy, bool flip, int alpha,
                 int gray, const float* lift = nullptr) {
  Blit b{cx - sp.w * sx * 0.5f, bottom - sp.h * sy, sx, sy, flip, &sp};
  for (int pass = 0; pass < (lift ? 3 : 1); pass++) {  // тело, потом ноги, потом кулаки поверх
    for (int v = 0; v < sp.h; v++) {
      int y0 = (int)(b.y0 + v * sy + 0.5f), y1 = (int)(b.y0 + (v + 1) * sy + 0.5f);
      if (y1 <= y0) continue;
      const uint8_t* row = sp.px + v * sp.w;
      for (int u = 0; u < sp.w; u++) {
        uint8_t k = row[u];
        if (!k) continue;
        int part = lift ? partOf(sp, u, v) : 0;
        if ((pass == 0 && part) || (pass == 1 && part < 3) || (pass == 2 && (part == 0 || part > 2))) continue;
        float du = flip ? sp.w - 1 - u : u;
        int x0 = (int)(b.x0 + du * sx + 0.5f), x1 = (int)(b.x0 + (du + 1) * sx + 0.5f);
        if (x1 <= x0) continue;
        Color col = sp.pal[k];
        if (gray > 0) {
          int l = (((col >> 11) & 31) * 8 + ((col >> 5) & 63) * 4 + (col & 31) * 8) / 3;
          col = mix(col, rgb(l, l, l + 8), gray);
        }
        int dy = part ? -(int)(lift[part] + 0.5f) : 0;
        c.fillRect(x0, y0 + dy, x1 - x0, y1 - y0, col, alpha);
      }
    }
  }
  return b;
}

// Выражение рисуется поверх нарисованных глаз и рта: сначала закрашиваем их цветом кожи.
static void face(Canvas& c, Form f, const Blit& b, const Pose& p, int A) {
  const artdata::Sprite& sp = *b.sp;
  Color ink = f == HACKER ? hex(0x7dff9b) : pal::ink;
  bool closed = p.eyes == E_CLOSED || p.blink >= 0.6f;
  if (closed || (p.eyes != E_NORMAL && p.eyes != E_ANGRY)) {
    for (int k = 0; k < 2; k++) {
      const uint8_t* e = sp.eye[k];
      float x0 = b.X(b.flip ? e[2] + 1 : e[0]), x1 = b.X(b.flip ? e[0] : e[2] + 1);
      float y0 = b.Y(e[1]), y1 = b.Y(e[3] + 1);
      c.fillRect((int)x0 - 1, (int)y0, (int)(x1 - x0 + 0.5f) + 2, (int)(y1 - y0 + 0.5f), sp.skin, A);
      float ex = (x0 + x1) * 0.5f, ey = (y0 + y1) * 0.5f, ew = (x1 - x0) * 0.5f;
      float t = 1.3f * b.sy + 0.2f;
      if (closed) {
        c.arc(ex, ey - ew * 0.35f, ew, ew * 0.55f, 0.35f, 2.8f, ink, A);
        if (t > 1.4f) c.arc(ex, ey - ew * 0.35f + 0.6f, ew, ew * 0.55f, 0.5f, 2.6f, ink, A);
      } else if (p.eyes == E_HAPPY) {
        c.arc(ex, ey + ew * 0.5f, ew, ew * 0.75f, 3.5f, 5.9f, ink, A);
        c.arc(ex, ey + ew * 0.5f + 0.6f, ew, ew * 0.75f, 3.6f, 5.8f, ink, A);
      } else {
        Pose q = p;
        q.blink = 0;
        eye(c, ex, ey, ew / 2.9f, q, f == HACKER ? hex(0x7dff9b) : (f == AI ? pal::cyan : hex(0x3b2a20)),
            f == HACKER || f == AI, k ? 1 : -1, A);
      }
    }
  }
  // Свой рот у спрайта — сердитая черта; остальные выражения рисуем поверх.
  if (p.mouth != M_SMILE && p.mouth != M_FLAT) {
    const uint8_t* m = sp.mouth;
    float x0 = b.X(b.flip ? m[2] + 1 : m[0]), x1 = b.X(b.flip ? m[0] : m[2] + 1);
    float y0 = b.Y(m[1]), y1 = b.Y(m[3] + 1);
    if (f != FREELANCER && f != HACKER)  // оскал и знак </> остаются, рот рисуем ниже них
      c.fillRect((int)x0, (int)y0, (int)(x1 - x0 + 0.5f), (int)(y1 - y0 + 0.5f), sp.skin, A);
    float s = b.sy * 1.1f;
    if (s < 0.8f) s = 0.8f;
    mouth(c, (x0 + x1) * 0.5f, (f == FREELANCER || f == HACKER) ? y1 + 1.5f * s : (y0 + y1) * 0.5f, s, p);
  }
}

void drawPet(Canvas& c, Form f, const Pose& p) {
  if (f >= FORM_COUNT) return;
  if (f == EGG) { drawEgg(c, p.x, p.y, p.shake, 0, p.t); return; }
  const artdata::Sprite& sp = artdata::FORM[f];
  int A = (int)p.alpha;
  float k = p.scale;
  float sq = p.squash;
  float sy = k * sq, sx = k / (sq > 0.3f ? fsqrt(sq) : 0.55f) * (1 + 0.22f * clampf(p.fat, 0, 1));
  if (p.walk) sx *= 1 + 0.03f * fsin(p.walk * 2);
  float sh = p.shake ? fsin(p.t * 0.09f) * p.shake : 0;
  float floatY = (f == AI || f == GHOST) ? 3 + fsin(p.t * 0.003f) * 3 : 0;
  float cx = p.x + sh, by = p.y - p.lift - floatY;  // низ спрайта
  float bw = sp.w * sx * 0.78f, bh = (sp.h - sp.head) * sy;
  float cy = by - bh / 2;

  // тень на полу
  float shK = 1 - clampf((p.lift + floatY) / 40, 0, 0.7f);
  c.ellipse(p.x, p.y + 1, bw * 0.56f * shK, 3.3f * shK, pal::ink, (int)(95 * shK) * A >> 8);
  c.ellipse(p.x, p.y + 1, bw * 0.39f * shK, 1.8f * shK, pal::ink, (int)(135 * shK) * A >> 8);
  if (p.glow > 0) {
    Color gl = shade(formColor(f), 330);
    for (int i = 3; i >= 1; i--) c.ellipse(cx, cy, bw / 2 + i * 4 * p.glow, bh / 2 + i * 4 * p.glow, gl, (int)(40 * p.glow) * A >> 8);
  }
  if (p.cane && f != GHOST && f != AI) {  // трость
    float kx = cx + p.dir * (bw * 0.55f + 4), ky = cy + bh * 0.05f;
    c.line(kx, ky, kx + p.dir * 2, p.y - p.lift, 2.2f, hex(0x7a4b2a), A);
    c.arc(kx - p.dir * 3, ky, 3.2f, 2.2f, p.dir > 0 ? 3.3f : 0.0f, p.dir > 0 ? 6.2f : 2.9f, hex(0x7a4b2a), A);
  }
  int bodyA = f == GHOST ? A * 225 / 255 : A;
  int gray = f == GHOST ? 0 : (int)(clampf(p.gray, 0, 1) * 150);
  // Ноги шагают по очереди, кулаки качаются в такт и поднимаются, когда питомец машет.
  float step = p.walk ? fsin(p.walk) : 0;
  float wave = clampf(p.arm, 0, 1) * 12 * sy;
  float lift[5] = {0, wave + (step > 0 ? step : 0) * 1.2f, wave + (step < 0 ? -step : 0) * 1.2f,
                   (step < 0 ? -step : 0) * 2.4f * sy, (step > 0 ? step : 0) * 2.4f * sy};
  if (p.arm > 0 && p.walk == 0 && f != AI && f != GHOST) {  // машет по очереди
    float w2 = fsin(p.t * 0.02f) * 1.5f * sy;
    lift[1] += w2; lift[2] -= w2;
  }
  if (p.hold && sp.arm[1][2]) lift[2] += p.holdLift * sy;
  lift[2] += p.armR * sy;
  // Купленная шапка или новогодний колпак заменяют головной убор формы: его строки не рисуем.
  bool hatOn = p.hat && !p.sleeping && f != GHOST && sp.hatW;
  if (hatOn && sp.hatCut) c.clip(0, (int)(by - sp.h * sy + sp.hatCut * sy + 0.5f), W, H);
  Blit b = blit(c, sp, cx, by, sx, sy, p.dir < 0, bodyA, gray, lift);
  c.noClip();
  if (p.hold && sp.arm[1][2]) {  // кружка в кулаке со стороны взгляда (правый кулак спрайта)
    const uint8_t* a = sp.arm[1];
    float hx = b.X((a[0] + a[2] + 1) * 0.5f), hy = b.Y(a[1]) - lift[2];
    drawArt(c, *p.hold, (int)(hx - p.hold->w * 0.5f), (int)(hy - p.hold->h * 0.55f), A);
  }
  face(c, f, b, p, A);

  float s = sy * 1.25f;  // масштаб мелких деталей
  if (s < 0.6f) s = 0.6f;
  if (p.blush && f != HACKER && f != BABY) {
    for (int i = 0; i < 2; i++) {
      const uint8_t* e = sp.eye[i];
      float ex = b.X((e[0] + e[2] + 1) * 0.5f) + (b.X(e[2]) > cx ? 2.5f : -2.5f) * s, ey = b.Y(e[3] + 3);
      c.ellipse(ex, ey, 2.6f * s, 1.3f * s, pal::pink, A * 120 >> 8);
    }
  }

  // аксессуары за достижения
  if (f != GHOST) {
    // У персонажей с артов лицо доходит почти до ног, поэтому шарф — тонкий пояс над ногами,
    // а медаль и звезда висят на нём по бокам, лица не касаются.
    float waist = sp.leg[0][2] ? b.Y(sp.leg[0][1] + 1.5f) : by - bh * 0.1f;
    float hw = bw * 0.3f;
    if (p.acc & ACC_SCARF) {
      c.roundRect(cx - hw, waist - 1.5f * s, hw * 2, 3 * s, 1.5f * s, hex(0xd9455a), A);
      for (int i = -2; i <= 2; i++) c.pixel((int)(cx + i * hw * 0.4f), (int)waist, pal::white, A * 150 >> 8);
      c.roundRect(cx + hw * 0.5f, waist, 2.5f * s, 5 * s, 1.2f * s, hex(0xb8364a), A);
    }
    if (p.acc & ACC_MEDAL) {
      float mx = cx - hw * 0.75f, my = waist + 1.5f * s;
      c.circle(mx, my, 2.4f * s, hex(0xb8860b), A);
      c.circle(mx, my, 1.7f * s, hex(0xf5c542), A);
    }
    if (p.acc & ACC_STAR) drawStar(c, cx + hw * 0.15f, waist + 1.5f * s, 1.1f * s, hex(0xf5c542), A);
  }
  float top = b.Y(sp.head) + 1;
  if (hatOn) {
    drawHat(c, p.hat, cx, b.Y(sp.hatY) - 2.5f * sy, sp.hatW * sx * 0.95f, s, p.dir, p.t);
  } else if ((p.acc & ACC_CROWN) && !p.sleeping && f != GHOST) {
    float kx = cx + bw * 0.22f, ky = top + 1;
    c.roundRect(kx - 6 * s, ky - 2 * s, 12 * s, 4 * s, 1, hex(0xf5c542), A);
    for (int i = -1; i <= 1; i++) c.triangle(kx + i * 4 * s - 2.4f * s, ky - 1.5f * s, kx + i * 4 * s + 2.4f * s, ky - 1.5f * s, kx + i * 4 * s, ky - 7 * s, hex(0xf5c542), A);
    c.circle(kx, ky - 0.2f * s, 1.1f * s, pal::red, A);
  }
  if (f == BABY && (p.t / 500) & 1) {  // лампочка на антенне мигает
    float ax = b.X(sp.w * 0.5f), ay = b.Y(4);
    c.circle(ax, ay, 5 * sy, pal::yellow, 50 * A >> 8);
  }
  if (p.sweat) {
    float sx2 = cx + bw * 0.45f, sy2 = top + bh * 0.25f + (p.t % 1200) * 0.006f;
    c.circle(sx2, sy2 + 2, 2.2f, pal::cyan, 220);
    c.triangle(sx2 - 2, sy2 + 1.6f, sx2 + 2, sy2 + 1.6f, sx2, sy2 - 3, pal::cyan, 220);
  }
}

void drawArt(Canvas& c, const artdata::Sprite& sp, int x, int y, int alpha) {
  for (int v = 0; v < sp.h; v++)
    for (int u = 0; u < sp.w; u++)
      if (uint8_t k = sp.px[v * sp.w + u]) c.pixel(x + u, y + v, sp.pal[k], alpha);
}

void drawEgg(Canvas& c, float x, float y, float wobble, float crack, uint32_t t) {
  float rot = wobble * fsin(t * 0.02f) * 3;
  const artdata::Sprite& sp = artdata::FORM[EGG];
  c.ellipse(x, y + 1, 15, 2.6f, pal::ink, 110);
  blit(c, sp, x + rot, y + 1, 1, 1, false, 255, 0);
  float cx = x + rot, cy = y - sp.h * 0.4f;
  if (crack > 0) {  // трещины расходятся от верхней
    float k = clampf(crack, 0, 1);
    float pts[] = {-13, -2, -7, 3, -1, -3, 5, 3, 10, -2, 14, 2};
    for (int i = 0; i < 5; i++) {
      if (i >= k * 5.99f) break;
      float x0 = cx + pts[i * 2], y0 = cy + pts[i * 2 + 1];
      float x1 = cx + pts[i * 2 + 2], y1 = cy + pts[i * 2 + 3];
      c.line(x0, y0, x1, y1, 2.7f, hex(0x483027));
      c.line(x0, y0 + 0.6f, x1, y1 + 0.6f, 0.9f, hex(0xffb349), 100 + (int)(k * 150));
    }
  }
}

void drawTomb(Canvas& c, float x, float y, const char* l1, const char* l2) {
  c.ellipse(x, y + 1, 52, 3, pal::ink, 120);
  c.roundRectGrad(x - 46, y - 62, 92, 64, 18, hex(0x8a93a6), hex(0x596173));
  c.roundRect(x - 49, y - 6, 98, 8, 0, hex(0x4a5161));
  c.text(font_ui, (int)x, (int)y - 58, "R.I.P.", hex(0x2a2f3a), 255, CENTER);
  c.text(font_small, (int)x, (int)y - 39, l1, hex(0x2a2f3a), 255, CENTER);
  c.text(font_small, (int)x, (int)y - 24, l2, hex(0x2a2f3a), 255, CENTER);
}

// --- шапки ---

void drawHat(Canvas& c, uint8_t hat, float cx, float top, float bw, float s, int dir, uint32_t t) {
  switch (hat) {
    case 1: {  // шапка с помпоном
      Color a = hex(0xd9455a), b = hex(0xf2f4f8);
      c.clip(0, 0, W, (int)top + 6);
      c.ellipse(cx, top + 6, bw * 0.38f, 11 * s, a);
      c.noClip();
      for (int i = -1; i <= 1; i++) c.line(cx + i * bw * 0.12f, top + 5, cx + i * bw * 0.08f, top - 3 * s, 1.4f * s, shade(a, 180));
      c.roundRect(cx - bw * 0.4f, top + 2, bw * 0.8f, 5 * s, 2.5f * s, b);
      c.circle(cx, top - 5 * s, 3.4f * s, b);
      break;
    }
    case 2: {  // кепка козырьком по ходу
      Color a = hex(0x3b82f6);
      c.clip(0, 0, W, (int)top + 5);
      c.ellipse(cx, top + 5, bw * 0.35f, 8.5f * s, a);
      c.noClip();
      c.ellipse(cx + dir * bw * 0.32f, top + 4, bw * 0.24f, 2.4f * s, shade(a, 170));
      c.circle(cx, top - 3 * s, 1.4f * s, shade(a, 150));
      break;
    }
    case 3: {  // цилиндр
      Color a = hex(0x23262e);
      c.ellipse(cx, top + 4, bw * 0.44f, 3 * s, a);
      c.roundRect(cx - bw * 0.24f, top - 14 * s, bw * 0.48f, 18 * s, 2, a);
      c.fillRect((int)(cx - bw * 0.24f), (int)(top - 1 * s), (int)(bw * 0.48f), (int)(3 * s), pal::red);
      c.line(cx - bw * 0.18f, top - 12 * s, cx - bw * 0.18f, top - 3 * s, 1, hex(0x4a4f5c));
      break;
    }
    case 4: {  // ковбойская шляпа
      Color a = hex(0xa0703f), b = hex(0x7a5230);
      c.ellipse(cx, top + 4, bw * 0.56f, 3.4f * s, b);
      c.roundRect(cx - bw * 0.25f, top - 9 * s, bw * 0.5f, 13 * s, 4 * s, a);
      c.line(cx, top - 8 * s, cx, top - 2 * s, 1.4f, b);
      c.fillRect((int)(cx - bw * 0.25f), (int)(top + 0 * s), (int)(bw * 0.5f), (int)(2 * s), hex(0x3b2a1a));
      break;
    }
    case 5: {  // колпак волшебника со звёздами
      Color a = hex(0x5b3fb5);
      float tipx = cx + dir * bw * 0.18f, tipy = top - 22 * s;
      c.triangle(cx - bw * 0.32f, top + 4, cx + bw * 0.32f, top + 4, tipx, tipy, a);
      c.ellipse(cx, top + 4, bw * 0.42f, 3 * s, shade(a, 170));
      drawStar(c, cx - bw * 0.08f, top - 4 * s, 1.0f * s, pal::yellow);
      drawStar(c, cx + bw * 0.1f, top - 11 * s, 0.8f * s, pal::yellow, 200 + (int)(fsin(t * 0.006f) * 50));
      break;
    }
    case 6: {  // кошачьи ушки
      Color a = hex(0x554b91), p = pal::pink;
      for (int k = -1; k <= 1; k += 2) {
        float ex = cx + k * bw * 0.26f;
        c.triangle(ex - 6 * s, top + 4, ex + 6 * s, top + 4, ex + k * 2 * s, top - 9 * s, a);
        c.triangle(ex - 3 * s, top + 3, ex + 3 * s, top + 3, ex + k * 1.4f * s, top - 5 * s, p);
      }
      c.roundRect(cx - bw * 0.36f, top + 2, bw * 0.72f, 3 * s, 1.5f * s, a);
      break;
    }
    case 20: {  // новогодний колпак: красный купол, кончик загнут набок, белый мех и помпон
      Color a = hex(0xd62839), d = hex(0xa31d33), l = hex(0xef4b5c), b = pal::white, g = hex(0xc9d2e0);
      float h = bw * 0.42f;
      float tipx = cx + dir * bw * 0.6f, tipy = top - h * 0.25f;
      c.clip(0, 0, W, (int)top + 2);
      c.ellipse(cx, top + 2, bw * 0.47f, h, a);
      c.ellipse(cx + dir * bw * 0.16f, top + 2, bw * 0.3f, h * 0.9f, d);   // тень с той стороны, куда загнут
      c.ellipse(cx - dir * bw * 0.18f, top - h * 0.45f, bw * 0.12f, h * 0.25f, l, 200);  // блик
      c.noClip();
      // загнутый кончик свисает набок
      c.triangle(cx + dir * bw * 0.05f, top - h * 0.95f, cx + dir * bw * 0.38f, top - h * 0.45f, tipx, tipy, a);
      c.triangle(cx + dir * bw * 0.25f, top - h * 0.75f, cx + dir * bw * 0.38f, top - h * 0.45f, tipx, tipy, d);
      // меховой кант
      c.roundRect(cx - bw * 0.52f, top - 1, bw * 1.04f, 6 * s, 3 * s, b);
      c.hline((int)(cx - bw * 0.47f), (int)(top - 1 + 5 * s), (int)(bw * 0.94f), g);
      for (int i = 0; i < 6; i++) c.pixel((int)(cx - bw * 0.42f + i * bw * 0.17f), (int)(top + 1.5f * s), g);
      c.circle(tipx, tipy, 3.2f * s, b);
      c.circle(tipx + dir * 0.8f * s, tipy + 0.9f * s, 1.5f * s, g, 170);
      break;
    }
    default: break;
  }
}

// --- праздничные украшения ---

// Ёлка в углу комнаты: ярусы с тенью, мерцающая гирлянда, золотая звезда и подарки под ней.
void drawTree(Canvas& c, float x, float floor, uint32_t t) {
  Color g1 = hex(0x2f8f4e), g2 = hex(0x1f6b3a), g3 = hex(0x4fb46a), ink = hex(0x0d1f16);
  c.fillRect((int)x - 2, (int)floor - 6, 4, 6, hex(0x6b4226));
  for (int i = 0; i < 4; i++) {  // от нижнего яруса к верхнему
    float w = 12 - i * 2.6f, y = floor - 5 - i * 7.5f;
    c.triangle(x - w - 1, y + 1, x + w + 1, y + 1, x, y - 11, ink);
    c.triangle(x - w, y, x + w, y, x, y - 10, g1);
    c.triangle(x, y - 10, x + w, y, x + w * 0.2f, y, g2);
    c.line(x - w * 0.6f, y - 2, x - w * 0.1f, y - 7, 1, g3, 160);
  }
  // гирлянда: огоньки бегут волной и мерцают
  static const Color lc[] = {pal::red, pal::yellow, pal::cyan, pal::pink, hex(0x8fff8f)};
  for (int i = 0; i < 14; i++) {
    int row = i / 4;
    float k = (i % 4) / 3.0f;
    float w = 10 - row * 2.6f;
    float lx = x - w + k * w * 2, ly = floor - 7 - row * 7.5f - k * 2;
    int ph = (int)((t / 140 + i * 3) % 10);
    int a = ph < 3 ? 255 : (ph < 6 ? 150 : 60);
    c.pixel((int)lx, (int)ly, lc[i % 5], a);
    if (a == 255) c.circle(lx, ly, 1.6f, lc[i % 5], 70);
  }
  // звезда на макушке мерцает
  float star = 1.5f + fsin(t * 0.006f) * 0.2f;
  drawStar(c, x, floor - 38, star, hex(0xffd23f));
  c.circle(x, floor - 38, 4, hex(0xffd23f), 40 + (int)(fsin(t * 0.006f) * 20));
  // подарки
  c.fillRect((int)x - 15, (int)floor - 5, 7, 5, hex(0xc0392b));
  c.vline((int)x - 12, (int)floor - 5, 5, hex(0xf5d482));
  c.fillRect((int)x + 7, (int)floor - 6, 8, 6, hex(0x2e86de));
  c.vline((int)x + 10, (int)floor - 6, 6, pal::white);
  c.hline((int)x + 7, (int)floor - 4, 8, pal::white);
  c.fillRect((int)x - 6, (int)floor - 3, 5, 3, hex(0x8e44ad));
  c.vline((int)x - 4, (int)floor - 3, 3, hex(0xf5d482));
}

void drawCake(Canvas& c, float x, float floor, const char* label, uint32_t t) {
  c.ellipse(x, floor, 15, 2.5f, pal::ink, 100);
  c.roundRectGrad(x - 13, floor - 15, 26, 14, 3, hex(0xffe0ec), hex(0xf2a7c3));
  c.roundRect(x - 13, floor - 16, 26, 5, 2.5f, hex(0xfff6fa));
  for (int i = 0; i < 3; i++) {  // свечи с огоньками
    float cxp = x - 7 + i * 7;
    c.fillRect((int)cxp - 1, (int)floor - 23, 2, 7, pal::cyan);
    float fl = fsin(t * 0.02f + i) * 0.6f;
    c.ellipse(cxp + fl, floor - 25, 1.5f, 2.6f, pal::yellow);
    c.ellipse(cxp + fl, floor - 25, 3.5f, 4, pal::yellow, 60);
  }
  if (label) c.text(font_mono, (int)x, (int)floor - 13, label, hex(0x9c2a5a), 255, CENTER);
}

void drawPumpkin(Canvas& c, float x, float floor, uint32_t t) {
  Color o1 = hex(0xf08a24), o2 = hex(0xc25e0b);
  c.ellipse(x, floor, 15, 2.5f, pal::ink, 100);
  c.ellipse(x - 6, floor - 8, 7, 8, o2);
  c.ellipse(x + 6, floor - 8, 7, 8, o2);
  c.ellipse(x, floor - 8, 8, 9, o1);
  c.fillRect((int)x - 1, (int)floor - 20, 3, 5, hex(0x4a6b2a));
  int glow = 180 + (int)(fsin(t * 0.008f) * 60);
  c.triangle(x - 6, floor - 10, x - 2, floor - 10, x - 4, floor - 13, pal::yellow, glow);
  c.triangle(x + 2, floor - 10, x + 6, floor - 10, x + 4, floor - 13, pal::yellow, glow);
  c.triangle(x - 5, floor - 6, x + 5, floor - 6, x, floor - 3, pal::yellow, glow);
}

// --- предметы ---

void drawCoffee(Canvas& c, float x, float y, float fill, uint32_t t) {
  for (int i = 0; i < 3; i++) {  // пар
    float ph = (t * 0.002f + i * 0.33f);
    ph -= (int)ph;
    float sx = x - 4 + i * 4 + fsin(ph * 6 + i) * 2;
    c.circle(sx, y - 15 - ph * 10, 1.6f + ph * 1.6f, pal::white, (int)(120 * (1 - ph)));
  }
  c.ring(x + 8, y - 6, 4, 2, hex(0xe8e3d8));
  c.roundRectGrad(x - 8, y - 14, 16, 15, 3, hex(0xfffaf0), hex(0xd8d0c0));
  float lv = 11 * clampf(fill, 0, 1);
  if (lv > 0.5f) c.roundRect(x - 6.5f, y - 2 - lv, 13, lv, 2, hex(0x6f4a2e));
  c.ellipse(x, y - 13, 7, 1.6f, hex(0x4a2f1c));
  c.text(font_mono, (int)x - 5, (int)y - 11, "JS", pal::orange, 0);
}

void drawPizza(Canvas& c, float x, float y, int bites) {
  c.triangle(x - 12, y - 14, x + 12, y - 14, x, y + 6, hex(0xf2c46b));
  c.triangle(x - 10, y - 13, x + 10, y - 13, x, y + 3, hex(0xf7d48a));
  c.roundRect(x - 13, y - 17, 26, 5, 2.5f, hex(0xc98a3e));
  float pep[] = {-5, -9, 4, -8, 0, -3, 2, -11};
  for (int i = 0; i < 4; i++) c.circle(x + pep[i * 2], y + pep[i * 2 + 1], 2.2f, pal::red);
  for (int b = 0; b < bites && b < 3; b++) c.circle(x, y + 6 - b * 6, 5.5f, pal::editor);
}

void drawCookie(Canvas& c, float x, float y, int bites) {
  c.circle(x, y - 8, 10, hex(0xb07a3c));
  c.circle(x, y - 8, 9, hex(0xd39a52));
  float ch[] = {-4, -11, 3, -12, 5, -5, -2, -4, -5, -7, 1, -8};
  for (int i = 0; i < 6; i++) c.circle(x + ch[i * 2], y + ch[i * 2 + 1], 1.5f, hex(0x4a2c17));
  for (int b = 0; b < bites && b < 3; b++) c.circle(x + 9 - b * 2, y - 14 + b * 6, 4.5f, pal::editor);
}

void drawBug(Canvas& c, float x, float y, float phase, int dir, float k, Color col) {
  float lg = fsin(phase) * 1.5f * k;
  for (int i = -1; i <= 1; i++) {
    float lx = x + i * 3.2f * k;
    c.line(lx, y - 3 * k, lx - 2 * k + (i & 1 ? lg : -lg), y + 0.5f, 0.9f, pal::ink);
    c.line(lx, y - 3 * k, lx + 2 * k + (i & 1 ? -lg : lg), y + 0.5f, 0.9f, pal::ink);
  }
  c.ellipse(x, y - 4 * k, 5.5f * k, 3.8f * k, shade(col, 120));
  c.ellipse(x, y - 4.4f * k, 4.8f * k, 3.2f * k, col);
  c.line(x, y - 7.4f * k, x, y - 1 * k, 0.9f, shade(col, 110));
  c.circle(x + dir * 5 * k, y - 4.6f * k, 2.3f * k, pal::ink);
  c.circle(x - 1.8f * k, y - 5.6f * k, 0.9f * k, pal::ink);
  c.circle(x + 1.8f * k, y - 3.2f * k, 0.9f * k, pal::ink);
  c.line(x + dir * 6.4f * k, y - 5.6f * k, x + dir * 8.5f * k, y - 8.5f * k, 0.8f, pal::ink);
}

void drawFoodIcon(Canvas& c, int kind, float x, float y) {
  if (kind == 0) {
    c.ring(x + 4.5f, y, 2.2f, 1.3f, hex(0xe8e3d8));
    c.roundRectGrad(x - 4.5f, y - 4.5f, 9, 9, 2, hex(0xfffaf0), hex(0xd8d0c0));
    c.ellipse(x, y - 3.6f, 3.6f, 1, hex(0x6f4a2e));
  } else if (kind == 1) {
    c.triangle(x - 6, y - 4, x + 6, y - 4, x, y + 6, hex(0xf2c46b));
    c.roundRect(x - 6.5f, y - 6, 13, 3, 1.5f, hex(0xc98a3e));
    c.circle(x - 1.5f, y - 1.5f, 1.3f, pal::red);
    c.circle(x + 2, y - 2.4f, 1.1f, pal::red);
  } else {
    c.circle(x, y, 5.5f, hex(0xd39a52));
    c.circle(x - 2, y - 1.5f, 1, hex(0x4a2c17));
    c.circle(x + 2, y + 1, 1, hex(0x4a2c17));
    c.circle(x + 1.5f, y - 2.5f, 0.9f, hex(0x4a2c17));
  }
}

void drawHeart(Canvas& c, float x, float y, float s, Color col, int a) {
  c.circle(x - 1.6f * s, y - 0.8f * s, 1.8f * s, col, a);
  c.circle(x + 1.6f * s, y - 0.8f * s, 1.8f * s, col, a);
  c.triangle(x - 3.3f * s, y - 0.2f * s, x + 3.3f * s, y - 0.2f * s, x, y + 3.2f * s, col, a);
}

void drawBolt(Canvas& c, float x, float y, float s, Color col, int a) {
  c.triangle(x + 1 * s, y - 4 * s, x - 2.5f * s, y + 0.8f * s, x + 0.4f * s, y + 0.8f * s, col, a);
  c.triangle(x - 1 * s, y + 4 * s, x + 2.5f * s, y - 0.8f * s, x - 0.4f * s, y - 0.8f * s, col, a);
}

void drawStar(Canvas& c, float x, float y, float s, Color col, int a) {
  c.triangle(x, y - 3 * s, x - 1 * s, y, x + 1 * s, y, col, a);
  c.triangle(x, y + 3 * s, x - 1 * s, y, x + 1 * s, y, col, a);
  c.triangle(x - 3 * s, y, x, y - 1 * s, x, y + 1 * s, col, a);
  c.triangle(x + 3 * s, y, x, y - 1 * s, x, y + 1 * s, col, a);
}

void drawMoon(Canvas& c, float x, float y, float r, Color sky) {
  c.circle(x, y, r + 4, hex(0xfff1b8), 30);
  c.circle(x, y, r, hex(0xfff1b8));
  c.circle(x + r * 0.45f, y - r * 0.25f, r * 0.85f, sky);
}

void drawCloud(Canvas& c, float x, float y, float s, Color col, int a) {
  c.circle(x - 6 * s, y, 4.5f * s, col, a);
  c.circle(x, y - 2.5f * s, 6 * s, col, a);
  c.circle(x + 6 * s, y, 4.5f * s, col, a);
  c.roundRect(x - 10 * s, y - 1 * s, 20 * s, 5.5f * s, 2.5f * s, col, a);
}

void drawBattery(Canvas& c, int x, int y, int pct, bool ch) {
  c.roundRect(x, y, 16, 8, 2, pal::dim);
  c.roundRect(x + 1, y + 1, 14, 6, 1.5f, pal::gutter);
  c.fillRect(x + 16, y + 2, 2, 4, pal::dim);
  int w = clampi(pct, 0, 100) * 12 / 100;
  Color col = pct > 40 ? pal::green : (pct > 15 ? pal::yellow : pal::red);
  if (w > 0) c.fillRect(x + 2, y + 2, w, 4, col);
  if (ch) drawBolt(c, x + 8, y + 4, 0.9f, pal::white);
}

void drawIcon(Canvas& c, Icon i, float x, float y, Color col, uint32_t t, float k) {
  Color bg = pal::gutter;
  switch (i) {
    case I_FOOD:  // кружка с паром
      c.roundRect(x - 5.5f * k, y - 3.5f * k, 9.5f * k, 9.5f * k, 2 * k, col);
      c.ring(x + 5 * k, y + 1 * k, 2.4f * k, 1.5f * k, col);
      c.line(x - 2.5f * k, y - 5.5f * k, x - 1.5f * k, y - 8 * k, 1.2f * k, col);
      c.line(x + 0.5f * k, y - 5.5f * k, x + 1.5f * k, y - 8 * k, 1.2f * k, col);
      break;
    case I_GAME:  // геймпад
      c.roundRect(x - 7.5f * k, y - 4 * k, 15 * k, 9 * k, 4.5f * k, col);
      c.line(x - 5.5f * k, y + 0.5f * k, x - 1.5f * k, y + 0.5f * k, 1.4f * k, bg);
      c.line(x - 3.5f * k, y - 1.5f * k, x - 3.5f * k, y + 2.5f * k, 1.4f * k, bg);
      c.circle(x + 3 * k, y - 0.5f * k, 1.2f * k, bg);
      c.circle(x + 5 * k, y + 1.8f * k, 1.2f * k, bg);
      break;
    case I_LIGHT:  // лампочка
      c.circle(x, y - 2 * k, 4.8f * k, col);
      c.roundRect(x - 2.6f * k, y + 2 * k, 5.2f * k, 4.5f * k, 1 * k, col);
      c.line(x - 2.6f * k, y + 4.4f * k, x + 2.6f * k, y + 4.4f * k, 0.9f * k, bg);
      break;
    case I_CLEAN:  // метла
      c.line(x + 5 * k, y - 7 * k, x - 1 * k, y + 1 * k, 1.8f * k, col);
      c.triangle(x - 2.5f * k, y - 1 * k, x + 2.5f * k, y + 2.5f * k, x - 6.5f * k, y + 7 * k, col);
      break;
    case I_HEAL:  // крест — отладчик
      c.circle(x, y, 6.5f * k, col);
      c.roundRect(x - 1.3f * k, y - 4 * k, 2.6f * k, 8 * k, 0.6f * k, bg);
      c.roundRect(x - 4 * k, y - 1.3f * k, 8 * k, 2.6f * k, 0.6f * k, bg);
      break;
    case I_STATUS:  // столбики
      c.roundRect(x - 6.5f * k, y + 1 * k, 3.4f * k, 5.5f * k, 0.8f * k, col);
      c.roundRect(x - 1.7f * k, y - 3 * k, 3.4f * k, 9.5f * k, 0.8f * k, col);
      c.roundRect(x + 3.1f * k, y - 6.5f * k, 3.4f * k, 13 * k, 0.8f * k, col);
      break;
    case I_REVIEW:  // облачко комментария
      c.roundRect(x - 7 * k, y - 6 * k, 14 * k, 10 * k, 3 * k, col);
      c.triangle(x - 4 * k, y + 3 * k, x + 1 * k, y + 3 * k, x - 5 * k, y + 7 * k, col);
      c.line(x - 2.5f * k, y + 1.5f * k, x - 0.5f * k, y - 3.5f * k, 1.2f * k, bg);
      c.line(x + 1 * k, y + 1.5f * k, x + 3 * k, y - 3.5f * k, 1.2f * k, bg);
      break;
    case I_WALK:  // метка
      c.circle(x, y - 2 * k, 5.2f * k, col);
      c.triangle(x - 4.5f * k, y, x + 4.5f * k, y, x, y + 7 * k, col);
      c.circle(x, y - 2 * k, 2 * k, bg);
      break;
    case I_SHOP: {  // сумка с монетой
      c.roundRect(x - 6.5f * k, y - 3 * k, 13 * k, 10 * k, 2.5f * k, col);
      c.arc(x, y - 3 * k, 3.6f * k, 1.6f * k, 3.3f, 6.1f, col);
      c.circle(x, y + 2 * k, 2.6f * k, bg);
      c.circle(x, y + 2 * k, 1.4f * k, col);
      break;
    }
    case I_SETTINGS: {  // шестерёнка
      float an = t * 0.0005f;
      for (int q = 0; q < 6; q++) {
        float a = an + q * 1.047f;
        c.line(x + fcos(a) * 3 * k, y + fsin(a) * 3 * k, x + fcos(a) * 6.5f * k, y + fsin(a) * 6.5f * k, 2.6f * k, col);
      }
      c.circle(x, y, 4.6f * k, col);
      c.circle(x, y, 1.9f * k, bg);
      break;
    }
    default: break;
  }
}

}  // namespace pet
