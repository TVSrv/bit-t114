// Отрисовка всех экранов.
#include "game.h"
#include "util.h"
#include "artdata.h"
#include "rooms.h"

namespace pet {
const char* favFoodName(uint8_t f);
const char* favGameName(uint8_t g);
}

namespace pet {
using namespace gfx;

static constexpr int TOP = 22;     // высота верхней панели
static constexpr int MENU_Y = 109; // начало нижней панели
// Арт комнат по-прежнему 240x87: переносим его на 19 пикселей вниз,
 // вместе с мебелью, окнами, питомцем и слоем одеяла, без искажения спрайтов.
static constexpr int ROOM_SHIFT_Y = 19;
static constexpr int FLOOR = 120;  // линия пола соответствует оригинальному y=101 + 19

static const char* FOOD_ITEMS[] = {"Кофе", "Пицца", "Печенька", "← Назад"};
static const char* FOOD_HINT[] = {"+энергия, +немного сытости", "+сытость, +вес", "+настроение, много — вредно", ""};
static const char* GAME_ITEMS[] = {"Отладка", "Память", "Деплой", "Пинг", "Компиляция", "Реакция", "← Назад"};
// магазин: строка списка -> вещь (0 — «без шапки» и «стандартная тема»)
static uint8_t shopItem(int row) {
  static const uint8_t rows[] = {IT_NONE, IT_BEANIE, IT_CAP, IT_TOPHAT, IT_COWBOY, IT_WIZARD, IT_CATEARS,
                                 IT_NONE, IT_TH_DRACULA, IT_TH_MONOKAI, IT_TH_LIGHT, IT_TH_MATRIX};
  return row >= 0 && row < 12 ? rows[row] : (uint8_t)IT_NONE;
}
uint8_t shopItemAt(int row) { return shopItem(row); }

static const char* GAME_HINT[] = {"Нажми, когда курсор на баге", "Запомни и повтори нули и единицы",
                                  "Прыгай через баги", "Жми, когда кольца совпадут", "Останови сборку в зелёной зоне",
                                  "Жми, когда загорится зелёный", ""};

// --- фон: код в редакторе ---

// темы редактора на фоне (покупаются в магазине)
struct Theme { Color bg, gutter, line, txt, cmt, str, kw, fn, typ, var, num, brace; };
static const Theme THEMES[5] = {
    {pal::editor, pal::gutter, pal::line, pal::fg, pal::dim, pal::green, pal::purple, pal::blue, pal::yellow, pal::red, pal::orange, pal::yellow},
    // Dracula
    {gfx::hex(0x282a36), gfx::hex(0x21222c), gfx::hex(0x343746), gfx::hex(0xf8f8f2), gfx::hex(0x6272a4), gfx::hex(0xf1fa8c),
     gfx::hex(0xff79c6), gfx::hex(0x50fa7b), gfx::hex(0x8be9fd), gfx::hex(0xbd93f9), gfx::hex(0xbd93f9), gfx::hex(0xf8f8f2)},
    // Monokai
    {gfx::hex(0x272822), gfx::hex(0x1e1f1c), gfx::hex(0x3e3d32), gfx::hex(0xf8f8f2), gfx::hex(0x75715e), gfx::hex(0xe6db74),
     gfx::hex(0xf92672), gfx::hex(0xa6e22e), gfx::hex(0x66d9ef), gfx::hex(0xfd971f), gfx::hex(0xae81ff), gfx::hex(0xf8f8f2)},
    // светлая
    {gfx::hex(0xf4f5f7), gfx::hex(0xe6e8ec), gfx::hex(0xe1e5ee), gfx::hex(0x383a42), gfx::hex(0xa0a1a7), gfx::hex(0x50a14f),
     gfx::hex(0xa626a4), gfx::hex(0x4078f2), gfx::hex(0xc18401), gfx::hex(0xe45649), gfx::hex(0x986801), gfx::hex(0x383a42)},
    // «Матрица»
    {gfx::hex(0x020a04), gfx::hex(0x010603), gfx::hex(0x062211), gfx::hex(0x22c55e), gfx::hex(0x14532d), gfx::hex(0x4ade80),
     gfx::hex(0x86efac), gfx::hex(0x4ade80), gfx::hex(0x22c55e), gfx::hex(0x16a34a), gfx::hex(0x86efac), gfx::hex(0x22c55e)},
};
static const Theme* curTheme = &THEMES[0];

static bool isKw(const char* s, int n) {
  static const char* kw[] = {"int", "return", "while", "if", "fn", "const", "self", "mut", "include", "git"};
  for (auto k : kw) {
    int i = 0;
    while (i < n && k[i] && k[i] == s[i]) i++;
    if (i == n && !k[i]) return true;
  }
  return false;
}

static void codeLine(Canvas& c, int x, int y, const char* s, int a) {
  // крошечная подсветка синтаксиса
  char tok[40];
  while (*s) {
    Color col = curTheme->txt;
    int n = 0;
    if (s[0] == '/' && s[1] == '/') {
      while (s[n]) n++;
      col = curTheme->cmt;
    } else if (*s == '"') {
      n = 1;
      while (s[n] && s[n] != '"') n++;
      if (s[n]) n++;
      col = curTheme->str;
    } else if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || *s == '_' || *s == '#') {
      while ((s[n] >= 'a' && s[n] <= 'z') || (s[n] >= 'A' && s[n] <= 'Z') || s[n] == '_' || s[n] == '#' ||
             (n > 0 && s[n] >= '0' && s[n] <= '9'))
        n++;
      const char* w = s[0] == '#' ? s + 1 : s;
      int wn = s[0] == '#' ? n - 1 : n;
      if (isKw(w, wn)) col = curTheme->kw;
      else if (s[n] == '(') col = curTheme->fn;
      else if (*s >= 'A' && *s <= 'Z') col = curTheme->typ;
      else col = curTheme->var;
    } else if (*s >= '0' && *s <= '9') {
      while (s[n] >= '0' && s[n] <= '9') n++;
      col = curTheme->num;
    } else {
      n = 1;
      if ((uint8_t)*s >= 0x80) while ((uint8_t)s[n] >= 0x80 && ((uint8_t)s[n] & 0xC0) == 0x80) n++;
      col = (*s == '{' || *s == '}' || *s == '(' || *s == ')') ? curTheme->brace : curTheme->txt;
    }
    if (n > 39) n = 39;
    memcpy(tok, s, n);
    tok[n] = 0;
    x += c.text(font_mono, x, y, tok, col, a);
    s += n;
  }
}

struct DayLight { int from; Color sky, horizon, wall, floor, beam; uint8_t light; };
// Один набор цветов для окна мастерской и неба на прогулке.
static constexpr DayLight LIGHT[] = {
    {0,  hex(0x101932), hex(0x243958), hex(0x141d38), hex(0x263554), hex(0x598cce), 52},
    {5,  hex(0x575083), hex(0xf1a082), hex(0x292540), hex(0x3d3555), hex(0xf3a38b), 72},
    {8,  hex(0x438abf), hex(0xd7e3eb), hex(0x3a4967), hex(0x495775), hex(0xffdc9f), 90},
    {11, hex(0x439fd1), hex(0xb9dded), hex(0x50607b), hex(0x62738c), hex(0xffe4bb), 45},
    {17, hex(0x664979), hex(0xf58263), hex(0x362941), hex(0x49374d), hex(0xff9974), 112},
    {19, hex(0x26244f), hex(0x845b86), hex(0x22233f), hex(0x323756), hex(0xa377c1), 45},
    {21, hex(0x101932), hex(0x243958), hex(0x141d38), hex(0x263554), hex(0x598cce), 52},
};

static int lightIndex(int hour) {
  int i = (int)(sizeof LIGHT / sizeof LIGHT[0]) - 1;
  while (hour < LIGHT[i].from) --i;
  return i;
}
static const DayLight& dayLight(int hour) { return LIGHT[lightIndex(hour)]; }
// картинка комнаты и крыши: ночь, рассвет, утро, день, закат, вечер (последняя строка LIGHT — снова ночь)
static int sceneIndex(int hour) { int i = lightIndex(hour); return i == 6 ? 0 : i; }

// Комната текущей формы: карта пикселей распаковывается один раз при смене комнаты,
// окно — при смене комнаты или времени суток. Палитра выбирает время суток.
static uint8_t roomBuf[artdata::SCENE_W * artdata::SCENE_H];
static uint8_t winBuf[artdata::SCENE_W * 60];
static int roomCached = -1, winCached = -1;
static const rooms::Room* room = nullptr;
static int roomTime = 0;
static int roomDim = 0;
static constexpr Color NIGHT_TINT = hex(0x070c1e);

static const rooms::Room& useRoom(int r, int t) {
  const rooms::Room& R = rooms::ROOM[r];
  if (roomCached != r) {
    if (rooms::unpack(R.px, R.pxLen, roomBuf, sizeof roomBuf) != (int)sizeof roomBuf) memset(roomBuf, 0, sizeof roomBuf);
    roomCached = r;
  }
  int wk = r * 8 + t, wn = (R.win[2] - R.win[0]) * (R.win[3] - R.win[1]);
  if (winCached != wk) {
    if (wn > (int)sizeof winBuf || rooms::unpack(R.winPx[t], R.winLen[t], winBuf, sizeof winBuf) != wn) memset(winBuf, 0, sizeof winBuf);
    winCached = wk;
  }
  room = &R;
  roomTime = t;
  return R;
}

// Строки комнаты [y0, y1) для столбцов [x0, x1); top — свой верх для каждого столбца (одеяло).
static void sceneRows(Canvas& c, int x0, int x1, int y0, int y1, const uint8_t* top = nullptr) {
  if (!room) return;
  const uint16_t* pal = room->pal + roomTime * 256;
  const uint16_t* wpal = room->winPal[roomTime];
  int wx0 = room->win[0], wy0 = room->win[1], wx1 = room->win[2], wy1 = room->win[3];
  for (int x = x0; x < x1; x++) {
    int from = y0;
    if (top) {
      if (top[x - x0] == 255) continue;
      from = top[x - x0];
    }
    for (int y = from; y < y1; y++) {
      Color col = x >= wx0 && x < wx1 && y >= wy0 && y < wy1 ? wpal[winBuf[(y - wy0) * (wx1 - wx0) + x - wx0]]
                                                             : pal[roomBuf[y * artdata::SCENE_W + x]];
      c.pixel(x, y + TOP + ROOM_SHIFT_Y, roomDim ? blend(col, NIGHT_TINT, roomDim) : col);
    }
  }
}

void Game::drawBackground(Canvas& c, bool dark) {
  int th = s.theme >= IT_TH_DRACULA && s.theme <= IT_TH_MATRIX ? s.theme - IT_TH_DRACULA + 1 : 0;
  curTheme = &THEMES[th];
  const Theme& T = *curTheme;
  int k = sceneIndex(hourNow());
  const rooms::Room& R = useRoom(s.form < GHOST ? s.form : JUNIOR, k);  // у каждой формы своя комната
  roomDim = dark && k != 0 ? 105 : (dark ? 60 : 0);  // свет выключен — комната в полумраке
  sceneRows(c, 0, W, 0, artdata::SCENE_H);
  // Дотягиваем верхнюю полосу стены и низ пола до границ кадра.
  // Оригинальные 87 строк не масштабируются: предметы, окна и кровати
  // сохраняют пропорции, а линия пола теперь совпадает с ногами Бита.
  const uint16_t* roomPalette = R.pal + roomTime * 256;
  for (int x = 0; x < W; ++x) {
    Color wall = roomPalette[roomBuf[x]];
    Color floor = roomPalette[roomBuf[(artdata::SCENE_H - 1) * W + x]];
    if (roomDim) {
      wall = blend(wall, NIGHT_TINT, roomDim);
      floor = blend(floor, NIGHT_TINT, roomDim);
    }
    for (int y = TOP; y < TOP + ROOM_SHIFT_Y; ++y) c.pixel(x, y, wall);
    for (int y = TOP + ROOM_SHIFT_Y + artdata::SCENE_H; y < H; ++y) c.pixel(x, y, floor);
  }
  // мониторы горят, пока питомец не спит и свет включён: строки кода цветами выбранной темы,
  // сами сменяются раз в несколько секунд, а когда питомец за компьютером — набираются быстро
  if (dark || s.asleep) return;
  bool typing = now < deskUntil;
  const Color cols[4] = {T.kw, T.str, T.fn, T.cmt};
  for (int m = 0; m < R.screens; m++) {
    const uint8_t* sc = R.screen[m];
    int x = sc[0] + 2, y = sc[1] + TOP + ROOM_SHIFT_Y + 2, sw = sc[2] - sc[0] - 4, sh = sc[3] - sc[1] - 3;
    if (sw < 4 || sh < 3) continue;
    c.fillRect(x - 1, y - 1, sw + 2, sh + 2, T.str, 14);
    int lines = sh / 3;
    uint32_t page = now / (typing ? 1800 : 4000) + m * 7;
    int row = (int)((now / (typing ? 90 : 260)) % (uint32_t)(sw + 6));  // набрано в последней строке
    for (int i = 0; i < lines; i++) {
      uint32_t h = (uint32_t)(page * 8 + i + 1) * 2654435761u;
      int ind = (int)((h >> 20) % 3) * 2, w = sw / 3 + (int)((h >> 24) % (uint32_t)(sw - sw / 3 + 1));
      if (ind + w > sw) w = sw - ind;
      int len = i == lines - 1 ? (row < w ? row : w) : w;
      int split = 2 + (int)((h >> 12) % 4);  // ключевое слово и остальное — двумя цветами
      if (split > len) split = len;
      c.fillRect(x + ind, y + i * 3, split, 2, cols[(h >> 16) % 4]);
      if (len - split - 1 > 0) c.fillRect(x + ind + split + 1, y + i * 3, len - split - 1, 2, cols[(h >> 8) % 4]);
      if (i == lines - 1 && (now / 400) & 1 && ind + len + 3 <= sw) c.fillRect(x + ind + len + 1, y + i * 3, 2, 2, T.txt);
    }
  }
}

// что просит питомец и какой пункт меню это решает
static const char* callText(uint8_t k) {
  switch (k) {
    case C_HUNGER: return "Хочет есть → Еда";
    case C_HAPPY: return "Скучает → Игры";
    case C_LIGHT: return "Выключи свет → Свет";
    case C_SICK: return "Болеет → Отладчик";
    case C_BUGS: return "Баги на полу → Уборка";
    case C_FAKE: return "Капризничает → Код-ревью";
    default: return "";
  }
}
static int callIcon(uint8_t k) {
  switch (k) {
    case C_HUNGER: return I_FOOD;
    case C_HAPPY: return I_GAME;
    case C_LIGHT: return I_LIGHT;
    case C_SICK: return I_HEAL;
    case C_BUGS: return I_CLEAN;
    case C_FAKE: return I_REVIEW;
    default: return -1;
  }
}

void Game::drawTopBar(Canvas& c, bool vitals, bool showTime) {
  c.fillRect(0, 0, W, TOP, pal::gutter);
  c.hline(0, TOP - 1, W, pal::line);
  drawBattery(c, W - 21, 7, batPct, batCharging);
  // На домашнем экране время скрыто: четыре показателя стали заметно шире.
  int left = W - 27;
  if (showTime) {
    char tm[8];
    sfmt(tm, sizeof tm, "%02d:%02d", hourNow(), minNow());
    int cx = W - 24;
    c.text(font_ui, cx, 2, tm, !s.clockSource ? pal::dim : (clockSure ? pal::white : pal::yellow), 255, RIGHT);
    left = cx - Canvas::width(font_ui, tm) - 6;
    c.vline(left, 4, 14, pal::line);
  }
  if (!vitals || s.form == EGG || !s.alive) {
    Color fc = s.form == EGG ? pal::yellow : formColor((Form)s.form);
    c.circle(10, 11, 4, fc);
    c.text(font_ui, 19, 2, formName((Form)s.form), pal::fg);
    if (wantsGps()) {
      Color gc = !fix.alive ? pal::red : (fix.posValid ? pal::green : pal::yellow);
      char n[6];
      sfmt(n, 6, "%d", fix.alive ? fix.inView : 0);
      int nw = Canvas::width(font_ui, n);
      c.text(font_ui, left - 2, 2, n, gc, 255, RIGHT);
      float gx = (float)left - nw - 12;
      c.ring(gx, 11, 4, 1.4f, gc);
      c.circle(gx, 11, 1.4f, gc);
      if (!fix.posValid && (now / 400) & 1) c.ring(gx, 11, 7, 1.1f, gc, 120);
    }
    return;
  }
  // четыре шкалы: сытость, настроение, энергия, дисциплина
  int vals[4] = {s.hunger, s.happy, s.energy, s.discipline};
  Color cols[4] = {pal::orange, pal::pink, pal::yellow, pal::blue};
  bool alert[4] = {s.callKind == C_HUNGER, s.callKind == C_HAPPY, s.energy < 15 && !s.asleep, s.callKind == C_FAKE};
  int cw = (left - 2) / 4;
  for (int i = 0; i < 4; i++) {
    int x = 2 + i * cw;
    int v = clampi(vals[i], 0, 100);
    Color fc = alert[i] || v < 25 ? pal::red : cols[i];
    if (alert[i]) c.roundRect(x, 1, cw - 2, TOP - 3, 4, pal::red, 42);
    switch (i) {
      case 0: drawIcon(c, I_FOOD, x + 9, 8.5f, cols[i], now, 0.68f); break;
      case 1: drawHeart(c, x + 9, 8.5f, 1.2f, cols[i]); break;
      case 2: drawBolt(c, x + 9, 8.5f, 1.15f, cols[i]); break;
      case 3: drawIcon(c, I_REVIEW, x + 9, 8.5f, cols[i], now, 0.65f); break;
    }
    char n[6];
    sfmt(n, sizeof n, "%d%%", v);
    c.text(font_mono, x + cw - 5, 2, n, alert[i] ? pal::red : pal::white, 255, RIGHT);
    int bw = cw - 10;
    c.fillRect(x + 4, 15, bw, 3, pal::line);
    c.fillRect(x + 4, 15, (bw * v + 50) / 100, 3, fc);
  }
}

static const char* ICON_NAMES[] = {"Еда", "Игры", "Свет", "Уборка", "Отладчик", "Статус", "Код-ревью", "Прогулка", "Магазин", "Настройки"};

void Game::drawMenuBar(Canvas& c) {
  if (scene != S_HOME) return;  // в списках/играх оставляем собственное управление
  int offset = menuOffset();
  if (offset >= H - MENU_Y) return; // полностью за нижней границей
  int yTop = MENU_Y + offset;
  c.fillRect(0, yTop, W, H - yTop, pal::gutter);
  c.hline(0, yTop, W, pal::line);
  const float step = (float)W / ICON_COUNT;
  int need = attention() ? callIcon(s.callKind) : -1;
  for (int i = 0; i < ICON_COUNT; i++) {
    float x = step * (i + 0.5f), y = yTop + 13.0f;
    bool sel = i == cursor && menuOpen;
    if (sel) {
      c.roundRect(x - 10.5f, y - 10, 21, 20, 5, pal::line);
      c.hline((int)x - 5, H - 2 + offset, 10, pal::cyan);
    }
    Color col = sel ? pal::cyan : (i == need ? pal::white : pal::fg);
    drawIcon(c, (Icon)i, x, y, col, now, 1.05f);
    if (i == need) {
      c.circle(x + 8, y - 7, 3.5f, pal::gutter);
      c.circle(x + 8, y - 7, 2.2f, pal::red);
    }
  }
  // Подпись появляется, только когда значки почти доехали до своего места.
  if (cursor >= 0 && menuOpen && offset <= 4) {
    float x = step * (cursor + 0.5f);
    const char* n = cursor == I_REVIEW && s.callKind != C_FAKE ? "Обучение" : ICON_NAMES[cursor];
    int w = Canvas::width(font_ui, n) + 16;
    float bx = clampf(x - w / 2.0f, 2, W - w - 2.0f);
    c.roundRect(bx + 1, yTop - 24, (float)w, 20, 6, pal::ink, 120);
    c.roundRect(bx, yTop - 25, (float)w, 20, 6, pal::white);
    c.triangle(x - 5, yTop - 6, x + 5, yTop - 6, x, yTop - 1, pal::white);
    c.text(font_ui, (int)(bx + w / 2.0f), yTop - 23, n, pal::editor, 255, CENTER);
  }
}

Pose Game::basePose() {
  Pose p;
  p.t = now;
  p.x = petX;
  p.y = FLOOR;
  p.dir = petDir;
  p.squash = 1 + fsin(now * 0.0032f) * 0.025f;  // дыхание
  p.lookX = lookX;
  p.lookY = lookY;
  if (now < blinkUntil) p.blink = 1;
  int base = baseWeight((Form)s.form);
  p.fat = clampf((s.weight - base) / (base + 10.0f), 0, 1);
  float lf = lifeFrac();
  p.gray = clampf((lf - 0.55f) / 0.3f, 0, 1);
  p.cane = lf > 0.8f;
  p.hat = itemIsHat(s.hat) ? s.hat : (holiday() == HD_NEWYEAR ? 20 : 0);
  p.acc = (uint8_t)((has(ACH_KM1) ? ACC_SCARF : 0) | (has(ACH_WEEK) ? ACC_MEDAL : 0) |
                    (has(ACH_GAMER) ? ACC_STAR : 0) | (has(ACH_COLLECTOR) ? ACC_CROWN : 0));
  // настроение
  if (s.sick) { p.eyes = E_DIZZY; p.mouth = M_WAVY; p.sweat = true; }
  else if (s.asleep) { p.eyes = E_CLOSED; p.mouth = M_O; p.squash = 0.93f + fsin(now * 0.0015f) * 0.04f; }
  else if (s.energy < 15) { p.blink = 0.55f; p.mouth = M_O; }
  else if (s.hunger < 20 || s.happy < 20) { p.eyes = E_SAD; p.mouth = M_SAD; }
  else if (s.happy > 80) { p.mouth = M_CAT; p.blush = true; }
  return p;
}

// Джун с кофе — картинка утра, с пиццей — заката: свет на них свой, поэтому только в эти часы.
bool Game::artMeal(uint32_t at) {
  if (actionKind != A_EAT || s.form != JUNIOR || at < 500 || itemIsHat(s.hat) || holiday() == HD_NEWYEAR) return false;
  int k = sceneIndex(hourNow());
  return (actionArg == 0 && (k == 2 || k == 3)) || (actionArg == 1 && k == 4);
}

void Game::drawPetHome(Canvas& c) {
  uint32_t& last = lastPetMs;
  float dt = (now - last) * 0.001f;
  if (dt > 0.1f) dt = 0.1f;
  last = now;
  bool still = s.asleep || actionKind != A_NONE || s.sick;
  bool hobby = hobbyKind && now < hobbyUntil && !still && scene == S_HOME && !attention();
  // Вечером, как на картинке, садится за компьютер; просьба, еда или сон — встаёт.
  const float DESK_PET_X = room ? room->desk : 0;  // где стоять у стола в этой комнате (0 — стола нет)
  bool evening = sceneIndex(hourNow()) == 5 && !s.lightOff && DESK_PET_X > 0;
  if (now < deskUntil && (still || attention() || !evening)) { deskUntil = 0; nextWander = now; }
  if (hobby) deskUntil = 0;
  bool working = now < deskUntil;
  if (!still && !working && evening && !attention() && now >= nextDesk && petTarget != DESK_PET_X) {
    petTarget = DESK_PET_X;
    nextWander = now + 60000;
  }
  if (!still && !working && evening && petTarget == DESK_PET_X && fabs_(petX - DESK_PET_X) < 1.5f && now >= nextDesk) {
    deskUntil = now + 15000 + rndInt(15000);
    nextDesk = deskUntil + 40000 + rndInt(40000);
    nextWander = deskUntil;
    working = true;
  }
  // блуждание
  if (!still && !working && !hobby && now >= nextWander) {
    petTarget = 45 + rndInt(140);
    nextWander = now + 2500 + rndInt(5000);
  }
  if (hobby) petTarget = petX;
  float d = petTarget - petX;
  bool walkingNow = !still && (d > 1.5f || d < -1.5f);
  if (walkingNow) {
    float v = s.energy < 20 ? 14 : 26;
    petX += (d > 0 ? 1 : -1) * v * dt;
    petDir = d > 0 ? 1 : -1;
    walkPh += dt * 9;
  }
  if (now >= nextBlink) { blinkUntil = now + 130; nextBlink = now + 1800 + rndInt(3500); }
  if (now >= nextLook) {
    lookX = walkingNow ? (float)petDir : (rndInt(3) - 1) * 0.8f;
    lookY = rndInt(4) == 0 ? -0.6f : 0;
    nextLook = now + 900 + rndInt(2000);
  }
  if (cursor >= 0) { lookX = (cursor * (float)W / ICON_COUNT + 12 - petX) / 60; lookY = 1; }
  Pose p = basePose();
  if (walkingNow) {
    p.walk = walkPh;
    p.lift = fabs_(fsin(walkPh)) * 2.2f;
    p.lookX = (float)petDir;
  }
  if (jumpAt && now - jumpAt < 900) {  // радость
    float t = (now - jumpAt) / 900.0f;
    p.lift = fsin(t * 3.1416f) * 14;
    p.squash = t < 0.15f ? 0.8f : 1.08f;
    p.arm = 1;
    p.eyes = E_HAPPY;
    p.mouth = M_GRIN;
  }
  if (attention() && !s.asleep && actionKind == A_NONE) {  // зовёт: машет рукой
    p.arm = (fsin(now * 0.012f) + 1) * 0.5f;
    if (s.callKind == C_FAKE) { p.mouth = M_OPEN; p.eyes = E_ANGRY; }
  }
  // Без дела не стоит столбом: оглядывается, прыгает, машет, потягивается, танцует, боксирует, крутится.
  enum { T_NONE, T_TURN, T_HOP, T_WAVE, T_STRETCH, T_DANCE, T_BOX, T_SPIN };
  static const uint16_t TRICK_MS[] = {0, 900, 1100, 1800, 1900, 2600, 1500, 1200};
  bool idle = !still && !working && !walkingNow && actionKind == A_NONE && !attention() && cursor < 0 &&
              !(jumpAt && now - jumpAt < 900);
  if (!idle || hobby) trick = T_NONE;
  else if (trick && now - trickAt > TRICK_MS[trick]) { trick = T_NONE; nextTrick = now + 1500 + rndInt(3500); }
  else if (!trick && now >= nextTrick) {
    int r = rndInt(100);
    if (s.energy < 40) trick = r < 45 ? T_STRETCH : (r < 75 ? T_TURN : T_WAVE);
    else if (s.happy > 70) trick = r < 25 ? T_DANCE : r < 45 ? T_HOP : r < 60 ? T_SPIN : r < 75 ? T_WAVE : r < 88 ? T_BOX : T_TURN;
    else trick = r < 25 ? T_TURN : r < 45 ? T_WAVE : r < 60 ? T_HOP : r < 75 ? T_BOX : r < 88 ? T_STRETCH : T_SPIN;
    trickAt = now;
    if (trick == T_TURN) petDir = -petDir;
  }
  if (trick) {
    float tt = (now - trickAt) / (float)TRICK_MS[trick];  // 0..1
    switch (trick) {
      case T_TURN:
        p.dir = petDir; p.lookX = (float)petDir;
        break;
      case T_HOP: {  // два прыжка
        float ph = tt * 2;
        p.lift = fabs_(fsin(ph * 3.1416f)) * 11;
        p.squash = p.lift < 1 ? 0.88f : 1.06f;
        p.arm = 0.5f;
        p.eyes = E_HAPPY;
        break;
      }
      case T_WAVE:  // машет рукой
        p.armR = 15 + fsin(now * 0.025f) * 4;
        p.mouth = M_CAT;
        break;
      case T_STRETCH: {  // присел — потянулся с зевком
        float k = fsin(tt * 3.1416f);
        p.squash = tt < 0.25f ? 1 - tt * 0.5f : 0.88f + k * 0.2f;
        p.arm = k;
        p.eyes = E_CLOSED;
        p.mouth = s.energy < 40 ? M_OPEN : M_O;
        break;
      }
      case T_DANCE:  // пританцовывает, разворачиваясь в такт
        p.lift = fabs_(fsin(now * 0.016f)) * 6;
        p.squash = 1 + fsin(now * 0.032f) * 0.05f;
        p.dir = ((now - trickAt) / 400) & 1 ? 1 : -1;
        p.arm = 0.6f;
        p.eyes = E_HAPPY; p.mouth = M_GRIN;
        break;
      case T_BOX:  // боксирует перед собой
        p.armR = ((now / 120) & 1) ? 9 : 1;
        p.arm = ((now / 120) & 1) ? 0.1f : 0.5f;
        p.shake = 0.6f;
        p.eyes = E_ANGRY; p.mouth = M_FLAT;
        break;
      case T_SPIN:  // крутится на месте и подпрыгивает
        p.dir = ((now - trickAt) / 150) & 1 ? 1 : -1;
        p.lift = fsin(tt * 3.1416f) * 13;
        p.squash = 1 + fsin(tt * 3.1416f) * 0.06f;
        p.eyes = E_HAPPY; p.mouth = M_O;
        break;
    }
  }
  if (hobby) {
    float pulse = fsin(now * 0.009f);
    switch (hobbyKind) {
      case 1: // размышляет, читает
        p.eyes = E_CLOSED; p.mouth = M_SMILE; p.lookY = -0.6f;
        p.arm = 0.2f + 0.2f * pulse; break;
      case 2: // придумывает проект
        p.eyes = E_WIDE; p.mouth = M_SMILE;
        p.armR = 6 + pulse * 3; p.lookX = -1; break;
      case 3: // потягивается
        p.eyes = E_CLOSED; p.mouth = M_OPEN;
        p.arm = 0.7f; p.squash = 0.95f + pulse * 0.06f; break;
      case 4: // танцует
        p.eyes = E_HAPPY; p.mouth = M_GRIN;
        p.arm = 0.7f; p.lift = fabs_(pulse) * 5;
        p.squash = 1 + pulse * 0.05f; break;
    }
  }
  uint32_t at = now - actionStart;
  if (working) {  // печатает: кулаки по очереди стучат по клавиатуре
    p.dir = -1; p.lookX = -1; p.lookY = 0.3f; p.arm = 0.3f;
  }
  switch (actionKind) {
    case A_EAT:
      if (at > 600) { p.mouth = M_CHEW; p.lookX = (float)petDir; p.eyes = at > 2200 ? E_HAPPY : E_NORMAL; }
      else p.mouth = M_OPEN;
      if (actionArg == 0) {  // кофе — кружка в руке, подносит ко рту
        p.hold = &artdata::MUG;
        p.holdLift = clampf(at / 400.0f, 0, 1) * (5 + (at > 600 && at < 2200 ? 3 * fsin(at * 0.008f) : 0));
        p.mouth = at > 600 && at < 2200 ? M_O : M_SMILE;
      }
      break;
    case A_REFUSE:
      p.shake = 3; p.mouth = M_FLAT; p.eyes = E_ANGRY; p.lookX = fsin(now * 0.03f);
      break;
    case A_REVIEW:
      if (s.fakeCall) { p.sweat = true; p.eyes = E_SAD; p.mouth = M_FLAT; }
      else { p.eyes = E_ANGRY; p.mouth = M_SAD; }
      p.lookX = -1;
      break;
    case A_HEAL:
      p.glow = at > 700 && at < 1700 ? fsin((at - 700) * 0.00314f) : 0;
      p.eyes = at > 1700 ? E_HAPPY : E_DIZZY;
      break;
    case A_NOHEAL: case A_PRAISE:
      p.lift = fsin(clampf(at / 700.0f, 0, 1) * 3.1416f) * 10; p.eyes = E_HAPPY; p.mouth = M_GRIN; p.arm = 1;
      break;
    case A_POOP:
      p.eyes = E_WIDE; p.mouth = M_O; p.lookX = 1;
      break;
    default: break;
  }
  float ph = formHeight((Form)s.form);
  float headY = FLOOR - ph - p.lift - 6;
  if (s.asleep && s.form != EGG && s.form != GHOST) {
    const float BED_X = room ? room->bed : 196;  // где спит в этой комнате
    petX = BED_X; petTarget = BED_X;
    bool night = sceneIndex(hourNow()) == 0 || sceneIndex(hourNow()) == 5;
    if (s.form == JUNIOR) {
      // Джун спит ровно как на картинке: под одеялом, кепка с пропеллером на полу.
      // Если на нём колпак или купленная шапка — на полу лежит она.
      auto art = [&](const artdata::Sprite& z, int zx, int zy) {
        for (int y = 0; y < z.h; y++)
          for (int x = 0; x < z.w; x++) {
            uint8_t k = z.px[y * z.w + x];
            if (k) c.pixel(zx + x, zy + y, roomDim ? blend(z.pal[k], NIGHT_TINT, roomDim) : z.pal[k]);
          }
      };
      art(night ? artdata::SLEEP_NIGHT : artdata::SLEEP_DAWN, night ? artdata::SLEEP_NIGHT_X : artdata::SLEEP_DAWN_X,
          (night ? artdata::SLEEP_NIGHT_Y : artdata::SLEEP_DAWN_Y) + TOP + ROOM_SHIFT_Y);
      const artdata::Sprite& cap = night ? artdata::CAP_NIGHT : artdata::CAP_DAWN;
      int capX = night ? artdata::CAP_NIGHT_X : artdata::CAP_DAWN_X, capY = (night ? artdata::CAP_NIGHT_Y : artdata::CAP_DAWN_Y) + TOP + ROOM_SHIFT_Y;
      if (p.hat) drawHat(c, p.hat, capX + cap.w * 0.5f, FLOOR - 4, 20, 0.7f, 1, now);
      else art(cap, capX, capY);
      headY = FLOOR - 52;
      petX = BED_X - 4;
    } else {
      // Остальные — на подушке, укрытые одеялом с картинки; глаза закрыты.
      const artdata::Sprite& sp = artdata::FORM[s.form];
      float sc = 0.62f;
      float eyeLow = sp.eye[0][3] > sp.eye[1][3] ? sp.eye[0][3] : sp.eye[1][3];
      p.x = BED_X - 6; p.scale = sc; p.lift = 0; p.walk = 0; p.arm = 0; p.squash = 1;
      int bi = clampi((int)p.x - (room ? room->blanketX0 : 0), 0, room && room->blanketN ? room->blanketN - 1 : 0);
      int bt = room && room->blanketN && room->blanket[bi] != 255 ? room->blanket[bi] : 52;
      p.y = TOP + ROOM_SHIFT_Y + bt - 1 + (sp.h - eyeLow - 2) * sc;  // глаза чуть выше края одеяла
      p.lookX = 0; p.lookY = 0; p.sleeping = true; p.cane = false; p.dir = 1;
      p.eyes = E_CLOSED; p.mouth = s.sick ? M_WAVY : M_SMILE;
      if (p.hat) drawHat(c, p.hat, BED_X - 43, FLOOR - 4, 20, 0.7f, 1, now);
      drawPet(c, (Form)s.form, p);
      if (room && room->blanketN) sceneRows(c, room->blanketX0, room->blanketX0 + room->blanketN, 0, 76, room->blanket);
      headY = p.y - (sp.h - sp.head) * sc - 3;
      petX = p.x;  // «z» вылетают от головы
    }
  } else if (working && s.form == JUNIOR && !p.hat) {
    // за компьютером на стуле — кусок вечерней картинки как есть
    drawArt(c, artdata::DESK, artdata::DESK_X, artdata::DESK_Y + TOP + ROOM_SHIFT_Y, 255);
    headY = FLOOR - 50;
  } else if (artMeal(at)) {
    // с кружкой кофе (утро) или с куском пиццы у коробки (закат) — рисунок целиком
    bool coffee = actionArg == 0;
    const artdata::Sprite& z = coffee ? artdata::COFFEE : artdata::PIZZA;
    int dx = coffee ? artdata::COFFEE_DX : artdata::PIZZA_DX;
    int top = (coffee ? artdata::COFFEE_Y : artdata::PIZZA_Y) + TOP + ROOM_SHIFT_Y;
    float ex = clampf(petX, (float)dx, (float)(W - z.w + dx));
    c.ellipse(ex, FLOOR + 1, 20, 3, pal::ink, 70);
    drawArt(c, z, (int)ex - dx, top + (at < 700 ? (int)((700 - at) / 120) : 0));
  } else {
    drawPet(c, (Form)s.form, p);
  }
  float side = petX + ph * 0.5f + 8;          // место для облачка справа от головы
  bool right = side + 30 < W;
  float bx = right ? side : petX - ph * 0.5f - 38;
  float by = clampf(headY + 2, TOP + 4, FLOOR - 40);
  // просьба: облачко со значком того, что нужно
  bool asks = attention() && actionKind == A_NONE && (!s.asleep || s.callKind == C_LIGHT);
  if (asks) {
    float bob = fsin(now * 0.008f) * 1.5f;
    c.roundRect(bx + 1, by + 2 + bob, 30, 26, 8, pal::ink, 110);
    c.roundRect(bx, by + bob, 30, 26, 8, pal::white);
    float tx = right ? bx + 2 : bx + 28;
    c.triangle(tx, by + 18 + bob, tx + (right ? 0 : 0), by + 25 + bob, right ? bx - 5 : bx + 35, by + 24 + bob, pal::white);
    float ix = bx + 15, iy = by + 13 + bob;
    switch (s.callKind) {
      case C_HUNGER: drawPizza(c, ix, iy + 5, 0); break;
      case C_BUGS: drawBug(c, ix, iy + 5, now * 0.03f, 1, 1.0f); break;
      case C_FAKE: c.text(font_big, (int)ix, (int)iy - 11, "?", pal::blue, 255, CENTER); break;
      default: {
        int ic = callIcon(s.callKind);
        if (ic >= 0) drawIcon(c, (Icon)ic, ix, iy, s.callKind == C_SICK ? pal::red : (s.callKind == C_LIGHT ? pal::orange : pal::accent), now, 1.25f);
      }
    }
  } else if (!s.asleep && actionKind == A_NONE) {
    if (s.hunger < 25 && (now / 3000) % 2 == 0) {  // думает о еде
      c.circle(right ? bx - 3 : bx + 33, by + 26, 2, pal::white, 200);
      c.ellipse(bx + 15, by + 12, 16, 12, pal::white, 230);
      if (s.favFood == 0) drawCoffee(c, bx + 15, by + 19, 1, now);
      else if (s.favFood == 1) drawPizza(c, bx + 15, by + 17, 0);
      else drawCookie(c, bx + 15, by + 20, 0);
    } else if (s.happy < 25) {  // дождевая туча
      float cx = petX, cy = clampf(headY - 10, TOP + 9, FLOOR);
      drawCloud(c, cx, cy, 1.2f, hex(0x7a8396));
      for (int i = 0; i < 4; i++) {
        float q = ((now + i * 260) % 1000) / 1000.0f;
        c.line(cx - 8 + i * 5, cy + 6 + q * 14, cx - 9 + i * 5, cy + 9 + q * 14, 1.3f, pal::blue, (int)(220 * (1 - q)));
      }
    }
  }
  if (s.asleep && (now / 900) != (lastRender / 900)) {
    spawn(3, petX + 14, headY + 8, 6, -10, 2.4f, hex(0xc9c2ff), (now / 900) % 2 ? "z" : "Z");
  }
  if (s.sick && !s.asleep) {  // ярлык ошибки слева от головы
    float bob = fsin(now * 0.006f) * 1.5f;
    int w = Canvas::width(font_small, "SIGSEGV") + 10;
    float lx = right ? petX - ph * 0.5f - 6 - w : petX + ph * 0.5f + 6;
    if (lx < 22) lx = 22;
    c.roundRect(lx, headY + 4 + bob, (float)w, 16, 4, pal::red);
    c.text(font_small, (int)(lx + w / 2.0f), (int)(headY + 5 + bob), "SIGSEGV", pal::white, 255, CENTER);
  }
  if (s.sick && (now % 2400) < 300) {  // глитч
    for (int k = 0; k < 6; k++) c.shiftRow(FLOOR - 46 + rndInt(48), rndInt(9) - 4);
  }
}

void Game::drawAction(Canvas& c) {
  uint32_t at = now - actionStart;
  float fx = petX + petDir * 32;
  switch (actionKind) {
    case A_EAT: {
      if (artMeal(at)) break;  // Джун ест как на картинке — вместе с едой
      float y = TOP + 2 + (FLOOR - 4 - TOP - 2) * (at < 500 ? bounce(at / 500.0f) : 1);
      int bites = at < 700 ? 0 : (int)((at - 700) / 500);
      if (bites > 3) bites = 3;
      if (at < 2300) {
        if (actionArg == 0) {}  // кружку держит в руке — рисуется вместе с питомцем
        else if (actionArg == 1) drawArt(c, artdata::BOX, (int)fx - artdata::BOX.w / 2, (int)y + 5 - artdata::BOX.h);
        else {
          c.roundRect(fx - 15, FLOOR - 4, 30, 7, 2, pal::ink);
          c.hline((int)fx - 12, FLOOR - 4, 24, pal::orange);
          drawCookie(c, fx, y, bites);
        }
      }
      if (at > 700 && at < 2200 && (at / 120) % 4 == 0)
        spawn(4, fx, FLOOR - 10, (rndInt(40) - 20), -30, 0.6f, actionArg == 0 ? pal::orange : pal::yellow);
      break;
    }
    case A_CLEAN: {
      float bx = -20 + ease(at / 1500.0f) * 280;
      c.gradientV((int)bx - 6, TOP, 12, H - TOP, hex(0x9ff0b0), pal::green);
      for (int i = 0; i < 18; i++) c.fillRect((int)bx - 18 - i * 3, TOP, 3, H - TOP, pal::green, 60 - i * 3);
      c.roundRect(60, TOP + 4, 120, 20, 6, pal::ink, 210);
      c.text(font_ui, 120, TOP + 5, "git clean -fd", pal::green, 255, CENTER);
      if ((at / 60) % 2 == 0) spawn(0, bx, FLOOR - rndInt(60), -40, -20 + rndInt(40), 0.5f, pal::green);
      break;
    }
    case A_HEAL: {
      float k = ease(at / 400.0f);
      float bx = petX - formHeight((Form)s.form) * 0.5f - 14, by = FLOOR - 36;
      c.circle(bx, by, 7 * k, pal::red);
      c.circle(bx, by, 11 * k, pal::red, 60);
      if (at > 500) {
        float ay = FLOOR - 54 + ease((at - 500) / 1200.0f) * 44;
        c.triangle(bx + 10, ay - 6, bx + 10, ay + 6, bx + 19, ay, pal::yellow);
        c.roundRect(50, TOP + 4, 140, 20, 6, pal::ink, 210);
        c.text(font_ui, 120, TOP + 5, "gdb: step over", pal::yellow, 255, CENTER);
      }
      break;
    }
    case A_REVIEW: {
      float k = elastic(clampf(at / 600.0f, 0, 1));
      float bw = 168 * k, bx = 6, by = TOP + 3;
      if (bw > 4) {
        c.roundRect(bx + 1, by + 2, bw, 38, 7, pal::ink, 110);
        c.roundRect(bx, by, bw, 38, 7, pal::white);
        c.fillRect((int)bx, (int)by + 4, 4, 30, s.fakeCall ? pal::blue : pal::red);
        if (k > 0.9f) {
          c.text(font_ui, (int)bx + 10, (int)by + 2, "Код-ревью", pal::editor);
          c.text(font_small, (int)bx + 10, (int)by + 20, s.fakeCall ? "// хватит капризничать" : "// придраться не к чему", pal::dim);
        }
      }
      break;
    }
    case A_REFUSE: {
      float x = petX + formHeight((Form)s.form) * 0.5f + 14, y = FLOOR - 54;
      c.line(x - 7, y - 7, x + 7, y + 7, 3.5f, pal::red);
      c.line(x + 7, y - 7, x - 7, y + 7, 3.5f, pal::red);
      break;
    }
    case A_POOP: {
      float k = clampf(at / 500.0f, 0, 1);
      int i = s.bugs - 1;
      static const float slots[] = {210, 186, 34, 58};
      if (i >= 0) {
        c.ring(slots[i], FLOOR - 4, 4 + k * 16, 2, pal::red, (int)(200 * (1 - k)));
        if (at < 60) burst(slots[i], FLOOR - 6, 8, pal::red);
      }
      break;
    }
    case A_LIGHT: {
      float k = at / 900.0f;
      c.tint(s.lightOff ? pal::ink : pal::white, (int)(160 * (1 - k)));
      break;
    }
    default: break;
  }
}

void Game::drawParticles(Canvas& c, uint32_t dtMs) {
  float dt = dtMs * 0.001f;
  if (dt > 0.1f) dt = 0.1f;
  for (auto& p : parts) {
    if (p.life <= 0) continue;
    p.life -= dt;
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    if (p.kind == 0 || p.kind == 4) p.vy += 120 * dt;
    if (p.kind == 3) p.vx = fsin(p.life * 3) * 8;
    float k = clampf(p.life / p.max, 0, 1);
    int a = (int)(255 * (k < 0.3f ? k / 0.3f : 1));
    switch (p.kind) {
      case 0: drawStar(c, p.x, p.y, 0.7f + k * 0.9f, p.col, a); break;
      case 1: drawHeart(c, p.x, p.y, 1.4f, p.col, a); break;
      case 2: case 3: c.text(p.kind == 3 ? font_small : font_mono, (int)p.x, (int)p.y, p.ch, p.col, a * 3 / 4, CENTER); break;
      case 4: c.circle(p.x, p.y, 1.5f, p.col, a); break;
    }
  }
}

// крупное уведомление сверху; пока его нет — подсказка, что просит питомец
void Game::drawToast(Canvas& c) {
  const char* t = nullptr;
  Color dot = 0;
  int a = 255;
  float y = TOP + 3;
  if (toast && now < toastUntil) {
    t = toast;
    dot = toastCol;
    uint32_t left = toastUntil - now;
    if (left < 300) a = (int)(left * 255 / 300);
    y -= (1 - ease(clampf((now - toastStart) / 220.0f, 0, 1))) * 8;
  } else if (scene == S_HOME && attention() && actionKind == A_NONE && cursor < 0) {
    t = callText(s.callKind);
    dot = pal::red;
    if (!*t) return;
  } else {
    return;
  }
  const Font& f = Canvas::width(font_ui, t) + 26 <= W - 4 ? font_ui : font_small;
  // не влезает и мелким шрифтом — переносим на две строки по пробелу ближе к середине
  char l1[160], l2[160];
  l2[0] = 0;
  int len = 0;
  while (t[len] && len < 159) len++;
  memcpy(l1, t, len);
  l1[len] = 0;
  if (Canvas::width(font_small, t) + 26 > W - 4) {
    int best = -1;
    for (int i = 0; i < len; i++)
      if (t[i] == ' ' && (best < 0 || (i - len / 2 < 0 ? len / 2 - i : i - len / 2) < (best - len / 2 < 0 ? len / 2 - best : best - len / 2))) best = i;
    if (best > 0) {
      memcpy(l1, t, best); l1[best] = 0;
      int r = len - best - 1;
      memcpy(l2, t + best + 1, r); l2[r] = 0;
    }
  }
  bool two = l2[0] != 0;
  int w1 = Canvas::width(two ? font_small : f, l1), w2 = two ? Canvas::width(font_small, l2) : 0;
  int w = (w1 > w2 ? w1 : w2) + 26;
  if (w > W - 4) w = W - 4;
  int h = two ? 32 : 20;
  float x = (W - w) / 2.0f;
  c.roundRect(x, y + 2, (float)w, (float)h, 10, pal::ink, a * 150 >> 8);
  c.roundRect(x, y, (float)w, (float)h, 6, pal::line, a);
  c.roundRect(x + 1, y + 1, (float)w - 2, (float)h - 2, 5, pal::gutter, a);
  c.circle(x + 10, y + 10, 3.2f, dot, a);
  if (two) {
    c.text(font_small, (int)x + 18, (int)y + 2, l1, pal::white, a);
    c.text(font_small, (int)x + 18, (int)y + 16, l2, pal::white, a);
  } else {
    c.text(f, (int)x + 18, (int)y + (&f == &font_ui ? 1 : 3), l1, pal::white, a);
  }
}

// Погода в комнате остаётся только художественным эффектом за окном;
// температура и прогноз теперь показываются на отдельном информационном экране.
void Game::drawWeather(Canvas& c, bool dark) {
  if (!hasWeather()) return;
  uint32_t t = now;
  if (!room || room->outdoor) return;
  const int x0 = room->win[0], y0 = room->win[1] + TOP + ROOM_SHIFT_Y, x1 = room->win[2], y1 = room->win[3] + TOP + ROOM_SHIFT_Y;
  const int ww = x1 - x0, wh = y1 - y0;
  int dimA = dark ? 120 : 0;
  c.clip(x0, y0, x1, y1);
  switch (weather) {
    case 2:  // облачно — тучи плывут мимо окна
      c.fillRect(x0, y0, ww, wh, hex(0x8a96ab), 70);
      for (int i = 0; i < 3; i++) {
        float x = (float)((int)(t / 120 + i * 23) % (ww + 24)) - 12 + x0;
        drawCloud(c, x, (float)(y0 + 3 + (i % 2) * 6), 0.7f, hex(0xdfe5ee), 230);
      }
      break;
    case 3: case 5:  // дождь и гроза: небо серое, косые капли
      c.fillRect(x0, y0, ww, wh, hex(0x3b4659), 170);
      for (int i = 0; i < 9; i++) {
        float x = (float)((i * 11 + t / 9) % (ww + 6)) + x0 - 3;
        float y = (float)(y0 + ((i * 7 + t / 5) % (wh + 4))) - 2;
        c.line(x, y, x - 1, y + 3, 0.8f, hex(0xa9c6ff), 200);
      }
      if (weather == 5 && (t % 7000) < 120) {  // молния в окне и отсвет в комнате
        c.fillRect(x0, y0, ww, wh, pal::white, 200);
        c.noClip();
        c.fillRect(0, TOP, W, H - TOP, pal::white, 40);
        c.clip(x0, y0, x1, y1);
      }
      break;
    case 4:  // снег
      c.fillRect(x0, y0, ww, wh, hex(0xc5d0de), 60);
      for (int i = 0; i < 10; i++) {
        uint32_t h = (uint32_t)i * 2654435761u;
        int speed = 40 + (int)(h >> 27);
        float x = (float)((h >> 8) % ww) + x0 + fsin(t * 0.0011f + i * 1.7f) * 2;
        float y = (float)(y0 + (((h >> 16) + t / speed) % (uint32_t)wh));
        c.fillRect((int)x, (int)y, 1 + (int)((h >> 4) & 1), 1, pal::white, 230);
      }
      break;
    case 6:  // туман — молочная пелена
      c.fillRect(x0, y0, ww, wh, hex(0xd8dde6), 150);
      for (int i = 0; i < 2; i++) {
        float y = (float)(y0 + 3 + i * 6) + fsin(t * 0.0007f + i) * 1.5f;
        c.fillRect(x0, (int)y, ww, 2, pal::white, 90);
      }
      break;
    default: break;
  }
  if (dimA) c.fillRect(x0, y0, ww, wh, hex(0x070c1e), dimA);
  c.noClip();
}

void Game::drawHome(Canvas& c) {
  if (infoScreen) {
    drawInfoScreen(c);
    return;
  }
  bool dark = s.lightOff;
  drawBackground(c, dark);
  static const float slots[] = {210, 186, 34, 58};
  float cleanX = actionKind == A_CLEAN ? -20 + ease((now - actionStart) / 1500.0f) * 280 : -100;
  for (int i = 0; i < s.bugs && i < 4; i++) {  // баги ползают на полу
    float x = slots[i] + fsin(now * 0.0013f + i * 2) * 5;
    if (x < cleanX) continue;
    int dir = fcos(now * 0.0013f + i * 2) > 0 ? 1 : -1;
    drawBug(c, x, FLOOR + 2, now * 0.03f + i, dir, 1.1f);
  }
  float deco = room ? room->tree : 32;  // свободный угол комнаты: мониторы и кровать не закрывает
  switch (holiday()) {  // праздник — украшение в углу
    case HD_NEWYEAR:
      drawTree(c, deco, FLOOR + 2, now);
      // Снежинки позади питомца, чтобы не закрывать лицо и шапки.
      for (int i = 0; i < 18; ++i) {
        int x = (i * 53 + (int)((now / 52) % W) + i * i * 7) % W;
        int y = TOP + 2 + (i * 19 + (int)((now / 92) % (FLOOR - TOP - 4))) % (FLOOR - TOP - 4);
        c.circle((float)x, (float)y, 0.8f, pal::white, 135);
      }
      break;
    case HD_PROGRAMMER: drawCake(c, deco, FLOOR + 2, "256", now); break;
    case HD_HALLOWEEN: drawPumpkin(c, deco, FLOOR + 2, now); break;
    case HD_BIRTHDAY: { char wk[8]; sfmt(wk, 8, "%u", s.ageMin / 10080); drawCake(c, deco, FLOOR + 2, wk, now); break; }
    default: break;
  }
  drawWeather(c, dark);  // атмосферная анимация за окном, без температурной таблички
  drawFriend(c);
  drawPetHome(c);
  if (actionKind != A_NONE && actionKind != A_LIGHT) drawAction(c);
  if (dark) c.fillRect(0, TOP, W, H - TOP, hex(0x0b1838), 70);  // ночная дымка до низа комнаты
  drawParticles(c, now - lastRender);
  if (actionKind == A_LIGHT) drawAction(c);
  drawTopBar(c, true, false);
  drawMenuBar(c);
  if (scene == S_HOME) drawToast(c);
}

// Полноэкранный информер: состояние погоды не сохраняется и приходит по BLE из Android.
// Компактные погодные пиктограммы: рисуются самим графическим ядром (240x135).
static void weatherTiny(Canvas& c, int x, int y, uint8_t kind, bool night = false) {
  const Color cloud = hex(0xc8dcf5), shade = hex(0x90b4dc);
  if (kind == 1) {
    if (night) {
      c.circle(x + 6, y + 5, 4.2f, pal::yellow);
      c.circle(x + 9, y + 3, 3.7f, hex(0x17243d));
    } else {
      c.circle(x + 6, y + 5, 4.5f, pal::yellow);
      c.line(x + 6, y - 2, x + 6, y, 1, pal::yellow);
      c.line(x - 1, y + 5, x + 1, y + 5, 1, pal::yellow);
      c.line(x + 11, y + 5, x + 13, y + 5, 1, pal::yellow);
    }
    return;
  }
  if (kind == 5) {
    c.circle(x + 6, y + 5, 5, hex(0x8296be));
    c.line(x + 5, y + 6, x + 8, y + 3, 1.5f, pal::yellow);
    c.line(x + 8, y + 3, x + 6, y + 8, 1.5f, pal::yellow);
  } else if (kind == 6) {
    c.fillRect(x, y + 2, 14, 2, shade);
    c.fillRect(x + 2, y + 6, 12, 2, cloud);
    c.fillRect(x, y + 10, 11, 1, shade);
  } else {
    c.circle(x + 5, y + 5, 3.5f, shade);
    c.circle(x + 9, y + 3.5f, 4, cloud);
    c.roundRect(x + 2, y + 5, 13, 5, 2, cloud);
    if (kind == 3) {
      c.line(x + 5, y + 11, x + 3, y + 13, 1, hex(0x70b9ff));
      c.line(x + 10, y + 11, x + 8, y + 13, 1, hex(0x70b9ff));
    }
    if (kind == 4) {
      c.circle(x + 5, y + 12, 1, pal::white);
      c.circle(x + 11, y + 12, 1, pal::white);
    }
  }
}

// Экран-информер использует только реально полученные показатели.
// Внешний вид ориентирован на выбранный плотный pixel dashboard.
void Game::drawInfoScreen(Canvas& c) {
  const Color panel = hex(0x182840), panel2 = hex(0x20334f);
  const Color line = hex(0x395373), muted = hex(0x9fb2ce);
  c.gradientV(0, 0, W, H, hex(0x0d1a31), hex(0x101b31));

  // Верхняя строка — узнаваемая шапка исходного проекта.
  weatherTiny(c, 6, 2, hasWeather() ? weather : 2);
  c.text(font_ui, 25, 1, "ЧАСЫ И ПОГОДА", pal::cyan);
  // Цветная точка показывает синхронизацию без подписи,
  // иначе верхняя строка не помещается на настоящих 240 пикселях.
  c.circle(174, 10, 2.3f, !s.clockSource ? pal::dim : (clockSure ? pal::green : pal::yellow));
  char charge[8];
  sfmt(charge, sizeof charge, "%d%%", batPct);
  c.text(font_small, 215, 4, charge, pal::white, 255, RIGHT);
  drawBattery(c, W - 19, 9, batPct, batCharging);
  c.hline(5, 19, 230, line);

  // Дата и цифровые часы, левая панель.
  c.roundRect(6, 24, 105, 53, 6, line);
  c.roundRect(7, 25, 103, 51, 5, panel);
  uint32_t days = s.epoch / 86400u;
  const int weekday = (int)((days + 5) % 7);  // 01.01.2000 была суббота
  static const char* wd[] = {"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
  int year = 2000;
  uint32_t day = days;
  while (year < 2100) {
    const int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    const int n = 365 + leap;
    if (day < (uint32_t)n) break;
    day -= n;
    year++;
  }
  static const int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int month = 0;
  for (; month < 11; ++month) {
    const int n = mdays[month] + (month == 1 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)));
    if (day < (uint32_t)n) break;
    day -= n;
  }
  char b[32];
  sfmt(b, sizeof b, "%02d.%02d.%04d", (int)day + 1, month + 1, year);
  c.text(font_small, 12, 28, b, muted);
  c.text(font_small, 106, 28, wd[weekday], pal::cyan, 255, RIGHT);
  sfmt(b, sizeof b, "%02d:%02d", hourNow(), minNow());
  c.roundRect(10, 43, 97, 30, 5, panel2);
  c.textFit(58, 46, b, !s.clockSource ? pal::dim : (clockSure ? pal::white : pal::yellow), 94);

  // Правая половина: текущая погода.
  c.roundRect(115, 24, 119, 53, 6, line);
  c.roundRect(116, 25, 117, 51, 5, panel);
  if (hasWeather()) {
    weatherTiny(c, 121, 32, weather, hourNow() >= 20 || hourNow() < 6);
    sfmt(b, sizeof b, "%s%d°", tempC > 0 ? "+" : "", (int)tempC);
    c.text(font_big, 145, 27, b, pal::white);
    const char* desc[] = {"", "Ясно", "Облачно", "Дождь", "Снег", "Гроза", "Туман"};
    c.text(font_small, 120, 53, desc[weather <= 6 ? weather : 0], pal::cyan);
    c.hline(121, 64, 107, line);
    if (hasWeatherExtra()) {
      if (humidityPct <= 100) {
        sfmt(b, sizeof b, "%d%%", (int)humidityPct);
        c.text(font_small, 120, 66, b, muted);
      }
      if (windMs < 255) {
        sfmt(b, sizeof b, "%d м/с", (int)windMs);
        c.text(font_small, 172, 66, b, muted);
      }
    } else {
      c.text(font_small, 121, 66, "Нет деталей", muted);
    }
  } else {
    c.text(font_ui, 125, 37, "Нет погоды", pal::yellow);
    c.text(font_small, 125, 55, "Нужен Android", muted);
  }

  // Нижняя область без подсказок кнопок: прогноз занимает всё место,
  // когда нет дополнительных показателей; при наличии показателей
  // оставляем снизу строку с реальными значениями.
  const bool extra = hasWeatherExtra();
  const int forecastHeight = extra ? 34 : 48;
  c.roundRect(6, 81, 228, forecastHeight, 5, line);
  c.roundRect(7, 82, 226, forecastHeight - 2, 4, panel);
  if (extra && forecastCount > 0) {
    for (int i = 0; i < forecastCount && i < 6; ++i) {
      const int x = 10 + i * 37;
      if (i) c.vline(x - 3, 85, 25, line);
      sfmt(b, sizeof b, "%02d:00", (int)forecast[i].hour);
      c.text(font_small, x + 1, 83, b, muted);
      weatherTiny(c, x + 9, 94, forecast[i].kind,
                  forecast[i].hour >= 20 || forecast[i].hour < 6);
      sfmt(b, sizeof b, "%s%d°", forecast[i].temp > 0 ? "+" : "", (int)forecast[i].temp);
      c.text(font_small, x + 33, 102, b, pal::white, 255, RIGHT);
    }
  } else {
    c.text(font_small, 13, 88, "ПРОГНОЗ ПО ЧАСАМ", pal::cyan);
    c.text(font_small, 13, 105, "Нет почасового прогноза", muted);
  }

  // Подвал отображается только с реальными погодными значениями.
  if (extra) {
    c.hline(7, 118, 226, line);
    if (feelsC > -60 && feelsC < 70) {
      sfmt(b, sizeof b, "Ощ. %s%d°", feelsC > 0 ? "+" : "", (int)feelsC);
      c.text(font_small, 10, 122, b, muted);
    }
    if (pressureMm > 0) {
      sfmt(b, sizeof b, "%d мм", (int)pressureMm);
      c.text(font_small, 89, 122, b, muted);
    }
    if (precipitationPct <= 100) {
      sfmt(b, sizeof b, "Осадки %d%%", (int)precipitationPct);
      c.text(font_small, 227, 122, b, pal::cyan, 255, RIGHT);
    }
  }
}

// --- списки ---

void Game::drawList(Canvas& c) {
  drawHome(c);
  c.dim(98);
  float k = ease(clampf((now - sceneStart) / 180.0f, 0, 1));
  float x = 6, y = 5 + (1 - k) * 5, w = 228, h = 125;
  c.roundRect(x + 1, y + 2, w, h, 8, pal::ink, 190);
  c.roundRect(x, y, w, h, 8, pal::line);
  c.roundRect(x + 1, y + 1, w - 2, h - 2, 7, pal::gutter);
  c.hline(12, (int)y + 24, W - 24, pal::line);

  static const Icon icons[] = {I_FOOD, I_GAME, I_SETTINGS, I_SHOP};
  static const char* titles[] = {"Еда", "Игры", "Настройки", "Магазин"};
  drawIcon(c, icons[listId], 19, y + 12, pal::cyan, now, 0.9f);
  c.text(font_ui, 33, (int)y + 3, titles[listId], pal::white);
  char tbuf[32];
  if (listId == 3) sfmt(tbuf, sizeof tbuf, "%u монет", s.coins);
  else sfmt(tbuf, sizeof tbuf, "%d/%d", listSel + 1, listCount);
  c.text(font_small, W - 14, (int)y + 5, tbuf, listId == 3 ? pal::yellow : pal::dim, 255, RIGHT);

  constexpr int vis = 4, rh = 19;
  int first = listSel >= vis ? listSel - vis + 1 : 0;
  for (int i = first; i < listCount && i < first + vis; i++) {
    float iy = y + 28 + (i - first) * rh;
    bool sel = i == listSel;
    if (sel) {
      c.roundRect(x + 5, iy, w - 15, rh, 5, pal::line);
      c.vline((int)x + 7, (int)iy + 4, rh - 8, pal::cyan);
    }
    char buf[64];
    const char* label = buf;
    const char* right = nullptr;
    char rbuf[16];
    if (listId == 0) label = (i == 3 && listCount == 5) ? "Угостить гостя" : FOOD_ITEMS[i == 4 ? 3 : i];
    else if (listId == 1) label = (listCount == 8 && i == 6) ? "Дуэль с гостем" : GAME_ITEMS[i == 7 ? 6 : i];
    else if (listId == 3) {
      uint8_t it = shopItem(i);
      if (i == listCount - 1) label = "← Назад";
      else if (it == IT_NONE) {
        label = i == 0 ? "Без шапки" : "Стандартная тема";
        if ((i == 0 && !s.hat) || (i != 0 && !s.theme)) right = "надето";
      } else {
        label = itemName(it);
        bool own = (s.owned >> it) & 1, on = s.hat == it || s.theme == it;
        if (on) right = "надето";
        else if (own) right = "есть";
        else { sfmt(rbuf, sizeof rbuf, "%u", itemPrice(it)); right = rbuf; }
      }
    }
    else {
      switch (i) {
        case 0: sfmt(buf, 64, "Часы: %02d:%02d", hourNow(), minNow()); break;
        case 1:
          if (s.alarmOn) sfmt(buf, 64, "Будильник: %02d:%02d", s.alarmH, s.alarmM);
          else sfmt(buf, 64, "Будильник: выкл");
          break;
        case 2: sfmt(buf, 64, "Пояс: UTC%s%d", s.tz >= 0 ? "+" : "", s.tz); break;
        case 3: sfmt(buf, 64, "Яркость: %d из 3", s.brightness); break;
        case 4: sfmt(buf, 64, "Светодиоды: %s", s.leds ? "вкл" : "выкл"); break;
        case 5: sfmt(buf, 64, "Ночник: %s", s.nightLight ? "вкл" : "выкл"); break;
        case 6: sfmt(buf, 64, "Экран: %s", s.flip ? "перевёрнут" : "обычный"); break;
        case 7: sfmt(buf, 64, "Время по GPS: %s", s.gpsAuto ? "вкл" : "выкл"); break;
        case 8: sfmt(buf, 64, "Уход: %s", s.reserved4[0] ? "классика" : "спокойный"); break;
        case 9: label = "Проверка GPS"; break;
        case 10: sfmt(buf, 64, "Bluetooth: %s", s.bleOn ? "вкл" : "выкл"); break;
        case 11: sfmt(buf, 64, "Встречи LoRa: %s", s.loraOn ? "вкл" : "выкл"); break;
        case 12: label = "Сбросить прогулки"; break;
        case 13: label = "Новый питомец"; break;
        default: label = "← Назад"; break;
      }
    }
    bool fav = (listId == 0 && i == s.favFood) || (listId == 1 && i == s.favGame);
    if (fav) drawHeart(c, x + w - 22, iy + 9, 1.4f, pal::pink);
    if (listId == 0 && i < 3) {
      drawFoodIcon(c, i, x + 18, iy + 9);
      c.text(font_ui, (int)x + 32, (int)iy + 1, label, sel ? pal::white : pal::fg);
    } else {
      c.text(font_ui, (int)x + 12, (int)iy + 1, label, sel ? pal::white : pal::fg);
    }
    if (right) c.text(font_small, (int)(x + w - 12), (int)iy + 3, right, sel ? pal::white : pal::yellow, 255, RIGHT);
  }
  const char* hint = listId == 0 ? (listCount == 5 && listSel >= 3 ? (listSel == 3 ? "печенька другу: ему +15, тебе +5" : "") : FOOD_HINT[listSel])
                                 : (listId == 1 ? ((listCount == 8 && listSel >= 6) ? (listSel == 6 ? "реакция наперегонки с гостем" : "") : GAME_HINT[listSel])
                                                : (listId == 3 ? "долго — купить или надеть" : ""));
  if (!*hint) hint = "коротко — дальше · долго — ок";
  c.hline(12, (int)y + 106, W - 24, pal::line);
  c.text(font_small, W / 2, (int)y + 108, hint, pal::fg, 255, CENTER);
  if (listCount > vis) {
    float th = vis * rh;
    float sh = (float)vis / listCount * th, sy = y + 28 + (float)first / listCount * th;
    c.vline((int)(x + w - 6), (int)y + 28, (int)th, pal::line);
    c.roundRect(x + w - 7, sy, 2, sh, 1, pal::cyan);
  }
  drawToast(c);
}

// --- статус ---

void Game::bar(Canvas& c, int x, int y, int w, int v, Color col, const char* label) {
  c.text(font_small, x, y, label, pal::fg);
  char n[8];
  sfmt(n, 8, "%d%%", v);
  c.text(font_ui, x + w, y - 1, n, pal::white, 255, RIGHT);
  c.roundRect((float)x, (float)y + 16, (float)w, 10, 5, pal::ink);
  float fw = (w - 2) * clampf(v / 100.0f * ease(clampf((now - sceneStart) / 500.0f, 0, 1)), 0, 1);
  if (fw > 1) c.roundRectGrad((float)x + 1, (float)y + 17, fw, 8, 4, shade(col, 330), col);
}

void Game::drawStatus(Canvas& c) {
  c.clear(pal::editor);
  drawTopBar(c, false);
  c.roundRect(5, TOP + 5, 72, H - TOP - 10, 8, pal::line);
  c.roundRectGrad(6, TOP + 6, 70, H - TOP - 12, 7, pal::editor, pal::gutter);
  c.text(font_mono, 41, TOP + 8, "ПИТОМЕЦ", pal::fg, 255, CENTER);
  c.hline(13, TOP + 89, 56, pal::line);
  char pos[8];
  sfmt(pos, sizeof pos, "%d/12", page + 1);
  c.text(font_mono, 41, TOP + 91, pos, pal::cyan, 255, CENTER);
  Pose p = basePose();
  p.x = 41; p.y = TOP + 84; p.scale = 0.84f; p.lookX = 0.6f;
  drawPet(c, (Form)s.form, p);
  int x = 86, w = 148;
  char b[64];
  switch (page) {
    case 0: {
      c.text(font_ui, x, TOP + 4, formName((Form)s.form), s.form == HACKER ? pal::green : formColor((Form)s.form));
      uint32_t d = s.ageMin / 1440, hh = (s.ageMin / 60) % 24;
      sfmt(b, 64, "Возраст: %u дн %u ч", d, hh); c.text(font_small, x, TOP + 27, b, pal::fg);
      sfmt(b, 64, "Вес: %d КБ", s.weight); c.text(font_small, x, TOP + 45, b, pal::fg);
      c.text(font_small, x, TOP + 63, "Имя:", pal::fg);
      c.text(font_small, x + Canvas::width(font_small, "Имя: "), TOP + 63, nick(), pal::cyan);
      sfmt(b, 64, "Поколение %u · v%d.%u", s.generation, stageOf((Form)s.form), d); c.text(font_small, x, TOP + 81, b, pal::fg);
      break;
    }
    case 1:
      bar(c, x, TOP + 4, w, s.hunger, pal::orange, "Сытость");
      bar(c, x, TOP + 34, w, s.happy, pal::pink, "Настроение");
      bar(c, x, TOP + 64, w, s.energy, pal::yellow, "Энергия");
      break;
    case 2:
      bar(c, x, TOP + 4, w, s.discipline, pal::blue, "Дисциплина");
      sfmt(b, 64, "Ошибки ухода: %u", s.mistakes); c.text(font_small, x, TOP + 38, b, s.mistakes ? pal::orange : pal::fg);
      if (s.sick) sfmt(b, 64, "Болеет: ещё %u прогон", s.doses);
      else sfmt(b, 64, "Здоров");
      c.text(font_small, x, TOP + 56, b, s.sick ? pal::red : pal::green);
      sfmt(b, 64, "Багов на полу: %u", s.bugs); c.text(font_small, x, TOP + 74, b, pal::fg);
      break;
    case 3:
      sfmt(b, 64, "Прогулки: %u м", s.totalMeters); c.text(font_small, x, TOP + 6, b, pal::fg);
      sfmt(b, 64, "Шаги сегодня: %u", s.stepsToday); c.text(font_small, x, TOP + 24, b, pal::fg);
      sfmt(b, 64, "Монеты: %u", s.coins); c.text(font_small, x, TOP + 42, b, pal::yellow);
      {
        int hl = batteryHoursLeft();
        if (batCharging) sfmt(b, 64, "Батарея %d%%, зарядка", batPct);
        else if (hl < 0) sfmt(b, 64, "Батарея %d%%", batPct);
        else if (hl < 48) sfmt(b, 64, "Батарея %d%%, ~%d ч", batPct, hl);
        else sfmt(b, 64, "Батарея %d%%, ~%d дн", batPct, hl / 24);
        c.text(font_small, x, TOP + 60, b, batPct < 20 ? pal::orange : pal::fg);
      }
      if (s.clockSource && !clockSure) c.text(font_small, x, TOP + 78, "часы не сверены", pal::yellow);
      else { sfmt(b, 64, "Друзья: %u, встреч %u", s.friendsMet, s.visits); c.text(font_small, x, TOP + 78, b, pal::fg); }
      break;
    case 4: {
      c.text(font_ui, x, TOP + 3, "Характер", pal::white);
      sfmt(b, 64, "%s%s", traitName(s.trait), (s.inherited & 1) ? " (от родителя)" : "");
      c.text(font_small, x, TOP + 22, b, pal::yellow);
      c.text(font_mono, x, TOP + 36, traitHint(s.trait), pal::fg);
      sfmt(b, 64, "Любит %s", favFoodName(s.favFood)); c.text(font_small, x, TOP + 50, b, pal::pink);
      sfmt(b, 64, "Игра: %s", favGameName(s.favGame)); c.text(font_small, x, TOP + 66, b, pal::pink);
      if (s.generation == 1) { sfmt(b, sizeof b, "Самостоятельных дел: %u", s.reserved4[1]); c.text(font_small, x, TOP + 84, b, pal::dim); }
      if (s.generation > 1) {
        sfmt(b, 64, "Родитель: %s", formName((Form)s.parentForm));
        c.text(font_mono, x, TOP + 84, b, pal::dim);
      }
      break;
    }
    case 11: drawCollection(c); break;
    default: drawAchievements(c, (page - 5) * 3, 3); break;  // страницы 5–10: достижения по 3
  }
  for (int i = 0; i < 12; i++) {
    c.hline(x + 2 + i * 12, H - 6, 9, i == page ? pal::cyan : pal::line);
    if (i == page) c.hline(x + 2 + i * 12, H - 5, 9, pal::cyan);
  }
}

// --- мини-игра «Отладка» ---

void Game::drawGameDebug(Canvas& c) {
  c.clear(pal::editor);
  drawTopBar(c, false);
  char b[64];
  sfmt(b, 64, "Отладка %d/5", gRound + 1 > 5 ? 5 : gRound + 1);
  c.text(font_ui, 10, TOP + 4, b, pal::white);
  sfmt(b, 64, "поймано %d", gScore);
  c.text(font_ui, W - 10, TOP + 4, b, pal::green, 255, RIGHT);
  int tx = 14, tw = 166, ty = 56;
  c.roundRect((float)tx - 6, (float)ty - 8, (float)tw + 12, 34, 7, pal::gutter);
  codeLine(c, tx, ty + 3, "if (ok) launch(rocket);", 230);
  uint32_t t = now - gStart;
  if (gPhase == 0) {
    float period = 2.0f * tw / gSpeed;  // мс туда и обратно (скорость в пикселях за мс)
    float ph = (float)(t % (uint32_t)period) / period;
    float tri = ph < 0.5f ? ph * 2 : 2 - ph * 2;
    gCaret = tx + tri * tw;
  }
  float zx = tx + gZone * tw / 200;  // зона бага
  gZoneX = zx;
  c.roundRect(zx - gZoneW / 2, (float)ty - 6, gZoneW, 30, 5, pal::red, 70);
  if (gPhase != 1) drawBug(c, zx, (float)ty + 20, now * 0.02f, 1, 1.5f);
  c.fillRect((int)gCaret, ty - 10, 3, 38, pal::accent);
  c.triangle(gCaret - 5, (float)ty - 15, gCaret + 8, (float)ty - 15, gCaret + 1.5f, (float)ty - 8, pal::accent);
  Pose p = basePose();
  p.x = 210; p.y = H - 4; p.lookX = -0.8f; p.lookY = -0.6f;
  if (gPhase == 1) { p.eyes = E_HAPPY; p.mouth = M_GRIN; p.arm = 1; p.lift = 6; }
  if (gPhase == 2) { p.eyes = E_SAD; p.mouth = M_SAD; }
  drawPet(c, (Form)s.form, p);
  if (gPhase == 1 || gPhase == 2) {
    c.text(font_big, 14, 96, gPhase == 1 ? "Пойман!" : "Мимо…", gPhase == 1 ? pal::green : pal::red);
    if (t > 900) {
      gRound++;
      if (gRound >= 5) {
        gameOver(G_DEBUG, gScore, 5);
      } else {
        gPhase = 0; gStart = now;
        gZoneW = 36 - gRound * 5.0f;
        gSpeed = 0.11f + gRound * 0.035f;
        gZone = 20 + rndInt(160);
      }
    }
  } else if (gPhase == 0) {
    c.text(font_small, 14, 96, "Жми, когда курсор", pal::fg);
    c.text(font_small, 14, 112, "на баге", pal::fg);
  }
  if (gPhase == 3) {
    c.dim(110);
    c.roundRect(16, 28, 208, 84, 11, pal::gutter);
    sfmt(b, 64, "Поймано %d из 5", gScore);
    c.textFit(120, 36, b, gScore >= 3 ? pal::green : pal::orange, 196);
    c.text(font_small, 120, 66, gScore >= 3 ? "Релиз без багов! +настроение" : "Баги ушли в прод…", pal::white, 255, CENTER);
    c.text(font_small, 120, 88, "нажми, чтобы выйти", pal::fg, 255, CENTER);
  }
}

// --- мини-игра «Память»: покажи последовательность, повтори короткими/долгими ---

void Game::drawGameBit(Canvas& c) {
  c.clear(pal::editor);
  drawTopBar(c, false);
  char b[64];
  sfmt(b, sizeof b, "Память %d/5", gRound + 1 > 5 ? 5 : gRound + 1);
  c.text(font_ui, 10, TOP + 4, b, pal::white);
  sfmt(b, sizeof b, "верно %d", gScore);
  c.text(font_ui, W - 10, TOP + 4, b, pal::green, 255, RIGHT);

  uint32_t elapsed = now - gStart;
  int active = -1;
  if (gPhase == 0) {
    uint32_t slot = elapsed / 850u;
    uint32_t phase = elapsed % 850u;
    if (slot < (uint32_t)gRound + 1 && phase < 610u) active = memoryPattern[slot];
    if (elapsed >= ((uint32_t)gRound + 1) * 850u + 200u) {
      gPhase = 1; memoryCursor = 0; gStart = now;
    }
  }
  if (gPhase == 2 && elapsed > 1100u) {
    gRound++;
    if (gRound >= gMax) gameOver(G_BIT, gScore, gMax);
    else { gPhase = 0; memoryCursor = 0; gStart = now; }
  }

  // Два крупных символа, читаемые на экране 240x135.
  for (int i = 0; i < 2; ++i) {
    int x = 99 + i * 66, y = 51;
    bool glow = gPhase == 0 && active == i;
    c.roundRect((float)x, (float)y, 56, 51, 8, glow ? pal::green : pal::line);
    c.roundRect((float)(x + 2), (float)(y + 2), 52, 47, 6, glow ? pal::accent : pal::gutter);
    c.text(font_big, x + 28, y + 7, i == 0 ? "0" : "1", glow ? pal::white : pal::fg, 255, CENTER);
  }
  Pose p = basePose();
  p.x = 44; p.y = H - 7; p.lookX = 1; p.scale = 0.8f;
  if (gPhase == 2) { p.eyes = gResult ? E_HAPPY : E_SAD; p.mouth = gResult ? M_GRIN : M_SAD; }
  drawPet(c, (Form)s.form, p);

  if (gPhase == 0) {
    sfmt(b, sizeof b, "Смотри и запоминай (%d)", gRound + 1);
    c.text(font_small, 155, 110, b, pal::cyan, 255, CENTER);
  } else if (gPhase == 1) {
    sfmt(b, sizeof b, "Повтори %d/%d", memoryCursor + 1, gRound + 1);
    c.text(font_small, 155, 106, b, pal::white, 255, CENTER);
    c.text(font_small, 155, 118, "коротко 0 · долго 1", pal::fg, 255, CENTER);
  } else if (gPhase == 2) {
    c.text(font_ui, 155, 110, gResult ? "Верно!" : "Ошибка", gResult ? pal::green : pal::red, 255, CENTER);
  } else if (gPhase == 3) {
    c.dim(110);
    c.roundRect(16, 28, 208, 84, 11, pal::gutter);
    sfmt(b, sizeof b, "Запомнил %d из %d", gScore, gMax);
    c.textFit(120, 36, b, gScore >= 3 ? pal::green : pal::orange, 196);
    c.text(font_small, 120, 66, gScore >= 3 ? "Отличная память! +настроение" : "Ещё немного тренировки!", pal::white, 255, CENTER);
    c.text(font_small, 120, 88, "нажми, чтобы выйти", pal::fg, 255, CENTER);
  }
}

// --- Обучение: пять коротких заданий на внимание ---
void Game::drawTraining(Canvas& c) {
  c.clear(pal::editor);
  drawTopBar(c, false);
  c.text(font_ui, 10, TOP + 4, "Тренировка Бита", pal::white);
  char b[48];
  sfmt(b, sizeof b, "%d из 5", trainPhase == 2 ? 5 : (int)trainRound + 1);
  c.text(font_small, W - 10, TOP + 7, b, pal::cyan, 255, RIGHT);
  Pose p = basePose();
  p.x = 45; p.y = H - 12; p.scale = 0.8f; p.lookX = 1;
  if (trainPhase == 1 || trainPhase == 2) {
    p.eyes = trainPhase == 2 || trainLastOk ? E_HAPPY : E_SAD;
    p.mouth = trainPhase == 2 || trainLastOk ? M_GRIN : M_SAD;
  }
  drawPet(c, (Form)s.form, p);
  if (trainPhase == 2) {
    sfmt(b, sizeof b, "Верно: %d из 5", trainCorrect);
    c.text(font_ui, 157, 47, b, pal::white, 255, CENTER);
    int reward = trainCorrect >= 2 ? 5 + trainCorrect : 0;
    sfmt(b, sizeof b, "Дисциплина +%d", reward);
    c.text(font_ui, 157, 76, b, reward ? pal::green : pal::orange, 255, CENTER);
    c.text(font_small, 157, 109, "нажми — домой", pal::dim, 255, CENTER);
    return;
  }
  // Мини-игра рассчитана на одну кнопку: короткое нажатие = 0, долгое = 1.
  int x = 129, y = 44;
  c.roundRect((float)x, (float)y, 56, 47, 7, pal::line);
  c.roundRect((float)(x + 2), (float)(y + 2), 52, 43, 6,
              trainPhase == 1 ? (trainLastOk ? pal::green : pal::red) : pal::gutter);
  c.text(font_big, x + 28, y + 4, trainPrompt ? "1" : "0", pal::white, 255, CENTER);
  uint32_t elapsed = now - trainRoundAt;
  int remaining = (int)((TRAIN_ROUND_MS - (elapsed < TRAIN_ROUND_MS ? elapsed : TRAIN_ROUND_MS)) / 1000u);
  sfmt(b, sizeof b, "%d с", remaining);
  c.text(font_small, 222, 55, b, pal::yellow, 255, RIGHT);
  float progress = clampf(elapsed / (float)TRAIN_ROUND_MS, 0, 1);
  c.fillRect(98, 98, 118, 4, pal::line);
  c.fillRect(98, 98, (int)(118 * (1 - progress)), 4, pal::cyan);
  if (trainPhase == 0)
    c.text(font_small, 157, 109, "коротко 0 · долго 1", pal::fg, 255, CENTER);
  else
    c.text(font_small, 157, 109, trainLastOk ? "Верно!" : "Попробуй дальше", trainLastOk ? pal::green : pal::orange, 255, CENTER);
}

// --- прогулка ---

void Game::drawWalk(Canvas& c) {
  int h = hourNow();
  const DayLight& l = dayLight(h);
  bool night = h < 5 || h >= 21;
  c.gradientV(0, TOP, W, MENU_Y - TOP, l.sky, l.horizon);
  bool moving = fix.posValid;
  float dt = clampf((now - lastWalkMs) * 0.001f, 0, 0.1f);
  lastWalkMs = now;
  walkScroll += dt * (moving ? 30 : 6);
  // Крыша с картинки: небо (индекс 0) — градиент по часам, ночью луна, днём солнце за домами.
  int k = sceneIndex(h);
  const uint8_t* px = artdata::ROOF_PX[k];
  const uint16_t* pal = artdata::ROOF_PAL[k];
  if (night) {
    const artdata::Sprite& m = artdata::MOON;
    for (int y = 0; y < m.h; y++)
      for (int x = 0; x < m.w; x++)
        if (uint8_t q = m.px[y * m.w + x]) c.pixel(artdata::MOON_X + x, TOP + artdata::MOON_Y + y, m.pal[q]);
  } else if (h < 19) {
    float sx = artdata::MOON_X + 9, sy = TOP + ((h < 8 || h >= 17) ? 30 : 10);
    c.circle(sx, sy, 12, hex(0xffe3a0), 45);
    c.circle(sx, sy, 7, hex(0xffd48e));
  }
  int shift = (int)walkScroll % artdata::ROOF_TILE;
  for (int y = 0; y < artdata::SCENE_H; y++) {
    bool floor = y >= artdata::ROOF_FLOOR_Y;
    for (int x = 0; x < W; x++) {
      int sx = floor ? (x + shift) % artdata::ROOF_TILE : x;
      if (uint8_t q = px[y * artdata::SCENE_W + sx]) c.pixel(x, TOP + y, pal[q]);
    }
  }
  Pose p = basePose();
  p.x = 62; p.y = 104; p.scale = 0.9f;
  p.walk = walkScroll * 0.3f;
  p.lift = moving ? fabs_(fsin(p.walk)) * 2.5f : 0;
  p.lookX = 1;
  p.mouth = M_SMILE;
  drawPet(c, (Form)s.form, p);
  drawTopBar(c, false);
  gpsPanel(c, 122, TOP + 4, 114, false);
  if (s.questKind && questDist >= 0) {  // квест: стрелка и расстояние
    c.roundRect(4, TOP + 4, 114, 40, 8, pal::ink, 190);
    float ax = 96, ay = TOP + 24;
    c.circle(ax, ay, 15, pal::gutter);
    float rel = (courseKnown ? questBearing - walkCourse : questBearing) * 0.0174533f;
    float sx = fsin(rel), sy = -fcos(rel);
    c.triangle(ax + sx * 12, ay + sy * 12, ax - sy * 5 - sx * 6, ay + sx * 5 - sy * 6, ax + sy * 5 - sx * 6, ay - sx * 5 - sy * 6, pal::yellow);
    if (!courseKnown) c.text(font_mono, (int)ax, (int)ay - 15, "С", pal::fg, 255, CENTER);
    if (s.questKind == 1) drawBug(c, 16, TOP + 22, now * 0.03f, 1, 0.9f);
    else drawFoodIcon(c, 0, 16, TOP + 18);
    char q[24];
    sfmt(q, sizeof q, "%d м", (int)questDist);
    c.text(font_ui, 28, TOP + 7, q, pal::white);
    c.text(font_mono, 28, TOP + 26, courseKnown ? "по ходу" : "север вверху", pal::dim);
  }
  char b[64];
  c.fillRect(0, MENU_Y, W, H - MENU_Y, pal::gutter);
  sfmt(b, 64, "Всего %u м · долго — домой", s.totalMeters);
  c.text(font_small, 120, MENU_Y + 6, b, pal::fg, 255, CENTER);
  drawToast(c);
}

// --- часы ---

void Game::drawClock(Canvas& c) {
  c.clear(pal::editor);
  drawTopBar(c, false);
  c.text(font_ui, 120, TOP + 6, clockMode ? "Будильник" : "Установка времени", pal::white, 255, CENTER);
  if (clockMode) {
    bool sel = clockField == 2;
    c.roundRect(186, 50, 46, 32, 8, sel ? pal::accent : pal::gutter);
    c.text(font_ui, 209, 57, clockOn ? "вкл" : "выкл", clockOn ? pal::green : pal::fg, 255, CENTER);
  }
  char hh[4], mm[4];
  sfmt(hh, 4, "%02d", clockH);
  sfmt(mm, 4, "%02d", clockM);
  for (int f = 0; f < 2; f++) {
    float x = f ? 150 : 90;
    bool sel = clockField == f;
    c.roundRect(x - 27, 46, 54, 40, 9, sel ? pal::accent : pal::gutter);
    c.text(font_big, (int)x, 54, f ? mm : hh, pal::white, 255, CENTER);
  }
  if ((now / 500) & 1) c.text(font_big, 120, 53, ":", pal::fg, 255, CENTER);
  c.text(font_small, 120, 96, "коротко +1 · долго — дальше", pal::fg, 255, CENTER);
  c.text(font_small, 120, 114, "с GPS время ставится само", pal::dim, 255, CENTER);
}

// --- эволюция и вылупление ---

void Game::drawHatch(Canvas& c) {
  drawBackground(c, false);
  float prog = s.stageMin / 60.0f + (secAcc / 60000.0f);
  if (prog > 1) prog = 1;
  drawEgg(c, 133, FLOOR, 0.4f + prog * 1.5f, prog, now);
  static const char* LOG[] = {"[ 10%] Building bit.o", "[ 35%] Linking heart.so", "[ 60%] Generating eyes",
                              "[ 85%] Cuteness -O3", "[100%] Built target bit"};
  c.roundRect(24, TOP + 4, 120, 44, 7, pal::ink, 190);
  int shown = (int)(prog * 5) + 1;
  if (shown > 5) shown = 5;
  int first = shown > 3 ? shown - 3 : 0;
  for (int i = first; i < shown; i++) c.text(font_mono, 30, TOP + 8 + (i - first) * 13, LOG[i], i == shown - 1 ? pal::green : pal::dim);
  c.fillRect(0, MENU_Y, W, H - MENU_Y, pal::gutter);
  c.text(font_ui, 8, MENU_Y + 4, "Сборка", pal::white);
  c.roundRect(70, MENU_Y + 9, 124, 9, 4.5f, pal::ink);
  c.roundRectGrad(71, MENU_Y + 10, 122 * prog + 1, 7, 3.5f, hex(0x9ff0b0), pal::green);
  char b[24];
  sfmt(b, 24, "%d%%", (int)(prog * 100));
  c.text(font_ui, W - 6, MENU_Y + 4, b, pal::white, 255, RIGHT);
  drawTopBar(c, false);
}

void Game::drawEvolve(Canvas& c) {
  uint32_t t = now - sceneStart;
  drawBackground(c, false);
  c.dim(120);
  Pose p = basePose();
  p.x = 120; p.y = FLOOR + 4;
  p.eyes = E_NORMAL; p.mouth = M_O;
  if (t < 1500) {
    p.glow = t / 1500.0f;
    p.shake = t / 500.0f;
    if (evoFrom == EGG) drawEgg(c, 120, FLOOR, 2 + t / 300.0f, 1, now);
    else drawPet(c, evoFrom, p);
    if ((t / 80) % 3 == 0) burst(120 + rndInt(40) - 20, FLOOR - 20, 1, pal::yellow);
  } else if (t < 2300) {
    float k = (t - 1500) / 800.0f;
    for (int i = 0; i < 3; i++) c.ring(120, FLOOR - 22, 10 + k * 120 + i * 14, 4, pal::white, (int)(220 * (1 - k)));
    c.tint(pal::white, (int)(255 * (1 - k)));
    if (t < 1560) burst(120, FLOOR - 22, 16, pal::yellow);
  } else {
    float k = clampf((t - 2300) / 600.0f, 0, 1);
    p.squash = 0.6f + elastic(k) * 0.4f;
    p.eyes = E_HAPPY; p.mouth = M_GRIN; p.arm = 1;
    drawPet(c, evoTo, p);
    float bk = ease(clampf((t - 2600) / 400.0f, 0, 1));
    char b[64];
    sfmt(b, 64, "v%d.0 · %s", stageOf(evoTo), formName(evoTo));
    const Font& f = Canvas::width(font_big, b) + 24 <= W - 8 ? font_big : font_ui;
    int w = Canvas::width(f, b) + 24;
    float y = TOP + 3 - (1 - bk) * 34;
    c.roundRect((W - w) / 2.0f, y, (float)w, 28, 9, formColor(evoTo));
    c.text(f, 120, (int)y + (&f == &font_big ? 3 : 6), b, evoTo == HACKER ? pal::green : pal::ink, 255, CENTER);
    if ((t / 200) % 2 == 0 && t < 4500) spawn(0, 120 + rndInt(100) - 50, FLOOR - 50, 0, -10, 0.8f, pal::yellow);
  }
  drawParticles(c, now - lastRender);
  drawTopBar(c, false);
  if (t > 5200) { goHome(); jumpAt = now; dirty = true; }
}

void Game::drawDeath(Canvas& c) {
  uint32_t t = now - sceneStart;
  bool old = s.causeOfDeath == 3;
  if (t < 4500 && !old) {  // «синий экран»
    c.clear(hex(0x1565c0));
    if (t < 400) for (int i = 0; i < 40; i++) c.shiftRow(rndInt(H), rndInt(30) - 15);
    c.text(font_big, 14, 6, ":(", pal::white);
    c.text(font_ui, 14, 36, "Питомец столкнулся", pal::white);
    c.text(font_ui, 14, 54, "с проблемой.", pal::white);
    int pct = clampi((int)(t / 35), 0, 100);
    char b[64];
    sfmt(b, 64, "%d%% сбор дампа памяти", pct);
    c.text(font_small, 14, 78, b, pal::white);
    c.text(font_ui, 14, 104, s.causeOfDeath == 1 ? "SEGMENTATION FAULT" : "OUT OF MEMORY", pal::white);
    return;
  }
  if (t < 4500 && old) {
    c.clear(hex(0x0b0e14));
    c.text(font_ui, 10, 8, "$ ./bit", pal::green);
    c.text(font_small, 10, 30, "Прожита долгая жизнь.", pal::fg);
    c.text(font_small, 10, 48, "exit code 0", pal::green);
    Pose p = basePose();
    p.x = 180; p.y = 126 - t * 0.02f; p.eyes = E_CLOSED; p.mouth = M_SMILE;
    p.alpha = 255 - clampf(t / 4500.0f, 0, 1) * 120;
    drawPet(c, GHOST, p);
    return;
  }
  // кладбище из арт-пака — в свете текущего часа
  c.clear(pal::gutter);
  useRoom(GHOST, sceneIndex(hourNow()));
  roomDim = 0;
  sceneRows(c, 0, W, 0, artdata::SCENE_H);
  char l1[64], l2[64];
  sfmt(l1, 64, "%s", formName((Form)s.form));
  sfmt(l2, 64, "%u дн. · пок. %u", s.ageMin / 1440, s.generation);
  drawTomb(c, 92, 108, l1, l2);
  Pose p = basePose();
  p.x = 190; p.y = 76 + fsin(now * 0.002f) * 4; p.eyes = E_CLOSED; p.mouth = M_SMILE; p.alpha = 170;
  drawPet(c, GHOST, p);
  if (t > 5500) c.text(font_small, 120, 116, "долго — новый проект", pal::white, 255, CENTER);
}

// состояние GPS: есть ли связь с модулем, сколько спутников видно и в работе, есть ли координаты
static const char* gpsState(const GpsFix& f, bool waiting, Color& col) {
  if (!f.alive) { col = waiting ? pal::yellow : pal::red; return waiting ? "Включаю" : "Нет связи"; }
  if (f.posValid) { col = pal::green; return "Есть сигнал"; }
  col = pal::yellow;
  return "Поиск";
}

void Game::gpsPanel(Canvas& c, int x, int y, int w, bool) {
  int h = 76;
  c.roundRect((float)x, (float)y, (float)w, (float)h, 8, pal::ink, 190);
  char b[64];
  Color sc;
  const char* st = gpsState(fix, now - gpsOnSince < 3000, sc);
  bool on = sc != pal::yellow || ((now / 400) & 1);
  c.circle((float)x + 10, (float)y + 11, 4, sc, on ? 255 : 90);
  c.text(font_ui, x + 18, y + 2, st, sc);
  if (!fix.alive) {
    c.text(font_small, x + 8, y + 24, now - gpsOnSince < 3000 ? "жду данные" : "модуль молчит", pal::fg);
  } else {
    sfmt(b, 64, "%d", fix.inView);
    c.text(font_big, x + 8, y + 21, b, pal::white);
    int nx = x + 12 + Canvas::width(font_big, b);
    c.text(font_small, nx, y + 22, "видно", pal::fg);
    c.text(font_small, nx, y + 34, "спутников", pal::fg);
    sfmt(b, 64, "слышно %d · в работе %d", fix.heard, fix.sats);
    c.text(font_mono, x + 8, y + 50, b, fix.sats >= 4 ? pal::green : (fix.heard ? pal::fg : pal::orange));
  }
  sfmt(b, 64, "%u м", s.walkMeters - walkStartMeters);
  c.text(font_ui, x + w - 8, y + h - 19, b, pal::white, 255, RIGHT);
}

void Game::drawGps(Canvas& c) {
  c.clear(pal::editor);
  drawTopBar(c, false);
  char b[64];
  bool waiting = now - gpsOnSince < 3000;
  Color sc;
  gpsState(fix, waiting, sc);
  bool on = sc != pal::yellow || ((now / 400) & 1);
  c.circle(14, TOP + 13, 5, sc, on ? 255 : 90);
  if (!fix.alive) {
    c.text(font_ui, 25, TOP + 4, waiting ? "GPS: включаю…" : "GPS: нет связи с модулем", sc);
    c.text(font_small, 10, TOP + 30, waiting ? "жду первые данные" : "приёмник не отвечает", pal::fg);
    if (!waiting) c.text(font_small, 10, TOP + 48, "пробую сменить выводы RX/TX", pal::dim);
  } else {
    c.text(font_ui, 25, TOP + 4, fix.posValid ? "GPS: координаты есть" : "GPS: ищу спутники", sc);
    // три числа: видно (знает о них), слышно (есть сигнал), в работе (для координат нужно 4)
    struct { int v; const char* l; Color c; } col[3] = {
        {fix.inView, "видно", pal::white},
        {fix.heard, "слышно", fix.heard ? pal::white : pal::red},
        {fix.sats, "в работе", fix.sats >= 4 ? pal::green : pal::white}};
    for (int i = 0; i < 3; i++) {
      int x = 10 + i * 78;
      sfmt(b, 64, "%d", col[i].v);
      c.text(font_big, x, TOP + 24, b, col[i].c);
      c.text(font_small, x + 4 + Canvas::width(font_big, b), TOP + 31, col[i].l, pal::fg);
    }
    // подсказка по сигналу
    const char* hint;
    Color hc = pal::fg;
    if (!fix.heard) { hint = "сигнала нет: на улицу, экраном вверх"; hc = pal::orange; }
    else if (fix.bestSnr < 25) { sfmt(b, 64, "сигнал слабый (%d дБ): открытое небо", fix.bestSnr); hint = b; hc = pal::yellow; }
    else if (!fix.posValid) { sfmt(b, 64, "сигнал хороший (%d дБ), считаю координаты", fix.bestSnr); hint = b; }
    else { int hd = (int)(fix.hdop * 10); sfmt(b, 64, "сигнал %d дБ, точность HDOP %d.%d", fix.bestSnr, hd / 10, hd % 10); hint = b; hc = pal::green; }
    c.text(font_small, 10, TOP + 54, hint, hc);
    char tb[64];
    Color tc = pal::dim;
    uint32_t t = fix.utc + (int32_t)s.tz * 3600;
    if (!s.gpsAuto) { sfmt(tb, sizeof tb, "время по GPS отключено"); tc = pal::dim; }
    else switch (fix.timeValid ? gpsTimeNote : 0) {
      case 1: sfmt(tb, sizeof tb, "время: жду устойчивой привязки (%u с)", (unsigned)(30 - (stableFix > 60 ? 60 : stableFix) / 2)); tc = pal::yellow; break;
      case 2: sfmt(tb, sizeof tb, "время модуля %02u:%02u — это время с включения", (t / 3600) % 24, (t / 60) % 60); tc = pal::orange; break;
      case 3: sfmt(tb, sizeof tb, "время %02u:%02u:%02u — часы поставлены", (t / 3600) % 24, (t / 60) % 60, t % 60); tc = pal::green; break;
      default: sfmt(tb, sizeof tb, "времени от спутников пока нет"); break;
    }
    c.text(font_small, 10, TOP + 70, tb, tc);
  }
  c.fillRect(0, MENU_Y, W, H - MENU_Y, pal::gutter);
  c.text(font_small, 120, MENU_Y + 6, "на улице до 2 мин · долго — выйти", pal::fg, 255, CENTER);
}

void Game::drawConfirm(Canvas& c) {
  drawHome(c);
  c.dim(90);
  c.roundRect(16, 22, 208, 92, 9, pal::line);
  c.roundRect(17, 23, 206, 90, 8, pal::gutter);
  c.hline(27, 72, 186, pal::line);
  if (confirmMode == 1) {
    c.text(font_ui, 120, 30, "Сбросить прогулки?", pal::white, 255, CENTER);
    c.text(font_small, 120, 52, "метры, места, квесты и их награды", pal::fg, 255, CENTER);
  } else {
    c.text(font_ui, 120, 30, "Удалить питомца?", pal::white, 255, CENTER);
    c.text(font_small, 120, 52, "rm -rf ./bit без возврата", pal::fg, 255, CENTER);
  }
  for (int i = 0; i < 2; i++) {
    float x = i ? 160 : 80;
    bool sel = listSel == i;
    Color col = i ? pal::cyan : pal::red;
    c.roundRect(x - 39, 78, 78, 25, 6, sel ? col : pal::line);
    c.roundRect(x - 38, 79, 76, 23, 5, sel ? pal::line : pal::editor);
    c.text(font_ui, (int)x, 81, i ? "Отмена" : "Да", sel ? col : pal::fg, 255, CENTER);
  }
}

void Game::render(Canvas& c) {
  switch (scene) {
    case S_HOME: drawHome(c); break;
    case S_LIST: drawList(c); break;
    case S_STATUS: drawStatus(c); break;
    case S_GAME_DEBUG: drawGameDebug(c); break;
    case S_GAME_BIT: drawGameBit(c); break;
    case S_WALK: drawWalk(c); break;
    case S_CLOCK: drawClock(c); break;
    case S_EVOLVE: drawEvolve(c); break;
    case S_DEATH: drawDeath(c); break;
    case S_HATCH: drawHatch(c); break;
    case S_CONFIRM: drawConfirm(c); break;
    case S_GPS: drawGps(c); break;
    case S_GAME_DEPLOY: drawGameDeploy(c); break;
    case S_GAME_PING: drawGamePing(c); break;
    case S_GAME_COMPILE: drawGameCompile(c); break;
    case S_ALARM: drawAlarm(c); break;
    case S_GAME_REACT: drawGameReact(c); break;
    case S_DUEL: drawDuel(c); break;
    case S_TRAIN: drawTraining(c); break;
  }
  lastRender = now;
}

}  // namespace pet
