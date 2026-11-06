// Картинки арт-пака для прошивки (данные — в artdata.cpp, создаётся tools/artconv.py).
#pragma once
#include <stdint.h>

namespace artdata {

// Сцена 240x87 между верхней и нижней панелью; индекс — номер цвета в палитре RGB565.
constexpr int SCENE_W = 240, SCENE_H = 87;
// Комнаты — в rooms.cpp (tools/roomconv.py); здесь крыша, персонажи и сцены Джуна в мастерской.
extern const uint8_t* const ROOF_PX[6];   // крыша; 0 — небо (рисуется градиентом по часам)
extern const uint16_t ROOF_PAL[6][256];
constexpr int ROOF_FLOOR_Y = 80, ROOF_TILE = 216;  // с этой строки пол, бесшовный с периодом ROOF_TILE
constexpr int MOON_X = 172, MOON_Y = 0;
// Верх одеяла по столбцам начиная с BLANKET_X0 (255 — одеяла нет).
constexpr int BLANKET_X0 = 160;
extern const uint8_t BLANKET_TOP[];
// Экран терминала: место для «>_» (координаты в сцене).
// Стекло окна в сцене: погода показывается только в нём.
constexpr int WIN_X0 = 185, WIN_Y0 = 7;
constexpr int WIN_X1 = 214, WIN_Y1 = 21;
constexpr int TERM_X = 29, TERM_Y = 33;

// Персонаж: индекс 0 — прозрачно. Глаза и рот — рамки x0,y0,x1,y1 включительно,
// skin — цвет кожи для век, head — верх головы (под шапку), arm и leg — кулаки и ноги
// (левый, правый; пустая рамка — части нет).
struct Sprite {
  uint8_t w, h;
  const uint16_t* pal;
  const uint8_t* px;
  uint8_t eye[2][4];
  uint8_t mouth[4];
  uint16_t skin;
  uint8_t head;
  uint8_t arm[2][4];
  uint8_t leg[2][4];
  uint8_t hatY, hatCut, hatW;  // шапка: строка меха, выше какой строки убор формы скрыт, ширина головы
};
extern const Sprite FORM[11];
extern const Sprite MOON;
// Спящий Джун с панелей «рассвет» и «ночь» (с одеялом и шапкой на полу) — левый верхний угол в сцене.
extern const Sprite SLEEP_DAWN, SLEEP_NIGHT;
constexpr int SLEEP_DAWN_X = 161, SLEEP_DAWN_Y = 24;
constexpr int SLEEP_NIGHT_X = 161, SLEEP_NIGHT_Y = 24;
// кепка Джуна на полу у кровати (во сне); на Новый год вместо неё лежит колпак
extern const Sprite CAP_DAWN, CAP_NIGHT;
constexpr int CAP_DAWN_X = 130, CAP_DAWN_Y = 60;
constexpr int CAP_NIGHT_X = 128, CAP_NIGHT_Y = 62;
// Джун с кофе (утро) и с пиццей (закат): питомец вместе с едой, как на картинке.
// *_DX — где середина питомца относительно левого края, низ спрайта — пол.
extern const Sprite COFFEE, PIZZA, MUG, BOX;
// Джун на стуле за компьютером (вечер) — левый верхний угол в сцене.
extern const Sprite DESK;
constexpr int DESK_X = 64, DESK_Y = 24;
constexpr int COFFEE_DX = 36, PIZZA_DX = 36;
constexpr int COFFEE_Y = 25, PIZZA_Y = 25;  // верх в сцене

}  // namespace artdata
