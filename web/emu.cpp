// Вход для WebAssembly: тот же код игры, что и в прошивке, — для эмулятора на сайте.
#include "../core/game.h"

static gfx::Color fb[gfx::W * gfx::H];
static pet::Game game;
static pet::Save saveBuf;

#define EXPORT(n) extern "C" __attribute__((export_name(n)))

EXPORT("start") void start(uint32_t seed) {
  memset(&game.s, 0, sizeof(game.s));
  game.init(seed);
}
EXPORT("tick") void tick(uint32_t dt) { game.tick(dt); }
EXPORT("button") void button(int b) { game.button((pet::Btn)b); }
EXPORT("frame") const gfx::Color* frame() {
  gfx::Canvas c(fb);
  game.render(c);
  return fb;
}
EXPORT("demo") void demo(int n) { game.demo(n); }
EXPORT("setSpeed") void setSpeed(uint32_t k) { game.speed = k ? k : 1; }
EXPORT("led") uint32_t led() { return game.ledColor(); }
EXPORT("attention") int attention() { return game.attention(); }
EXPORT("wantsGps") int wantsGps() { return game.wantsGps(); }
EXPORT("battery") void battery(int pct, int ch) { game.setBattery(pct, ch); }
EXPORT("gps") void gps(int on, int sats, double lat, double lon, uint32_t utc) {
  pet::GpsFix f;
  f.on = on;
  f.sats = (uint8_t)sats;
  f.alive = on;
  f.inView = (uint8_t)(on ? sats + 4 : 0);
  f.posValid = on && sats >= 4;
  f.timeValid = on && utc != 0;
  f.hdop = 1.2f;
  f.lat = lat;
  f.lon = lon;
  f.utc = utc;
  game.gpsFeed(f);
}
// сохранение в браузере: копия структуры Save
EXPORT("saveData") const pet::Save* saveData() {
  saveBuf = game.s;
  saveBuf.crc = pet::Game::crc32(&saveBuf, offsetof(pet::Save, crc));
  return &saveBuf;
}
EXPORT("saveSize") int saveSize() { return sizeof(pet::Save); }
EXPORT("loadBuf") pet::Save* loadBuf() { return &saveBuf; }
EXPORT("loadData") int loadData() { return game.load(saveBuf); }
EXPORT("stat") int stat(int k) {
  const pet::Save& s = game.s;
  switch (k) {
    case 0: return s.form; case 1: return s.hunger; case 2: return s.happy; case 3: return s.energy;
    case 4: return s.discipline; case 5: return s.weight; case 6: return (int)s.ageMin; case 7: return s.bugs;
    case 8: return s.sick; case 9: return s.mistakes; case 10: return s.alive; case 11: return (int)(s.epoch % 86400);
    case 12: return s.asleep; case 13: return s.lightOff; case 14: return s.generation;
    default: return 0;
  }
}
// портрет формы для схемы эволюции: питомец по центру на заданном фоне
EXPORT("portrait") const gfx::Color* portrait(int form, uint32_t t, uint32_t bg) {
  gfx::Canvas c(fb);
  c.clear(gfx::hex(bg));
  pet::Pose p;
  p.x = 120; p.y = 104; p.t = t;
  p.squash = 1 + gfx::fsin(t * 0.0032f) * 0.025f;
  p.mouth = pet::M_SMILE;
  if ((t / 3000) % 7 == 0 && (t % 3000) < 130) p.blink = 1;
  if (form == pet::GHOST) p.eyes = pet::E_CLOSED;
  pet::drawPet(c, (pet::Form)form, p);
  return fb;
}
