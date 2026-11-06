// Встречи питомцев по LoRa: маяки, гости, угощение.
#include "game.h"
#include "util.h"

namespace pet {
using namespace gfx;

static constexpr int FLOOR = 120;  // единая линия пола расширенной комнаты
enum PacketType : uint8_t { P_BEACON = 1, P_GIFT = 2, P_DUEL_INV = 3, P_DUEL_ACC = 4, P_DUEL_RES = 5, P_DUEL_NO = 6 };
// пакет: 'B' 'T' тип | id отправителя (4) | id получателя (4, 0 — всем) | форма | черта | поколение (2) | настроение
static constexpr int PKT = 16;

static void put32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t get32(const uint8_t* p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

bool Game::loraListen() const {
  if (!s.loraOn || !s.alive || s.form == EGG) return false;
  return now < listenUntil || visiting() || duelState;
}

bool Game::loraWantTx(uint8_t* buf, int& len) {
  if (!s.loraOn || !s.alive || s.form == EGG) return false;
  uint8_t type = 0;
  if (!nextBeacon) { nextBeacon = now + 2000 + rndInt(28000); return false; }  // первый маяк — в случайный момент
  if (duelTx && duelTxLeft && now >= nextDuelTx) {
    // пакет дуэли: в байтах 11–14 число (задержка до зелёного или время реакции)
    buf[0] = 'B'; buf[1] = 'T'; buf[2] = duelTx;
    put32(buf + 3, s.deviceId);
    put32(buf + 7, duelPeer);
    put32(buf + 11, duelTx == P_DUEL_ACC ? duelDelay : duelMy);
    buf[15] = 0;
    len = PKT;
    if (--duelTxLeft == 0) duelTx = 0;
    nextDuelTx = now + 700;  // повтор через 0,7 с
    return true;
  }
  if (giftPending) { type = P_GIFT; giftPending = false; }
  else if (now >= nextBeacon) {
    type = P_BEACON;
    // в гостях — чаще, чтобы встреча не прерывалась; иначе раз в ~30 с со случайным сдвигом
    nextBeacon = now + (visiting() ? 8000 + rndInt(4000) : 25000 + rndInt(10000));
    listenUntil = now + 4000;
  }
  if (!type) return false;
  buf[0] = 'B'; buf[1] = 'T'; buf[2] = type;
  put32(buf + 3, s.deviceId);
  put32(buf + 7, type == P_GIFT ? friendId : 0);
  buf[11] = s.form;
  buf[12] = s.trait;
  buf[13] = (uint8_t)s.generation; buf[14] = (uint8_t)(s.generation >> 8);
  buf[15] = (uint8_t)clampi(s.happy, 0, 100);
  len = PKT;
  return true;
}

void Game::loraRx(const uint8_t* b, int len, int rssi) {
  if (!s.loraOn || len < PKT || b[0] != 'B' || b[1] != 'T') return;
  uint32_t from = get32(b + 3), to = get32(b + 7);
  if (from == s.deviceId) return;
  if (to && to != s.deviceId) return;
  if (b[2] >= P_DUEL_INV && b[2] <= P_DUEL_NO) {  // дуэль
    uint32_t val = get32(b + 11);
    switch (b[2]) {
      case P_DUEL_INV:
        if (duelState == 0 && scene == S_HOME && !s.asleep && actionKind == A_NONE) {
          duelState = 2; duelPeer = from; duelSince = now; scene = S_DUEL; sceneStart = now;
          justEvolved = true;  // включить экран
          event("Вызов на дуэль по LoRa!");
        }
        break;
      case P_DUEL_ACC:
        if (duelState == 1 && from == duelPeer) { duelState = 3; duelGoAt = now + val - 200; duelSince = now; }  // ~0,2 с пакет летел
        break;
      case P_DUEL_NO:
        if (duelState == 1 && from == duelPeer) { duelState = 0; goHome(); say("Гость отказался от дуэли", pal::dim); }
        break;
      case P_DUEL_RES:
        if ((duelState == 3 || duelState == 4) && from == duelPeer && !duelTheir) duelTheir = val ? val : 1;
        break;
    }
    return;
  }
  if (b[11] >= FORM_COUNT) return;  // маяк или угощение с неизвестной формой
  bool newVisit = !visiting() || friendId != from;
  friendId = from;
  friendForm = b[11];
  friendTrait = b[12];
  friendGen = (uint16_t)(b[13] | b[14] << 8);
  friendRssi = rssi;
  visitUntil = now + 3 * 60000;  // гость остаётся 3 минуты после последнего сигнала
  if (newVisit) {
    friendX = petX < 120 ? 200 : 40;
    friendTarget = friendX;
    nextBeacon = now + 500 + rndInt(1500);  // ответить своим маяком, чтобы и у друга началась встреча
  }
  if (b[2] == P_GIFT) {
    s.happy = (int16_t)clampi(s.happy + 15, 0, 100);
    sfmt(toastBuf, sizeof toastBuf, "%s угостил печенькой!", formName((Form)friendForm));
    say(toastBuf, pal::pink);
    for (int i = 0; i < 6; i++) spawn(1, petX - 15 + i * 6, 60, (rndInt(20) - 10), -30, 1.5f, pal::pink);
    event(toastBuf);
    dirty = true;
    return;
  }
  if (!newVisit) return;
  // радость от встречи — не чаще раза в полчаса с одним и тем же другом
  int slot = -1, oldest = 0;
  for (int i = 0; i < 8; i++) {
    if (friendSeenId[i] == from) { slot = i; break; }
    if (friendSeenAt[i] < friendSeenAt[oldest]) oldest = i;
  }
  bool recent = slot >= 0 && now - friendSeenAt[slot] < 30 * 60000;
  if (slot < 0) slot = oldest;
  friendSeenId[slot] = from;
  friendSeenAt[slot] = now;
  bool known = false;
  for (uint32_t f : s.friends) if (f == from) { known = true; break; }
  if (!known) {
    s.friends[s.friendsMet % 8] = from;
    s.friendsMet++;
  }
  if (!recent) {
    s.visits++;
    s.happy = (int16_t)clampi(s.happy + 10, 0, 100);
    resolveCall(C_HAPPY);
    char fn[48];
    nickName(from, fn, sizeof fn);
    sfmt(toastBuf, sizeof toastBuf, known ? "В гости: %s" : "Новый друг: %s", fn);
    say(toastBuf, pal::cyan);
    event(toastBuf);
    ledFlashUntil = now + 2000;
    ledFlashColor = 0x00C0FF;
    justEvolved = true;  // показать гостя на экране
    checkAchievements();
    dirty = true;
  }
}

// --- дуэль ---

void Game::duelStart() {
  if (!visiting()) return;
  duelState = 1; duelPeer = friendId; duelSince = now; duelMy = duelTheir = 0;
  duelTx = P_DUEL_INV; duelTxLeft = 3; nextDuelTx = now;
  scene = S_DUEL; sceneStart = now;
}

void Game::duelAnswer(bool yes) {
  if (duelState != 2) return;
  if (!yes) { duelTx = P_DUEL_NO; duelTxLeft = 2; nextDuelTx = now; duelState = 0; goHome(); return; }
  duelDelay = 2500 + rndInt(2000);
  duelTx = P_DUEL_ACC; duelTxLeft = 3; nextDuelTx = now;
  duelGoAt = now + duelDelay;
  duelState = 3; duelSince = now; duelMy = duelTheir = 0;
}

void Game::duelPress() {
  if (duelState != 3) return;
  duelMy = now < duelGoAt ? 9999 : (now - duelGoAt);  // фальстарт — 9999 мс
  if (duelMy == 0) duelMy = 1;
  duelState = 4; duelSince = now;
  duelTx = P_DUEL_RES; duelTxLeft = 4; nextDuelTx = now;
}

void Game::duelTick() {
  if (duelState == 1 && now - duelSince > 15000) { duelState = 0; goHome(); say("Гость не ответил", pal::dim); }
  if (duelState == 2 && now - duelSince > 15000) { duelState = 0; goHome(); }
  if (duelState == 3 && now > duelGoAt + 5000) { duelMy = 9999; duelState = 4; duelSince = now; duelTx = P_DUEL_RES; duelTxLeft = 4; nextDuelTx = now; }
  if (duelState == 4 && duelTheir) {
    duelState = 5; duelSince = now;
    bool win = duelMy < duelTheir;
    addCoins(win ? 20 : 5, false);
    s.happy = (int16_t)clampi(s.happy + (win ? 10 : 3), 0, 100);
    if (win) { burst(120, 60, 16, pal::green); ledFlashUntil = now + 2000; ledFlashColor = 0x20FF40; }
    s.gamesPlayed++;
    if (win) { s.gamesWon++; s.totalWins++; }
    dirty = true;
  }
  if (duelState == 4 && now - duelSince > 12000) { duelState = 0; goHome(); say("Связь с гостем пропала", pal::dim); }
}

void Game::drawDuel(Canvas& c) {
  duelTick();
  char fn[24], b[64];
  nickName(duelPeer, fn, sizeof fn);
  bool go = duelState == 3 && now >= duelGoAt;
  c.clear(duelState == 3 ? (go ? hex(0x14532d) : hex(0x3a1218)) : pal::editor);
  drawTopBar(c, false);
  sfmt(b, sizeof b, "Дуэль · %s", fn);
  c.text(font_ui, 10, 26, b, pal::white);
  float cx = 160, cy = 74;
  switch (duelState) {
    case 1: c.text(font_ui, 120, 60, "Вызов отправлен…", pal::fg, 255, CENTER); c.text(font_small, 120, 82, "жду ответа гостя", pal::dim, 255, CENTER); break;
    case 2:
      c.text(font_ui, 120, 52, "Тебя вызывают на дуэль!", pal::yellow, 255, CENTER);
      c.text(font_small, 120, 76, "кто быстрее нажмёт на зелёный", pal::fg, 255, CENTER);
      c.text(font_small, 120, 96, "долго — принять · коротко — нет", pal::white, 255, CENTER);
      break;
    case 3:
      c.circle(cx, cy, 28, go ? hex(0x22c55e) : hex(0xdc2626));
      c.text(font_big, (int)cx, (int)cy - 12, go ? "ЖМИ!" : "жди", pal::white, 255, CENTER);
      break;
    case 4:
      sfmt(b, sizeof b, duelMy >= 9999 ? "Фальстарт" : "%u мс", duelMy);
      c.text(font_big, (int)cx, (int)cy - 12, b, pal::white, 255, CENTER);
      c.text(font_small, (int)cx, (int)cy + 16, "жду результат гостя…", pal::dim, 255, CENTER);
      break;
    case 5: {
      bool win = duelMy < duelTheir;
      c.text(font_big, 120, 44, win ? "Победа! +20 монет" : "Проигрыш · +5 монет", win ? pal::green : pal::orange, 255, CENTER);
      sfmt(b, sizeof b, "ты: %s", duelMy >= 9999 ? "фальстарт" : "");
      if (duelMy < 9999) sfmt(b, sizeof b, "ты: %u мс", duelMy);
      c.text(font_ui, 120, 72, b, pal::white, 255, CENTER);
      sfmt(b, sizeof b, "гость: %s", duelTheir >= 9999 ? "фальстарт" : "");
      if (duelTheir < 9999) sfmt(b, sizeof b, "гость: %u мс", duelTheir);
      c.text(font_ui, 120, 90, b, pal::fg, 255, CENTER);
      c.text(font_small, 120, 112, "нажми, чтобы выйти", pal::dim, 255, CENTER);
      break;
    }
  }
  if (duelState == 1 || duelState == 3 || duelState == 4) {
    Pose p = basePose();
    p.x = 46; p.y = H - 8; p.lookX = 1; p.scale = 0.8f;
    if (duelState == 3 && !go) { p.eyes = E_WIDE; p.mouth = M_O; }
    drawPet(c, (Form)s.form, p);
  }
}

// гость гуляет по экрану рядом с питомцем
void Game::drawFriend(Canvas& c) {
  if (!visiting()) return;
  uint32_t& last = lastFriendMs;
  float dt = clampf((now - last) * 0.001f, 0, 0.1f);
  last = now;
  if ((now / 3000) != ((now - (uint32_t)(dt * 1000)) / 3000) && rndInt(2) == 0) friendTarget = 35 + rndInt(170);
  float d = friendTarget - friendX;
  if (d > 1.5f || d < -1.5f) {
    friendX += (d > 0 ? 1 : -1) * 22 * dt;
    friendDir = d > 0 ? 1 : -1;
    friendWalk += dt * 9;
  }
  Pose p;
  p.t = now + 777;
  p.x = friendX; p.y = FLOOR; p.dir = friendDir; p.scale = 0.85f;
  p.walk = friendWalk; p.lookX = petX > friendX ? 1.f : -1.f;
  p.mouth = M_SMILE; p.blush = true;
  p.squash = 1 + fsin(now * 0.0035f) * 0.025f;
  drawPet(c, (Form)friendForm, p);
  float top = FLOOR - formHeight((Form)friendForm) * 0.85f - 18;
  int w = Canvas::width(font_mono, "гость") + 8;
  c.roundRect(friendX - w / 2.0f, top, (float)w, 12, 4, pal::cyan, 200);
  c.text(font_mono, (int)friendX, (int)top + 1, "гость", pal::ink, 255, CENTER);
}

}  // namespace pet
