// Мост к ядру игры: телефон рисует экран платы тем же кодом, что и прошивка.
#include <jni.h>
#include <string.h>
#include <mutex>
#include "game.h"

static gfx::Color fb[gfx::W * gfx::H];
static pet::Game game;
static bool ready = false;
// Применение BLE-снимков и рендер выполняются в разных потоках Android.
static std::mutex renderMutex;

static void ensure() {
  if (ready) return;
  memset(&game.s, 0, sizeof(game.s));
  game.init(12345);
  game.speed = 0;
  ready = true;
}

extern "C" JNIEXPORT void JNICALL Java_ru_bitpet_app_PetRenderer_nativeApply(JNIEnv* env, jobject, jintArray fields) {
  std::lock_guard<std::mutex> lock(renderMutex);
  ensure();
  jsize n = env->GetArrayLength(fields);
  jint* v = env->GetIntArrayElements(fields, nullptr);
  game.mirror((const int32_t*)v, (int)n);
  env->ReleaseIntArrayElements(fields, v, JNI_ABORT);
}

extern "C" JNIEXPORT void JNICALL Java_ru_bitpet_app_PetRenderer_nativeFrame(JNIEnv* env, jobject, jint dtMs, jintArray out) {
  std::lock_guard<std::mutex> lock(renderMutex);
  ensure();
  game.tick((uint32_t)dtMs);
  gfx::Canvas c(fb);
  game.render(c);
  jint* p = env->GetIntArrayElements(out, nullptr);
  for (int i = 0; i < gfx::W * gfx::H; i++) {
    uint16_t v = fb[i];
    uint32_t r = (v >> 11) << 3, g = ((v >> 5) & 63) << 2, b = (v & 31) << 3;
    r |= r >> 5; g |= g >> 6; b |= b >> 5;
    p[i] = (jint)(0xFF000000u | r << 16 | g << 8 | b);
  }
  env->ReleaseIntArrayElements(out, p, 0);
}

// портрет формы для альбома: питомец по центру на фоне, квадрат size×size
extern "C" JNIEXPORT void JNICALL Java_ru_bitpet_app_PetRenderer_nativePortrait(JNIEnv* env, jobject, jint form, jint bg, jint size, jintArray out) {
  std::lock_guard<std::mutex> lock(renderMutex);
  gfx::Canvas c(fb);
  c.clear(gfx::rgb((bg >> 16) & 255, (bg >> 8) & 255, bg & 255));
  pet::Pose p;
  p.x = 120; p.y = 104; p.t = 1500; p.mouth = pet::M_SMILE; p.blush = true;
  pet::drawPet(c, (pet::Form)form, p);
  jint* o = env->GetIntArrayElements(out, nullptr);
  int x0 = 120 - size / 2, y0 = 112 - size;  // низ квадрата чуть ниже пола
  for (int y = 0; y < size; y++)
    for (int x = 0; x < size; x++) {
      int sx = x0 + x, sy = y0 + y;
      uint16_t v = (sx >= 0 && sx < gfx::W && sy >= 0 && sy < gfx::H) ? fb[sy * gfx::W + sx] : 0;
      uint32_t r = (v >> 11) << 3, g = ((v >> 5) & 63) << 2, b = (v & 31) << 3;
      o[y * size + x] = (jint)(0xFF000000u | (r | r >> 5) << 16 | (g | g >> 6) << 8 | (b | b >> 5));
    }
  env->ReleaseIntArrayElements(out, o, 0);
}
