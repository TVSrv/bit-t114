// Снимки экранов: прогоняет сценарии и пишет кадры PPM (для проверки и превью сайта).
#define private public
#include "../core/game.h"
#undef private
#include <cstdio>
#include <cstring>
using namespace pet;
static gfx::Color fb[gfx::W * gfx::H];
static const char* dir;
static void save(Game& g, const char* name) {
  gfx::Canvas c(fb);
  g.render(c);
  char n[256];
  snprintf(n, sizeof n, "%s/%s.ppm", dir, name);
  FILE* o = fopen(n, "wb");
  fprintf(o, "P6 240 135 255\n");
  for (int k = 0; k < gfx::W * gfx::H; k++) {
    gfx::Color v = fb[k];
    unsigned char px[3] = {(unsigned char)((v >> 11) << 3), (unsigned char)(((v >> 5) & 63) << 2), (unsigned char)((v & 31) << 3)};
    fwrite(px, 1, 3, o);
  }
  fclose(o);
}
static void run(Game& g, int ms) {  // с отрисовкой, как на устройстве (~30 к/с)
  gfx::Canvas c(fb);
  for (int t = 0; t < ms; t += 33) { g.tick(33); g.render(c); }
}
#include <set>
#include <string>
static std::set<std::string> overflow;
int main(int, char** argv) {
  dir = argv[1];
  gfx::textOverflow = [](const char* s, int x0, int x1) {
    std::string t = s;
    if (t == "{" || t == "}" || t == "git push origin main") return;  // декорации, уезжают за край нарочно
    char b[160]; snprintf(b, sizeof b, "%s  [%d..%d]", s, x0, x1); overflow.insert(b);
  };
  Game g;
  memset(&g.s, 0, sizeof g.s);
  g.init(7);
  run(g, 20000); save(g, "01_hatch");
  run(g, 42000); run(g, 2000); save(g, "02_evolve_flash"); run(g, 1500); save(g, "03_evolve_banner");
  run(g, 3000); save(g, "04_home_baby");
  g.demo(0); g.s.form = JUNIOR; run(g, 2000); g.button(B_SHORT); run(g, 300); save(g, "05_menu_cursor");
  g.button(B_LONG); run(g, 400); save(g, "06_food_list");
  g.button(B_SHORT); g.button(B_LONG); run(g, 1300); save(g, "07_eat");
  g.demo(1); run(g, 3000); save(g, "08_hungry");
  g.demo(2); run(g, 1500); save(g, "09_sick");
  g.demo(3); run(g, 2500); save(g, "10_sleep");
  g.demo(4); run(g, 1500); save(g, "11_sad_hacker");
  g.demo(5); run(g, 3500); save(g, "12_evolve_arch");
  g.demo(7); run(g, 1500); save(g, "13_ai");
  g.demo(8); run(g, 1500); save(g, "14_walk");
  g.demo(6); run(g, 1500); save(g, "15_bsod"); run(g, 5500); save(g, "16_tomb");
  g.demo(0); g.s.form = MIDDLE; g.s.bugs = 3; run(g, 500);
  for (int i = 0; i < 4; i++) g.button(B_SHORT);
  g.button(B_LONG); run(g, 700); save(g, "17_clean");
  g.demo(0); g.s.form = SENIOR; for (int i = 0; i < 6; i++) g.button(B_SHORT); g.button(B_LONG); run(g, 600); save(g, "18_status");
  g.button(B_SHORT); run(g, 700); save(g, "19_status2");
  g.demo(0); g.s.form = FREELANCER; g.s.discipline = 100; g.button(B_SHORT); g.button(B_SHORT); g.button(B_LONG); g.button(B_LONG); run(g, 900); save(g, "20_game_debug");
  g.demo(0); g.s.form = JUNIOR; g.s.discipline = 100; g.button(B_SHORT); g.button(B_SHORT); g.button(B_LONG); g.button(B_SHORT); g.button(B_LONG); run(g, 600); save(g, "21_game_bit");
  g.button(B_SHORT); run(g, 500); save(g, "22_game_bit_flip");
  g.demo(0); g.s.form = ARCHITECT; for (int i = 0; i < 9; i++) g.button(B_SHORT); g.button(B_LONG); run(g, 400); save(g, "23_settings");
  g.demo(9); run(g, 400); save(g, "24_gps_nolink");
  g.demo(10); run(g, 400); save(g, "25_gps_search");
  g.demo(11); run(g, 400); save(g, "26_gps_fix");
  g.demo(12); run(g, 400); save(g, "27_walk_search");
  g.demo(8); g.gpsFeed([]{ GpsFix f; f.on = true; f.alive = true; f.inView = 15; f.sats = 9; f.posValid = true; f.hdop = 1.1f; return f; }()); run(g, 400); save(g, "28_walk_fix");
  // новые функции
  g.demo(0); g.s.form = MIDDLE; g.s.discipline = 100; g.startGame(G_DEPLOY); run(g, 1800);
  g.button(B_SHORT); run(g, 250); save(g, "29_deploy");
  g.demo(0); g.s.form = JUNIOR; g.startGame(G_PING); run(g, 700); save(g, "30_ping");
  g.demo(0); g.s.form = HACKER; g.startGame(G_COMPILE); run(g, 1500); save(g, "31_compile");
  g.demo(0); g.s.form = ARCHITECT; g.triggerAlarm(); run(g, 900); save(g, "32_alarm");
  g.demo(0); g.s.form = SENIOR; g.s.weight = 52; g.s.ageMin = 13 * 24 * 60;
  g.s.achievements = (1u << ACH_KM1) | (1u << ACH_WEEK) | (1u << ACH_GAMER) | (1u << ACH_COLLECTOR) | (1u << ACH_ADULT);
  run(g, 1500); save(g, "33_old_fat_acc");
  g.demo(0); g.s.form = JUNIOR; g.s.trait = T_GLUTTON; g.s.inherited = 1; g.s.generation = 2; g.s.parentForm = HACKER; g.s.favFood = 2; g.s.favGame = G_PING;
  g.scene = S_STATUS; g.page = 4; g.sceneStart = 0; run(g, 300); save(g, "34_status_trait");
  g.page = 5; run(g, 200); save(g, "35_status_ach");
  g.s.formsSeen = (1 << BABY) | (1 << JUNIOR) | (1 << MIDDLE) | (1 << HACKER) | (1 << SENIOR);
  g.page = 8; run(g, 200); save(g, "36_status_collection");
  g.demo(0); g.s.form = JUNIOR; g.s.discipline = 100; g.s.favGame = G_DEPLOY; g.openList(1); g.listSel = 2; run(g, 400); save(g, "37_games_list");
  // квест, реакция, гость, настройки
  g.demo(0); g.s.form = MIDDLE; g.activate(I_WALK);
  { GpsFix f; f.on = true; f.alive = true; f.posValid = true; f.sats = 9; f.inView = 15; f.hdop = 1; f.lat = 55.75; f.lon = 37.61;
    g.gpsFeed(f); f.lat += 0.00005; g.gpsFeed(f); }
  g.toastUntil = 0; run(g, 600); save(g, "38_walk_quest");
  g.demo(0); g.s.form = JUNIOR; g.startGame(G_REACT); g.rGoAt = g.now + 100000; run(g, 500); save(g, "39_react_wait");
  g.rGoAt = g.now; run(g, 100); save(g, "40_react_go");
  g.demo(0); g.s.form = SENIOR; g.s.loraOn = 1; g.s.deviceId = 1;
  { uint8_t p[16] = {'B', 'T', 1, 9, 9, 9, 9, 0, 0, 0, 0, (uint8_t)HACKER, 0, 2, 0, 70}; g.loraRx(p, 16, -70); }
  run(g, 2500); save(g, "41_friend_visit");
  g.openList(0); g.listSel = 3; run(g, 400); save(g, "42_gift_menu");
  g.demo(0); g.s.form = ARCHITECT; g.openList(2); g.listSel = 9; run(g, 400); save(g, "43_settings_ble");
  g.demo(0); g.s.form = JUNIOR; g.openList(2); g.listSel = 11; run(g, 300); g.button(B_LONG); run(g, 300); save(g, "44_confirm_walks");
  // магазин, шапки, темы, праздники
  g.demo(0); g.s.form = MIDDLE; g.s.coins = 230; g.openList(3); g.listSel = 3; run(g, 300); save(g, "45_shop");
  { const uint8_t hats[] = {IT_BEANIE, IT_CAP, IT_TOPHAT, IT_COWBOY, IT_WIZARD, IT_CATEARS};
    const Form fs[] = {JUNIOR, MIDDLE, SENIOR, FREELANCER, HACKER, BABY};
    for (int i = 0; i < 6; i++) { g.demo(0); g.s.form = fs[i]; g.s.hat = hats[i]; g.s.theme = (uint8_t)(i < 4 ? IT_TH_DRACULA + i : 0);
      run(g, 800); char n[32]; snprintf(n, 32, "46_hat%d", i); save(g, n); } }
  g.s.hat = 0; g.s.theme = 0; g.s.clockSource = 1;
  g.demo(0); g.s.form = JUNIOR; g.s.clockSource = 1; g.s.epoch = daysFrom2000(2026, 12, 31) * 86400u + 12 * 3600; g.toastUntil = 0; run(g, 2500); save(g, "47_newyear");
  g.demo(0); g.s.form = SENIOR; g.s.clockSource = 1; g.s.epoch = daysFrom2000(2026, 9, 13) * 86400u + 12 * 3600; run(g, 800); save(g, "48_progday");
  g.demo(0); g.s.form = HACKER; g.s.clockSource = 1; g.s.epoch = daysFrom2000(2026, 10, 31) * 86400u + 12 * 3600; run(g, 800); save(g, "49_halloween");
  g.s.epoch = daysFrom2000(2026, 11, 4) * 86400u + 12 * 3600;
  { const char* cmds[] = {"погода 3 7", "погода 4 -5", "погода 5 22", "погода 2 12", "погода 6 3", "погода 1 25"};
    for (int i = 0; i < 6; i++) { char o[64]; g.demo(0); g.s.form = JUNIOR; g.command(cmds[i], o, sizeof o); run(g, 1200);
      char n[32]; snprintf(n, 32, "50_weather%d", i); save(g, n); } g.weather = 0; }
  g.demo(0); g.s.form = JUNIOR; g.s.loraOn = 1; g.s.deviceId = 1;
  { uint8_t p[16] = {'B', 'T', 1, 0x42, 0x83, 0xC0, 0x0D, 0, 0, 0, 0, (uint8_t)HACKER, 0, 2, 0, 70}; g.loraRx(p, 16, -70); }
  g.openList(1); g.listSel = 6; run(g, 300); save(g, "51_duel_menu");
  g.listChoose(); run(g, 300); g.duelState = 2; run(g, 300); save(g, "52_duel_ask");
  g.duelState = 5; g.duelMy = 231; g.duelTheir = 318; run(g, 300); save(g, "53_duel_result");
  g.duelState = 0; g.goHome(); g.friendId = 0;
  // итоги всех игр с максимальным счётом — проверка, что заголовки влезают
  { const uint8_t ids[] = {G_DEBUG, G_BIT, G_DEPLOY, G_PING, G_COMPILE, G_REACT};
    for (int i = 0; i < 6; i++) { g.demo(0); g.s.form = JUNIOR; g.startGame(ids[i]); g.gScore = g.gMax; g.gameOver(ids[i], g.gMax, g.gMax);
      run(g, 300); char n[32]; snprintf(n, 32, "54_res%d", i); save(g, n); } g.goHome(); }
  // все страницы статуса (в том числе с длинными строками) — проверка надписей
  g.demo(0); g.s.form = ARCHITECT; g.s.achievements = 0x2AAAA; g.s.coins = 99999; g.s.totalMeters = 123456; g.s.stepsToday = 99999;
  g.s.friendsMet = 99; g.s.visits = 999; g.s.clockSource = 1; g.clockSure = true; g.setBattery(100, false);
  for (int pg = 0; pg < 12; pg++) { g.scene = S_STATUS; g.page = (uint8_t)pg; g.sceneStart = 0; run(g, 200);
    char n[32]; snprintf(n, 32, "55_status%02d", pg); save(g, n); }
  g.setBattery(100, true); g.page = 3; run(g, 200); save(g, "55_status_charge");
  g.goHome();
  // Кадры нового оформления при фиксированных форме, времени и позиции.
  {
    Game art;
    art.init(42);
    art.setBattery(82, false);
    art.clockSure = true;
    art.s.clockSource = 1;
    const uint32_t date = daysFrom2000(2026, 11, 4) * 86400u;
    art.now = 1832;
    art.nextBlink = art.now + 10000;
    art.blinkUntil = 0;
    for (int f = EGG; f < FORM_COUNT; ++f) {
      art.demo(0);
      art.s.form = (Form)f;
      art.s.epoch = date + 12 * 3600;
      art.s.weight = baseWeight((Form)f);
      art.petX = art.petTarget = 121;
      art.nextWander = art.now + 10000;
      if (f == EGG) {
        art.scene = S_HATCH;
        art.s.stageMin = 24;
      }
      char n[32];
      snprintf(n, sizeof n, "60_form_%02d", f);
      save(art, n);
    }
    const int hours[] = {6, 9, 12, 18, 20, 23};
    for (int h : hours) {
      art.demo(0);
      art.s.form = JUNIOR;
      art.s.epoch = date + h * 3600;
      art.s.asleep = art.s.lightOff = art.sleepTime();
      art.petX = art.petTarget = 121;
      char n[32];
      snprintf(n, sizeof n, "61_room_%02d", h);
      save(art, n);
      art.demo(8);
      art.s.form = HACKER;
      art.fix.alive = true;
      art.fix.inView = 11;
      art.fix.heard = 7;
      snprintf(n, sizeof n, "62_walk_%02d", h);
      save(art, n);
    }
    art.demo(0);
    art.s.form = JUNIOR;
    art.s.epoch = date + 18 * 3600;
    art.petX = art.petTarget = 106;
    art.actionKind = A_EAT;
    art.actionArg = 1;
    art.now += 850;
    art.actionStart = art.now - 850;
    save(art, "63_feeding");
    art.demo(0);
    art.s.form = JUNIOR;
    art.s.epoch = date + 9 * 3600;
    art.petX = art.petTarget = 110;
    art.actionKind = A_EAT;
    art.actionArg = 0;
    art.now += 900;
    art.actionStart = art.now - 900;
    save(art, "64_coffee");
    // вечером за компьютером: Джун — кусок картинки, Сеньор — стоя у стола
    const Form coders[] = {JUNIOR, SENIOR};
    for (int i = 0; i < 2; i++) {
      art.demo(0);
      art.s.form = coders[i];
      art.s.epoch = date + 20 * 3600;
      art.petX = art.petTarget = 92;
      art.nextDesk = 0;
      for (int k = 0; k < 40; k++) save(art, i ? "66_desk_1" : "66_desk_0");
    }
    art.demo(0);
    art.s.form = MIDDLE;
    art.s.epoch = date + 20 * 3600;
    art.petX = art.petTarget = 120;
    art.actionKind = A_EAT;
    art.actionArg = 0;
    art.now += 1200;
    art.actionStart = art.now - 1200;
    save(art, "67_coffee_hand");
    // Новый год: каждая форма в своей комнате, ёлка в свободном углу, колпак вместо убора
    for (int f = BABY; f < GHOST; f++) {
      art.demo(0);
      art.s.form = f;
      art.s.epoch = daysFrom2000(2026, 12, 31) * 86400u + 12 * 3600;
      art.petX = art.petTarget = 130;
      art.nextWander = art.now + 100000;
      char n[32];
      snprintf(n, sizeof n, "68_newyear_%02d", f);
      save(art, n);
    }
    const Form nySleep[] = {JUNIOR, MIDDLE};
    for (int i = 0; i < 2; i++) {
      art.demo(0);
      art.s.form = nySleep[i];
      art.s.epoch = daysFrom2000(2026, 12, 31) * 86400u + 23 * 3600;
      art.s.asleep = true;
      char n[32];
      snprintf(n, sizeof n, "69_newyear_sleep_%d", i);
      save(art, n);
    }
    const Form sleepers[] = {MIDDLE, FREELANCER, ARCHITECT, HACKER};
    for (int i = 0; i < 4; i++) {
      art.demo(0);
      art.s.form = sleepers[i];
      art.s.epoch = date + 23 * 3600;
      art.s.asleep = true;
      art.s.lightOff = i & 1;
      char n[32];
      snprintf(n, sizeof n, "65_sleep_%d", i);
      save(art, n);
    }
  }
  // все сообщения из кода — на главном экране
  static const char* toasts[] = {
#include "toasts.inc"
  };
  for (const char* t : toasts) { g.demo(0); g.say(t, pal::green); run(g, 300); }
  for (auto& o : overflow) printf("НЕ ВЛЕЗАЕТ: %s\n", o.c_str());
  return 0;
}
