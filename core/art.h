// Рисование питомца (спрайты арт-пака), предметов и значков.
#pragma once
#include "gfx.h"
#include "artdata.h"

namespace pet {

using gfx::Canvas;
using gfx::Color;

// палитра (в духе тёмной темы редактора)
namespace pal {
constexpr Color bg = gfx::hex(0x111b2c);
constexpr Color editor = gfx::hex(0x152237);
constexpr Color gutter = gfx::hex(0x0d1728);
constexpr Color line = gfx::hex(0x273a52);
constexpr Color fg = gfx::hex(0xbccce0);
constexpr Color dim = gfx::hex(0x8598b2);
constexpr Color white = gfx::hex(0xf1f7ff);
constexpr Color ink = gfx::hex(0x09101e);
constexpr Color red = gfx::hex(0xff7887);
constexpr Color orange = gfx::hex(0xf5ad73);
constexpr Color yellow = gfx::hex(0xf5d482);
constexpr Color green = gfx::hex(0xa5db86);
constexpr Color cyan = gfx::hex(0x78e5d2);
constexpr Color blue = gfx::hex(0x83baff);
constexpr Color purple = gfx::hex(0xc678dd);
constexpr Color pink = gfx::hex(0xff8fab);
constexpr Color accent = gfx::hex(0x456db9);
}  // namespace pal

enum Form : uint8_t { EGG, BABY, JUNIOR, MIDDLE, FREELANCER, SENIOR, ARCHITECT, HACKER, LEGACY, AI, GHOST, FORM_COUNT };

enum Mouth : uint8_t { M_SMILE, M_OPEN, M_FLAT, M_SAD, M_O, M_CAT, M_CHEW, M_GRIN, M_WAVY };
enum Eyes : uint8_t { E_NORMAL, E_HAPPY, E_CLOSED, E_SAD, E_WIDE, E_DIZZY, E_ANGRY, E_HEART };

struct Pose {
  float x = 120, y = 106;  // середина, линия пола
  float squash = 1;        // >1 вытянут, <1 сплющен
  float lift = 0;          // высота прыжка
  float lean = 0;          // наклон -1..1
  int dir = 1;             // куда смотрит
  float blink = 0;         // 0 открыты .. 1 закрыты
  float lookX = 0, lookY = 0;
  float walk = 0;          // фаза шага
  float arm = 0;           // взмах руками 0..1
  float shake = 0;         // дрожь (болезнь, отказ)
  float glow = 0;          // свечение (эволюция)
  float alpha = 255;
  Mouth mouth = M_SMILE;
  Eyes eyes = E_NORMAL;
  bool blush = false;
  bool sweat = false;
  float scale = 1;         // размер (коллекция, мини-игры)
  float fat = 0;           // полнота 0..1 — от лишнего веса
  float gray = 0;          // седина 0..1 — к старости
  bool cane = false;       // трость в глубокой старости
  bool sleeping = false;   // во сне головной убор лежит рядом
  uint8_t hat = 0;         // купленная шапка (Item 1–6) или 20 — новогодняя; заменяет шапку формы
  const artdata::Sprite* hold = nullptr;  // предмет в руке (кружка) — в кулаке со стороны взгляда
  float holdLift = 0;      // насколько поднят этот кулак (пьёт)
  float armR = 0;          // отдельно поднять кулак со стороны взгляда (машет), в пикселях спрайта
  uint32_t t = 0;          // мс, для мелких анимаций
};

const char* formName(Form f);
Color formColor(Form f);
float formHeight(Form f);  // высота тела в пикселях

void drawPet(Canvas& c, Form f, const Pose& p);
// картинка из арт-пака как есть: левый верхний угол
void drawArt(Canvas& c, const artdata::Sprite& sp, int x, int y, int alpha = 255);
void drawPropellerCap(Canvas& c, float x, float y, float s, float spin, int alpha = 255);
void drawEgg(Canvas& c, float x, float y, float wobble, float crack, uint32_t t);
void drawTomb(Canvas& c, float x, float y, const char* line1, const char* line2);

// предметы
void drawCoffee(Canvas& c, float x, float y, float fill, uint32_t t);
void drawPizza(Canvas& c, float x, float y, int bites);
void drawCookie(Canvas& c, float x, float y, int bites);
void drawFoodIcon(Canvas& c, int kind, float x, float y);
void drawBug(Canvas& c, float x, float y, float phase, int dir, float scale = 1, Color col = pal::red);
void drawHeart(Canvas& c, float x, float y, float s, Color col, int a = 255);
void drawBolt(Canvas& c, float x, float y, float s, Color col, int a = 255);
void drawStar(Canvas& c, float x, float y, float s, Color col, int a = 255);
void drawMoon(Canvas& c, float x, float y, float r, Color sky);
void drawCloud(Canvas& c, float x, float y, float s, Color col, int a = 255);
void drawBattery(Canvas& c, int x, int y, int pct, bool charging);
void drawHat(Canvas& c, uint8_t hat, float cx, float top, float bw, float s, int dir, uint32_t t);
// праздничные украшения на полу
void drawTree(Canvas& c, float x, float floor, uint32_t t);
void drawCake(Canvas& c, float x, float floor, const char* label, uint32_t t);
void drawPumpkin(Canvas& c, float x, float floor, uint32_t t);

// значки меню (центр, размер ~14)
enum Icon : uint8_t { I_FOOD, I_GAME, I_LIGHT, I_CLEAN, I_HEAL, I_STATUS, I_REVIEW, I_WALK, I_SHOP, I_SETTINGS, ICON_COUNT };
void drawIcon(Canvas& c, Icon i, float x, float y, Color col, uint32_t t, float k = 1);

}  // namespace pet
