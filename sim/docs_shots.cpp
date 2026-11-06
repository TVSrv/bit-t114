// Воспроизводимые скриншоты *настоящего* графического ядра прошивки 0.41.
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
  g.button(B_SHORT);
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
  return 0;
}
