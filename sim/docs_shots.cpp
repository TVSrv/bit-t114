// Воспроизводимые скриншоты *настоящего* графического ядра прошивки 0.43.
// Никакого концепт-арта: каждый PPM рендерится Game::render(Canvas).
#define private public
#include "../core/game.h"
#undef private
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace pet;

static gfx::Color framebuffer[gfx::W * gfx::H];

static void shot(Game& g, const char* dir, const char* name) {
  gfx::Canvas c(framebuffer);
  g.render(c);
  char filename[512];
  snprintf(filename, sizeof filename, "%s/%s.ppm", dir, name);
  FILE* out = fopen(filename, "wb");
  if (!out) { perror(filename); std::exit(1); }
  fprintf(out, "P6\n%d %d\n255\n", gfx::W, gfx::H);
  for (int i = 0; i < gfx::W * gfx::H; ++i) {
    const gfx::Color rgb = framebuffer[i];
    const unsigned char pix[3] = {
      (unsigned char)((rgb >> 11) << 3),
      (unsigned char)(((rgb >> 5) & 63) << 2),
      (unsigned char)((rgb & 31) << 3)
    };
    if (fwrite(pix, 1, 3, out) != 3) { perror("write"); std::exit(1); }
  }
  fclose(out);
  printf("%s\n", filename);
}
static void prepare(Game& g, Form form, int hour, bool sleeping = false) {
  g.demo(0);
  g.s.form = (uint8_t)form;
  g.s.epoch = daysFrom2000(2026, 11, 6) * 86400u + (uint32_t)hour * 3600u;
  g.s.tz = 0;
  g.s.clockSource = 1;
  g.s.asleep = sleeping ? 1 : 0;
  g.s.lightOff = sleeping ? 1 : 0;
  g.s.hat = 0;
  g.s.achievements = 0;
  g.s.ageMin = 0;  // чистое состояние: праздники и награды не наследуются от предыдущего снимка
  g.setBattery(82, false);
  g.clockSure = true;
  g.s.hunger = 84;
  g.s.happy = 76;
  g.s.energy = 91;
  g.petX = g.petTarget = 120;
  g.nextWander = g.now + 100000;
  g.nextBlink = g.now + 100000;
  g.blinkUntil = 0;
  g.toastUntil = 0;
  g.scene = S_HOME;
  g.menuOpen = false;
  g.cursor = -1;
  g.menuTransitionAt = g.now - Game::MENU_ANIM_MS;
  g.cursorMs = g.now;
}
int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: docs_shots <directory>\n"); return 2; }
  Game g;
  memset(&g.s, 0, sizeof g.s);
  g.init(410041);
  g.speed = 0;
  g.tick(600);
  const char* dir = argv[1];

  prepare(g, JUNIOR, 12);
  shot(g, dir, "01-home-fullscreen");
  g.button(B_LONG);
  g.tick(120);
  shot(g, dir, "02-menu-sliding");
  g.tick(125);
  shot(g, dir, "03-menu-open");
  g.button(B_SHORT); g.button(B_SHORT); g.button(B_SHORT);
  shot(g, dir, "04-menu-selection");
  g.button(B_HOLD);
  g.tick(260);

  prepare(g, MIDDLE, 18);
  shot(g, dir, "05-middle-evening");
  prepare(g, HACKER, 12);
  shot(g, dir, "06-hacker-day");
  prepare(g, JUNIOR, 23, true);
  shot(g, dir, "07-junior-sleep");
  prepare(g, SENIOR, 12);
  g.s.bugs = 2;
  shot(g, dir, "08-senior-bugs");

  // Настоящая графика нового экранного режима: 1-й клик из сна -> часы,
  // 2-й клик -> питомец. Проверяем обе стороны последовательности.
  prepare(g, JUNIOR, 12);
  g.tempC = 7;
  g.weather = 2;  // облачно: погода из подключенного Android
  g.weatherUntil = g.now + 3600000u;
  // Демонстрационные значения взяты только для графического теста.
  // На реальном устройстве их заменяет Android/Open-Meteo.
  g.humidityPct = 76; g.windMs = 3; g.feelsC = 5;
  g.pressureMm = 754; g.precipitationPct = 10;
  g.weatherExtraUntil = g.now + 3600000u;
  const uint8_t hours[6] = {12, 15, 18, 21, 0, 3};
  const int8_t temps[6] = {7, 8, 6, 5, 4, 4};
  const uint8_t kinds[6] = {2, 1, 2, 1, 1, 2};
  g.forecastCount = 6;
  for (int i = 0; i < 6; i++) {
    g.forecast[i].hour = hours[i];
    g.forecast[i].temp = temps[i];
    g.forecast[i].kind = kinds[i];
  }
  g.wakeToInfo();
  shot(g, dir, "09-clock-weather-after-wake");
  g.button(B_SHORT);
  shot(g, dir, "10-pet-after-second-press");
  g.button(B_SHORT);
  if (g.infoScreen) { fprintf(stderr, "unexpected weather screen after pet\n"); return 1; }

  // Настоящие кадры всех шапок, наград, праздников и девяти форм.
  static const uint8_t hats[] = {1, 2, 3, 4, 5, 6, 20};
  static const char* hatNames[] = {
    "11-hat-beanie", "12-hat-cap", "13-hat-tophat", "14-hat-cowboy",
    "15-hat-wizard", "16-hat-catears", "17-hat-newyear"
  };
  for (int i = 0; i < 7; ++i) {
    prepare(g, (Form)(BABY + i), 12);
    g.s.hat = hats[i] == 20 ? 0 : hats[i];
    if (hats[i] == 20)
      g.s.epoch = daysFrom2000(2026, 12, 31) * 86400u + 12 * 3600u;
    shot(g, dir, hatNames[i]);
  }
  static const int bits[] = {ACH_KM1, ACH_WEEK, ACH_GAMER, ACH_COLLECTOR};
  static const char* badgeNames[] = {
    "18-status-kilometer", "19-status-week",
    "20-status-gamer", "21-status-collector"
  };
  for (int i = 0; i < 4; ++i) {
    prepare(g, (Form)(BABY + i), 12);
    g.s.achievements = (uint32_t)1 << bits[i];
    g.scene = S_STATUS; g.page = (uint8_t)(5 + bits[i] / 3);
    shot(g, dir, badgeNames[i]);
  }
  static const char* holidays[] = {
    "22-holiday-tree", "23-holiday-programmer",
    "24-holiday-pumpkin", "25-holiday-birthday"
  };
  const int hm[] = {12, 9, 10, 11}, hd[] = {31, 13, 31, 6};
  for (int i = 0; i < 4; ++i) {
    prepare(g, JUNIOR, 12);
    g.s.epoch = daysFrom2000(2026, hm[i], hd[i]) * 86400u + 12 * 3600u;
    if (i == 3) g.s.ageMin = 7 * 1440;
    shot(g, dir, holidays[i]);
  }
  static const char* allNames[] = {
    "26-baby-all", "27-junior-all", "28-middle-all",
    "29-freelancer-all", "30-senior-all", "31-architect-all",
    "32-hacker-all", "33-legacy-all", "34-ai-all"
  };
  for (int i = 0; i < 9; ++i) {
    prepare(g, (Form)(BABY + i), 12);
    g.s.achievements = ((uint32_t)1 << ACH_KM1) |
                       ((uint32_t)1 << ACH_WEEK) |
                       ((uint32_t)1 << ACH_GAMER) |
                       ((uint32_t)1 << ACH_COLLECTOR);
    g.s.hat = i % 2 == 0 ? 0 : (i % 6 + 1);
    shot(g, dir, allNames[i]);
  }
  // Несколько поз реальной ходьбы: контакт ног с полом в разных формах,
  // без фиктивного подъёма всего туловища.
  static const Form walkingForms[] = {JUNIOR, JUNIOR, MIDDLE, HACKER, LEGACY};
  static const char* walkingNames[] = {
    "35-junior-walk-stance", "36-junior-walk-step",
    "37-middle-walk", "38-hacker-walk", "39-legacy-walk"
  };
  for (int i = 0; i < 5; ++i) {
    prepare(g, walkingForms[i], 12);
    g.petTarget = 182;  // принудительно идёт по комнате, не стоит
    g.walkPh = i == 0 ? 0 : 1.5707963f;
    g.lastPetMs = g.now - 30;
    shot(g, dir, walkingNames[i]);
  }
  // Витрина наград без достижений, с полным набором и на последней странице.
  prepare(g, JUNIOR, 12);
  g.scene = S_STATUS; g.page = 5;
  g.s.achievements = 0;
  shot(g, dir, "40-status-awards-locked");
  g.s.achievements = (1u << ACH_COUNT) - 1u;
  shot(g, dir, "41-status-awards-unlocked");
  g.page = 10;
  shot(g, dir, "42-status-awards-final-page");
  // Разные фазы бегущего текста на экранах мебели — проверка рамки.
  prepare(g, JUNIOR, 12);
  g.now = 3200; g.deskUntil = 0;
  shot(g, dir, "43-monitor-junior-idle");
  g.now = 5400; g.deskUntil = g.now + 25000;
  shot(g, dir, "44-monitor-junior-typing");
  prepare(g, HACKER, 12);
  g.now = 9200; g.deskUntil = g.now + 25000;
  shot(g, dir, "45-monitor-hacker-typing");
  prepare(g, FREELANCER, 12);
  g.now = 16100; g.deskUntil = 0;
  shot(g, dir, "46-monitor-freelancer-idle");
  // Время и текущая погода используют весь экран; батарея лишь на питомце.
  prepare(g, JUNIOR, 12);
  g.setBattery(17, false);
  shot(g, dir, "47-pet-battery-percent");
  g.wakeToInfo();
  g.weather = 0; g.weatherUntil = 0;
  g.weatherExtraUntil = 0; g.forecastCount = 0;
  shot(g, dir, "48-info-weather-no-data");

  // Visually inspect redesigned unified status header with battery at 3 levels.
  prepare(g, JUNIOR, 12);
  g.s.hunger = 94; g.s.happy = 78; g.s.energy = 61; g.s.discipline = 85;
  g.setBattery(87, false);
  shot(g, dir, "49-hud-unified-healthy");
  g.setBattery(19, false);
  shot(g, dir, "50-hud-unified-low-battery");
  g.setBattery(100, true);
  shot(g, dir, "51-hud-unified-charging");
  g.s.hunger = 12; g.s.happy = 25; g.s.energy = 15; g.s.discipline = 40;
  g.s.callKind = C_HUNGER; g.setBattery(7, false);
  shot(g, dir, "52-hud-unified-alert");
  return 0;
}
