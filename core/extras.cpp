// Характер, наследство, достижения, коллекция, новые мини-игры и будильник.
#include "game.h"
#include "util.h"

namespace pet {
using namespace gfx;

static constexpr int TOP = 22, MENU_Y = 109, FLOOR = 120;

// --- характер ---

const char* traitName(uint8_t t) {
  static const char* n[] = {"Обжора", "Непоседа", "Соня", "Умник", "Спокойный"};
  return t < TRAIT_COUNT ? n[t] : "?";
}
const char* traitHint(uint8_t t) {
  static const char* n[] = {"быстрее голодает, еда сытнее", "быстрее скучает, игры веселее",
                            "быстрее устаёт, быстрее высыпается", "ревью даёт больше дисциплины",
                            "грустит медленнее"};
  return t < TRAIT_COUNT ? n[t] : "";
}

static const char* FOOD_NAMES[] = {"кофе", "пиццу", "печеньки"};
static const char* GAME_NAMES[] = {"Отладку", "Память", "Деплой", "Пинг", "Компиляцию", "Реакцию"};
const char* favFoodName(uint8_t f) { return FOOD_NAMES[f % 3]; }
const char* favGameName(uint8_t g) { return GAME_NAMES[g % GAME_COUNT]; }

// --- достижения ---

const char* achName(uint8_t a) {
  static const char* n[] = {"Первый километр", "Марафон", "Неделя без ошибок", "Игроман", "Вырос",
                            "Долгожитель", "Коллекционер", "Секрет", "Чистюля", "Ранняя пташка", "Гурман",
                            "Исследователь", "Охотник", "Реакция", "Друг", "Душа компании", "Ходок", "Модник"};
  return a < ACH_COUNT ? n[a] : "?";
}
const char* achHint(uint8_t a) {
  static const char* n[] = {"1 км прогулок", "10 км прогулок", "7 дней без ошибок", "20 побед в играх",
                            "вырастить взрослого", "дожить до старости", "все 9 форм питомца", "вырастить Нейросеть",
                            "убрать 50 багов", "5 раз по будильнику", "30 раз любимая еда", "25 новых мест",
                            "10 GPS-квестов", "быстрее 0,25 с", "встреча по LoRa", "5 разных друзей",
                            "10 000 шагов за день", "покупка в магазине"};
  return a < ACH_COUNT ? n[a] : "";
}

void Game::unlock(uint8_t a) {
  if (has(a)) return;
  s.achievements |= 1u << a;
  sfmt(toastBuf, sizeof toastBuf, "Достижение: %s", achName(a));
  sfmt(eventText, sizeof eventText, "Достижение: %s", achName(a));
  event(eventText);
  say(toastBuf, hex(0xf5c542), 6000);
  ledFlashUntil = now + 2500;
  ledFlashColor = 0xFFC020;
  addCoins(25, false);
  dirty = true;
}

void Game::checkAchievements() {
  if (s.totalMeters >= 1000) unlock(ACH_KM1);
  if (s.totalMeters >= 10000) unlock(ACH_KM10);
  if (s.streak >= 7) unlock(ACH_WEEK);
  if (s.totalWins >= 20) unlock(ACH_GAMER);
  if (s.formsSeen & ((1 << SENIOR) | (1 << ARCHITECT) | (1 << HACKER) | (1 << LEGACY) | (1 << AI))) unlock(ACH_ADULT);
  const uint16_t all = (1 << BABY) | (1 << JUNIOR) | (1 << MIDDLE) | (1 << FREELANCER) | (1 << SENIOR) |
                       (1 << ARCHITECT) | (1 << HACKER) | (1 << LEGACY) | (1 << AI);
  if ((s.formsSeen & all) == all) unlock(ACH_COLLECTOR);
  if (s.formsSeen & (1 << AI)) unlock(ACH_AI);
  if (s.bugsCleaned >= 50) unlock(ACH_CLEAN);
  if (s.alarmWakes >= 5) unlock(ACH_ALARM);
  if (s.favFed >= 30) unlock(ACH_GOURMET);
  if (s.placesTotal >= 25) unlock(ACH_EXPLORER);
  if (s.questsDone >= 10) unlock(ACH_HUNTER);
  if (s.bestReactCs && s.bestReactCs <= 25) unlock(ACH_REFLEX);
  if (s.friendsMet >= 1) unlock(ACH_FRIEND);
  if (s.friendsMet >= 5) unlock(ACH_SOCIAL);
  if (s.stepsToday >= 10000) unlock(ACH_STEPS);
  if (s.owned) unlock(ACH_SHOPPER);
}

float Game::lifeFrac() const {
  uint32_t d = lifeDays((Form)s.form);
  if (!d) return 0;
  return s.ageMin / (float)((d + s.seed % 3u) * 24u * 60u);
}

// --- загрузка с переносом старых версий ---

static void v3Defaults(Save& v) {
  v.bleOn = 1;
  v.nightLight = 1;
  v.deviceId = v.seed * 2654435761u;
}

bool Game::loadRaw(const uint8_t* data, size_t n) {
  if (n >= sizeof(Save)) {
    Save v;
    memcpy(&v, data, sizeof v);
    if (load(v)) return true;
  }
  // версия 3: всё до полей версии 4, затем контрольная сумма
  const size_t v3body = offsetof(Save, totalSteps);
  if (n >= v3body + 4) {
    uint16_t ver;
    uint32_t crc;
    memcpy(&ver, data + offsetof(Save, version), 2);
    memcpy(&crc, data + v3body, 4);
    if (ver == 3 && crc32(data, v3body) == crc) {
      Save v;
      memset(&v, 0, sizeof v);
      memcpy(&v, data, v3body);
      v.version = 4;
      v.crc = crc32(&v, offsetof(Save, crc));
      if (!load(v)) return false;
      dirty = true;
      return true;
    }
  }
  // версия 2: всё до полей версии 3, затем контрольная сумма
  const size_t v2body = offsetof(Save, questLat);
  if (n >= v2body + 4) {
    uint16_t ver;
    uint32_t crc;
    memcpy(&ver, data + offsetof(Save, version), 2);
    memcpy(&crc, data + v2body, 4);
    if (ver == 2 && crc32(data, v2body) == crc) {
      Save v;
      memset(&v, 0, sizeof v);
      memcpy(&v, data, v2body);
      v.version = 4;
      v3Defaults(v);
      v.crc = crc32(&v, offsetof(Save, crc));
      if (!load(v)) return false;
      dirty = true;
      return true;
    }
  }
  if (n >= sizeof(SaveV1)) {
    SaveV1 o;
    memcpy(&o, data, sizeof o);
    if (o.magic != 0x42495431) return false;
    if (o.version != 1 || crc32(&o, offsetof(SaveV1, crc)) != o.crc) return false;
    Save v;
    memset(&v, 0, sizeof v);
    memcpy(&v, &o, offsetof(SaveV1, crc));  // общая часть лежит одинаково
    v.version = 4;
    v.favFood = (uint8_t)(o.seed % 3);
    v.favGame = (uint8_t)((o.seed >> 4) % GAME_COUNT);
    v.trait = (uint8_t)((o.seed >> 8) % TRAIT_COUNT);
    v.lastDay = o.epoch / 86400;
    v.alarmH = 7; v.alarmM = 30;
    v.totalWins = o.gamesWon;
    for (int f = BABY; f <= (int)o.form && f < GHOST; f++)
      if (stageOf((Form)f) < stageOf((Form)o.form) || f == o.form) v.formsSeen |= (uint16_t)(1 << f);
    v3Defaults(v);
    v.crc = crc32(&v, offsetof(Save, crc));
    if (!load(v)) return false;
    dirty = true;
    return true;
  }
  return false;
}

// --- GPS-квесты: спрятанный баг или кофе в 120–300 м, новые места ---

void Game::questStep(const GpsFix& f) {
  const float M = 111195.0f;
  float coslat = fcos((float)f.lat * 0.0174533f);
  // посещённые места: ячейки 0,002° (~200 м)
  int32_t cy = (int32_t)(f.lat * 500), cx = (int32_t)(f.lon * 500);
  uint32_t cell = ((uint32_t)cy * 73856093u) ^ ((uint32_t)cx * 19349663u) ^ 0x9E3779B9u;
  bool known = false;
  for (uint32_t p : s.places) if (p == cell) { known = true; break; }
  if (!known) {
    s.places[s.placeHead % 24] = cell;
    s.placeHead = (uint8_t)((s.placeHead + 1) % 24);
    s.placesTotal++;
    addCoins(1, false);
    if (s.placesTotal > 1) {
      s.happy = (int16_t)clampi(s.happy + 2, 0, 100);
      sfmt(toastBuf, sizeof toastBuf, "Новое место! Всего %u", s.placesTotal);
      say(toastBuf, pal::cyan);
    }
    dirty = true;
  }
  // цель: нет, или слишком далеко (новая прогулка в другом месте) — ставим новую
  float dy = 0, dx = 0, d = 1e9f;
  if (s.questKind) {
    dy = (s.questLat / 1e6f - (float)f.lat) * M;
    dx = (s.questLon / 1e6f - (float)f.lon) * M * coslat;
    d = fsqrt(dx * dx + dy * dy);
  }
  if (!s.questKind || d > 2000) {
    float dist = 120 + rndInt(180), b = rndInt(360) * 0.0174533f;
    s.questLat = (int32_t)((f.lat + fcos(b) * dist / M) * 1e6);
    s.questLon = (int32_t)((f.lon + fsin(b) * dist / (M * coslat)) * 1e6);
    s.questKind = (uint8_t)(rndInt(3) == 0 ? 2 : 1);
    dy = fcos(b) * dist; dx = fsin(b) * dist; d = dist;
    say(s.questKind == 1 ? "Рядом спрятался баг — найди!" : "Где-то рядом кофе — найди!", pal::yellow);
    dirty = true;
  }
  questDist = d;
  questBearing = fatan2(dx, dy) * 57.29578f;
  if (questBearing < 0) questBearing += 360;
  if (f.courseValid) { walkCourse = f.course; courseKnown = true; }
  if (d <= 20) {  // нашли
    if (s.questKind == 1) { s.happy = (int16_t)clampi(s.happy + 15, 0, 100); say("Баг пойман! +настроение", pal::green); event("Квест: баг пойман!"); }
    else { s.energy = (int16_t)clampi(s.energy + 25, 0, 100); say("Нашёл кофе! +энергия", pal::orange); event("Квест: найден кофе!"); }
    s.questsDone++;
    s.careScore++;
    addCoins(15, false);
    s.questKind = 0;
    questDist = -1;
    ledFlashUntil = now + 1500;
    ledFlashColor = 0x20FF40;
    burst(70, 70, 14, pal::yellow);
    checkAchievements();
    dirty = true;
  }
}

// --- игры ---

void Game::startGame(uint8_t id) {
  static const Scene sc[] = {S_GAME_DEBUG, S_GAME_BIT, S_GAME_DEPLOY, S_GAME_PING, S_GAME_COMPILE, S_GAME_REACT};
  scene = sc[id];
  sceneStart = now;
  gRound = 0; gScore = 0; gPhase = 0; gStart = now; gResult = 0;
  switch (id) {
    case G_DEBUG: gMax = 5; gZoneW = 36; gSpeed = 0.11f; gZone = 20 + rndInt(160); break;
    case G_BIT:
      gMax = 5; memoryCursor = 0;
      for (int i = 0; i < 5; ++i) memoryPattern[i] = (uint8_t)rndInt(2);
      break;
    case G_DEPLOY:
      gMax = 10; dLift = 0; dVy = 0; dSpeed = 70; dScroll = 0;
      dBugs[0] = 260; dBugs[1] = 260 + 110 + rndInt(60); dBugs[2] = dBugs[1] + 110 + rndInt(60);
      break;
    case G_PING: gMax = 10; pRing = 62; break;
    case G_COMPILE: gMax = 5; cProg = 0; cZoneW = 0.12f; cZone = 0.68f + rndInt(18) * 0.01f; break;
    case G_REACT: gMax = 10; rGoAt = now + 1500 + rndInt(2500); rTime = 0; break;
  }
}

void Game::gameOver(uint8_t id, int score, int max) {
  gPhase = 3;
  gStart = now;
  s.gamesPlayed++;
  bool win = score * 10 >= max * 6;
  if (win) { s.gamesWon++; s.totalWins++; }
  int joy = score * 30 / (max ? max : 1);
  if (s.trait == T_FIDGET) joy = joy * 3 / 2;
  if (id == s.favGame) { joy += 10; burst(120, 60, 10, pal::pink); }
  addCoins(score * 10 / (max ? max : 1) + (win ? 5 : 0), false);
  s.happy = (int16_t)clampi(s.happy + joy, 0, 100);
  s.energy = (int16_t)clampi(s.energy - 5, 0, 100);
  if (s.weight > baseWeight((Form)s.form)) s.weight--;
  resolveCall(C_HAPPY);
  checkAchievements();
  dirty = true;
}

static void resultBox(Canvas& c, const char* title, Color tc, const char* line, bool fav) {
  c.dim(110);
  c.roundRect(16, 26, 208, 88, 11, pal::gutter);
  c.textFit(120, 33, title, tc, 196);
  c.text(font_small, 120, 62, line, pal::white, 255, CENTER);
  if (fav) c.text(font_small, 120, 78, "любимая игра: +10", pal::pink, 255, CENTER);
  c.text(font_small, 120, 95, "нажми, чтобы выйти", pal::fg, 255, CENTER);
}

void Game::drawGameResult(Canvas& c, const char* title, const char* good, const char* bad) {
  char b[48];
  sfmt(b, sizeof b, title, gScore, gMax);
  bool win = gScore * 10 >= gMax * 6;
  uint8_t id = scene == S_GAME_DEPLOY ? G_DEPLOY : (scene == S_GAME_PING ? G_PING : (scene == S_GAME_REACT ? G_REACT : G_COMPILE));
  resultBox(c, b, win ? pal::green : pal::orange, win ? good : bad, id == s.favGame);
}

static void gameHeader(Canvas& c, const char* name, int round, int max, int score, const char* scoreWord) {
  char b[48];
  if (round >= 0) sfmt(b, sizeof b, "%s %d/%d", name, round > max ? max : round, max);
  else sfmt(b, sizeof b, "%s", name);
  c.text(font_ui, 10, TOP + 4, b, pal::white);
  sfmt(b, sizeof b, "%s %d", scoreWord, score);
  c.text(font_ui, W - 10, TOP + 4, b, pal::green, 255, RIGHT);
}

// «Деплой»: питомец бежит, баги едут навстречу, нажатие — прыжок
void Game::drawGameDeploy(Canvas& c) {
  float dt = clampf((now - lastRender) * 0.001f, 0, 0.05f);
  c.clear(pal::editor);
  drawTopBar(c, false);
  gameHeader(c, "Деплой", -1, 0, gScore, "пройдено");
  int ground = 112;
  if (gPhase == 0) {
    dVy += 560 * dt;
    dLift -= dVy * dt;
    if (dLift <= 0) { dLift = 0; dVy = 0; }
    dScroll += dSpeed * dt;
    for (int i = 0; i < 3; i++) {
      dBugs[i] -= dSpeed * dt;
      if (dBugs[i] < 40 && dBugs[i] + dSpeed * dt >= 40) {  // баг проехал под питомцем
        gScore++;
        dSpeed += 6;
        if (gScore >= gMax) { gameOver(G_DEPLOY, gScore, gMax); break; }
      }
      if (dBugs[i] < -20) {
        float far = dBugs[0];
        for (float b : dBugs) if (b > far) far = b;
        dBugs[i] = (far < W ? W : far) + 90 + rndInt(80);
      }
      if (dBugs[i] > 40 && dBugs[i] < 66 && dLift < 9) {  // столкновение
        gResult = 1;
        gameOver(G_DEPLOY, gScore, gMax);
        break;
      }
    }
  }
  // дорога из строк кода
  c.fillRect(0, ground, W, H - ground, pal::gutter);
  for (int x = -((int)dScroll % 24); x < W; x += 24) c.fillRect(x, ground + 4, 12, 2, pal::line);
  c.text(font_mono, 238 - ((int)(dScroll * 0.3f) % 300), TOP + 44, "git push origin main", pal::dim, 120);
  for (float b : dBugs) if (b > -20 && b < W + 20) drawBug(c, b, (float)ground, now * 0.04f, -1, 1.3f);
  Pose p = basePose();
  p.x = 52; p.y = (float)ground; p.scale = 0.7f; p.lift = dLift; p.walk = dScroll * 0.12f; p.lookX = 1;
  if (dLift > 0) { p.arm = 1; p.squash = 1.08f; }
  if (gPhase == 3 && gResult) { p.eyes = E_DIZZY; p.mouth = M_O; }
  drawPet(c, (Form)s.form, p);
  if (gPhase == 0 && now - gStart < 3000 && dLift < 1) c.text(font_small, 150, TOP + 24, "нажми — прыжок через баг", pal::fg, 255, CENTER);
  if (gPhase == 3) drawGameResult(c, "Задеплоено %d из %d", "Прод стоит! +настроение", "Баг уронил прод…");
}

// «Пинг»: кольцо сжимается к цели, нажать в момент совпадения
void Game::drawGamePing(Canvas& c) {
  float dt = clampf((now - lastRender) * 0.001f, 0, 0.05f);
  c.clear(pal::editor);
  drawTopBar(c, false);
  gameHeader(c, "Пинг", gRound + 1, 5, gScore, "очки");
  float tx = 160, ty = 72, tr = 14;
  if (gPhase == 0) {
    pRing -= (40 + gRound * 9) * dt;
    if (pRing < 5) { gResult = 0; gPhase = 1; gStart = now; }  // не успел
  }
  // сервер-цель
  c.circle(tx, ty, tr + 2, pal::accent, 60);
  c.ring(tx, ty, tr, 2.5f, pal::accent);
  c.roundRect(tx - 7, ty - 5, 14, 10, 2, pal::fg);
  c.hline((int)tx - 5, (int)ty - 2, 10, pal::editor);
  c.hline((int)tx - 5, (int)ty + 1, 10, pal::editor);
  if (gPhase == 0) c.ring(tx, ty, pRing, 2.5f, pal::yellow);
  // питомец шлёт пакеты
  Pose p = basePose();
  p.x = 52; p.y = H - 8; p.lookX = 1;
  if (gPhase == 1) { p.eyes = gResult ? E_HAPPY : E_SAD; p.mouth = gResult ? M_GRIN : M_SAD; }
  drawPet(c, (Form)s.form, p);
  if (gPhase == 0) {
    float k = ((now - gStart) % 700) / 700.0f;
    c.roundRect(76 + k * 60, ty - 3, 8, 6, 2, pal::green, (int)(255 * (1 - k)));
    c.text(font_small, 120, TOP + 24, "жми, когда кольца совпадут", pal::fg, 255, CENTER);
  }
  if (gPhase == 1) {
    static const char* words[] = {"timeout", "ping 64 ms", "ping 8 ms"};
    Color wc[] = {pal::red, pal::yellow, pal::green};
    c.text(font_ui, 160, 94, words[gResult], wc[gResult], 255, CENTER);
    if (now - gStart > 900) {
      gRound++;
      if (gRound >= 5) gameOver(G_PING, gScore, gMax);
      else { gPhase = 0; gStart = now; pRing = 62; }
    }
  }
  if (gPhase == 3) drawGameResult(c, "Очки: %d из %d", "Сеть летает! +настроение", "Пакеты потерялись…");
}

// «Компиляция»: шкала разгоняется, остановить в зелёной зоне
void Game::drawGameCompile(Canvas& c) {
  float dt = clampf((now - lastRender) * 0.001f, 0, 0.05f);
  c.clear(pal::editor);
  drawTopBar(c, false);
  gameHeader(c, "Компиляция", gRound + 1, 5, gScore, "собрано");
  if (gPhase == 0) {
    cProg += (0.14f + cProg * 0.9f + gRound * 0.05f) * dt;
    if (cProg >= 1) { cProg = 1; gResult = 2; gPhase = 1; gStart = now; }
  }
  float bx = 18, by = 58, bw = 204, bh = 16;
  c.roundRect(bx, by, bw, bh, 8, pal::ink);
  c.roundRect(bx + bw * cZone, by - 3, bw * cZoneW, bh + 6, 4, pal::green, 90);
  float fw = (bw - 4) * cProg;
  Color fc = cProg > cZone + cZoneW ? pal::red : (cProg >= cZone ? pal::green : pal::accent);
  if (fw > 2) c.roundRectGrad(bx + 2, by + 2, fw, bh - 4, 6, shade(fc, 330), fc);
  char b[48];
  sfmt(b, sizeof b, "%d%%", (int)(cProg * 100));
  c.text(font_ui, 120, by + bh + 4, b, pal::white, 255, CENTER);
  static const char* LOG[] = {"compiling bit.cpp", "linking heart.o", "optimizing -O3", "packing release"};
  c.text(font_mono, 18, by + 40, LOG[(int)(cProg * 3.99f)], pal::dim);
  if (gPhase == 0) c.text(font_small, 120, 112, "останови в зелёной зоне", pal::fg, 255, CENTER);
  if (gPhase == 1) {
    static const char* words[] = {"Слишком рано", "Build OK", "Ошибка: перегрев"};
    Color wc[] = {pal::orange, pal::green, pal::red};
    c.text(font_ui, 120, 110, words[gResult], wc[gResult], 255, CENTER);
    if (now - gStart > 1000) {
      gRound++;
      if (gRound >= 5) gameOver(G_COMPILE, gScore, gMax);
      else {
        gPhase = 0; gStart = now; cProg = 0;
        cZoneW = 0.12f - gRound * 0.015f;
        cZone = 0.62f + rndInt(25) * 0.01f;
      }
    }
  }
  if (gPhase == 3) drawGameResult(c, "Собрано %d из %d", "Релиз готов! +настроение", "Сборка красная…");
}

// «Реакция»: ждать зелёного (экран и светодиоды) и нажать как можно быстрее
void Game::drawGameReact(Canvas& c) {
  bool go = gPhase == 0 && now >= rGoAt;
  c.clear(gPhase == 0 ? (go ? hex(0x14532d) : hex(0x3a1218)) : pal::editor);
  drawTopBar(c, false);
  gameHeader(c, "Реакция", gRound + 1, 5, gScore, "очки");
  float cx = 150, cy = 72;
  if (gPhase == 0) {
    c.circle(cx, cy, 30, go ? hex(0x22c55e) : hex(0xdc2626));
    c.circle(cx, cy, 36, go ? hex(0x22c55e) : hex(0xdc2626), 60);
    c.text(font_big, (int)cx, (int)cy - 12, go ? "ЖМИ!" : "жди", pal::white, 255, CENTER);
    c.text(font_small, (int)cx, 112, "жми на зелёный", pal::fg, 255, CENTER);
  }
  Pose p = basePose();
  p.x = 46; p.y = H - 8; p.lookX = 1; p.scale = 0.8f;
  if (gPhase == 0 && !go) { p.eyes = E_WIDE; p.mouth = M_O; }
  if (gPhase == 1) { p.eyes = gResult ? E_HAPPY : E_SAD; p.mouth = gResult ? M_GRIN : M_SAD; }
  drawPet(c, (Form)s.form, p);
  if (gPhase == 1) {
    char b[32];
    if (!rTime) sfmt(b, sizeof b, "Фальстарт!");
    else sfmt(b, sizeof b, "%u мс", rTime);
    Color wc[] = {pal::orange, pal::yellow, pal::green};
    c.text(font_big, (int)cx, (int)cy - 12, b, rTime ? wc[gResult] : pal::red, 255, CENTER);
    if (s.bestReactCs) {
      sfmt(b, sizeof b, "рекорд %u мс", s.bestReactCs * 10);
      c.text(font_small, (int)cx, (int)cy + 14, b, pal::dim, 255, CENTER);
    }
    if (now - gStart > 1300) {
      gRound++;
      if (gRound >= 5) gameOver(G_REACT, gScore, gMax);
      else { gPhase = 0; gStart = now; rGoAt = now + 1500 + rndInt(2500); }
    }
  }
  if (gPhase == 3) drawGameResult(c, "Очки: %d из %d", "Молниеносно! +настроение", "Медленновато…");
}

// сброс прогулок: метры, места, квесты и достижения за них (их могли накрутить скачки координат)
void Game::resetWalks() {
  s.totalMeters = 0;
  s.walkMeters = 0;
  s.placesTotal = 0;
  s.placeHead = 0;
  memset(s.places, 0, sizeof s.places);
  s.questsDone = 0;
  s.questKind = 0;
  s.achievements &= ~((1u << ACH_KM1) | (1u << ACH_KM10) | (1u << ACH_EXPLORER) | (1u << ACH_HUNTER));
  walkStartMeters = 0;
  dirty = true;
}

const char* Game::nick() {
  static char buf[48];
  nickName(hwId ? hwId : s.deviceId, buf, sizeof buf);
  return buf;
}

// --- монеты, шаги, магазин ---

void Game::addCoins(int n, bool show) {
  if (n <= 0) return;
  s.coins += (uint32_t)n;
  if (show) { sfmt(toastBuf, sizeof toastBuf, "+%d монет", n); say(toastBuf, pal::yellow); }
  dirty = true;
}

// шаги с телефона: ~0,7 м на шаг; награды как за прогулку, пока питомец не спит
void Game::addSteps(uint32_t n) {
  if (!n || n > 20000) return;  // разовый пакет больше 20 000 — ошибка счётчика
  s.totalSteps += n;
  s.stepsToday += n;
  addMeters(n * 7 / 10, s.alive && !s.asleep);
  checkAchievements();
}

const char* itemName(uint8_t it) {
  static const char* n[] = {"—", "Шапка с помпоном", "Кепка", "Цилиндр", "Ковбойская шляпа", "Колпак волшебника",
                            "Кошачьи ушки", "Тема Dracula", "Тема Monokai", "Светлая тема", "Тема «Матрица»"};
  return it < IT_COUNT ? n[it] : "?";
}

uint16_t itemPrice(uint8_t it) {
  static const uint16_t p[] = {0, 40, 50, 120, 100, 150, 80, 80, 80, 100, 150};
  return it < IT_COUNT ? p[it] : 0;
}

// --- праздники ---

static void civil(uint32_t days, int& y, int& m, int& d, int& yday) {  // дни от 2000-01-01 -> дата
  y = 2000;
  while (true) {
    uint32_t len = (y % 4 == 0) ? 366 : 365;
    if (days < len) break;
    days -= len;
    y++;
  }
  yday = (int)days + 1;
  static const uint8_t ml[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  m = 1;
  for (int i = 0; i < 12; i++) {
    uint32_t l = ml[i] + (i == 1 && y % 4 == 0 ? 1 : 0);
    if (days < l) break;
    days -= l;
    m++;
  }
  d = (int)days + 1;
}

Holiday Game::holiday() const {
  if (!s.clockSource) return HD_NONE;  // часы не заданы — дата неизвестна
  int y, m, d, yd;
  civil(s.epoch / 86400, y, m, d, yd);
  if ((m == 12 && d == 31) || (m == 1 && d <= 2)) return HD_NEWYEAR;
  if (yd == 256) return HD_PROGRAMMER;  // 13 сентября, в високосный год 12-го
  if (m == 10 && d == 31) return HD_HALLOWEEN;
  if (s.form != EGG && s.ageMin >= 7 * 1440 && (s.ageMin / 1440) % 7 == 0) return HD_BIRTHDAY;  // каждая полная неделя жизни
  return HD_NONE;
}

// бонус раз в праздничный день: настроение, монеты, событие на телефон
void Game::holidayCheck() {
  Holiday h = holiday();
  if (h == HD_NONE || s.asleep || !s.alive) return;
  uint16_t today = (uint16_t)(s.epoch / 86400);
  if (s.bonusDay == today) return;
  s.bonusDay = today;
  static const char* txt[] = {"", "С Новым годом! +50 монет", "С Днём программиста! +50 монет",
                              "Хэллоуин! Сладость или баг? +50 монет", "День рождения! +50 монет"};
  s.happy = (int16_t)clampi(s.happy + 20, 0, 100);
  addCoins(50, false);
  say(txt[h], pal::yellow, 8000);
  event(txt[h]);
  ledFlashUntil = now + 3000;
  ledFlashColor = h == HD_HALLOWEEN ? 0xFF6000 : 0xFF40C0;
  burst(petX, 60, 16, pal::yellow);
  dirty = true;
}

// на сколько часов хватит батареи: по падению заряда за последние часы
int Game::batteryHoursLeft() const {
  if (batCharging || batCount < 7) return -1;  // нужен хотя бы час замеров
  int span = batCount - 1;                      // интервалов по 10 минут
  int drop = batSamples[span] - batSamples[0];
  if (drop <= 0) return -1;
  return batSamples[0] * span / 6 / drop;       // часов: заряд / (падение за час)
}

// --- будильник ---

void Game::triggerAlarm() {
  alarmActive = true;
  sfmt(eventText, sizeof eventText, "Будильник %02d:%02d — подъём!", s.alarmH, s.alarmM);
  event(eventText);
  scene = S_ALARM;
  sceneStart = now;
  justEvolved = true;  // платформа включит экран
  idleMs = 0;
}

void Game::drawAlarm(Canvas& c) {
  uint32_t t = now - sceneStart;
  c.gradientV(0, 0, W, H, hex(0x2a1f4d), hex(0xf0a35e));
  c.circle(200, 104, 26, hex(0xffe9a8), 200);  // рассвет
  c.fillRect(0, 108, W, H - 108, hex(0x3b2f5c));
  char b[16];
  sfmt(b, sizeof b, "%02d:%02d", s.alarmH, s.alarmM);
  c.text(font_big, 16, 8, b, pal::white);
  c.text(font_ui, 16, 34, "Подъём!", pal::white);
  // звенящий колокольчик
  float sw = fsin(now * 0.04f) * 0.35f;
  float bx = 190, by = 40;
  c.circle(bx + sw * 10, by + 14, 3, hex(0xf5c542));
  c.ellipse(bx + sw * 6, by + 2, 12, 13, hex(0xf5c542));
  c.roundRect(bx - 15 + sw * 6, by + 8, 30, 5, 2, hex(0xd4a017));
  for (int i = 0; i < 3; i++) {
    float k = ((now + i * 220) % 660) / 660.0f;
    c.arc(bx, by, 18 + k * 14, 2, 3.6f, 5.8f, pal::white, (int)(200 * (1 - k)));
  }
  Pose p = basePose();
  p.x = 128; p.y = 108; p.eyes = E_HAPPY; p.mouth = M_GRIN; p.arm = (fsin(now * 0.015f) + 1) * 0.5f;
  p.lift = fabs_(fsin(now * 0.008f)) * 12;
  drawPet(c, (Form)s.form, p);
  c.text(font_small, 120, 116, "нажми — проснулся", pal::white, 255, CENTER);
  if (t > 180000) {
    alarmActive = false;
    goHome();
    event("Будильник выключен");  // остановить звонок и уведомление Android
  }  // три минуты звонка
}

// --- статус: достижения и коллекция ---

void Game::drawAchievements(Canvas& c, int first, int count) {
  // Шесть страниц по три достижения. Кубки — особо ценные награды,
  // остальные — медали. Не полученные награды показаны приглушёнными.
  const int x = 86;
  int y = TOP + 2;
  for (int i = first; i < first + count && i < ACH_COUNT; ++i) {
    bool got = has(i);
    bool trophy = i == ACH_KM10 || i == ACH_OLD || i == ACH_COLLECTOR ||
                  i == ACH_AI || i == ACH_SOCIAL || i == ACH_STEPS;
    Color metal = got ? (trophy ? hex(0xffd56a) : hex(0xd8e4ef)) : pal::line;
    Color accent = got ? hex(0xe7ac40) : pal::dim;
    float cx = x + 9, cy = y + 10;
    // Локальная подложка: иконки одинаково читаются на всех шести страницах.
    c.roundRect(x, y + 1, 19, 23, 5, got ? hex(0x34344a) : pal::gutter);
    if (trophy) {
      c.circle(cx - 5.5f, cy - 3, 3.6f, accent);
      c.circle(cx + 5.5f, cy - 3, 3.6f, accent);
      c.roundRect(cx - 5, cy - 8, 10, 10, 2.5f, metal);
      c.roundRect(cx - 1.5f, cy + 1, 3, 5, 1, accent);
      c.roundRect(cx - 6, cy + 6, 12, 2.8f, 1.4f, metal);
      if (got) drawStar(c, cx, cy - 4, 0.65f, hex(0x99601b));
    } else {
      c.triangle(cx - 5, cy - 8, cx - 0.7f, cy - 8, cx - 2, cy + 1, got ? pal::blue : pal::dim);
      c.triangle(cx + 0.7f, cy - 8, cx + 5, cy - 8, cx + 2, cy + 1, got ? pal::red : pal::dim);
      c.circle(cx, cy + 3, 5.5f, accent);
      c.circle(cx, cy + 3, 4.1f, metal);
      if (got) drawStar(c, cx, cy + 3, 0.6f, hex(0x8073a5));
    }
    const char* nm = achName(i);
    bool big = Canvas::width(font_ui, nm) <= W - x - 24;
    c.text(big ? font_ui : font_small, x + 23, y + (big ? 0 : 2), nm, got ? pal::white : pal::fg);
    c.text(font_small, x + 23, y + 16, achHint(i), got ? hex(0xf5c542) : pal::dim);
    if (i + 1 < first + count && i + 1 < ACH_COUNT)
      c.hline(x + 23, y + 30, W - x - 29, pal::line);
    y += 32;
  }
}

void Game::drawCollection(Canvas& c) {
  c.text(font_ui, 86, TOP + 3, "Коллекция", pal::white);
  static const Form forms[] = {BABY, JUNIOR, MIDDLE, FREELANCER, SENIOR, ARCHITECT, HACKER, LEGACY, AI};
  int n = 0;
  for (int i = 0; i < 9; i++) {
    int col = i % 5, row = i / 5;
    float cx = 98 + col * 28, cy = TOP + 50 + row * 40;
    bool seen = (s.formsSeen >> forms[i]) & 1;
    if (seen) {
      n++;
      Pose p;
      p.t = now; p.x = cx; p.y = cy; p.scale = 0.42f; p.mouth = M_SMILE;
      drawPet(c, forms[i], p);
    } else {
      c.circle(cx, cy - 10, 9, pal::ink);
      c.text(font_ui, (int)cx, (int)cy - 19, "?", pal::dim, 255, CENTER);
    }
  }
  char b[24];
  sfmt(b, sizeof b, "%d из 9", n);
  c.text(font_small, W - 8, TOP + 5, b, pal::fg, 255, RIGHT);
}

}  // namespace pet
