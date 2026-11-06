// Комнаты по формам питомца (данные — rooms.cpp, создаёт tools/roomconv.py).
#pragma once
#include <stdint.h>

namespace rooms {

// Сцена 240x87. px — индексы (LZ4), pal — 6 палитр по 256 цветов: ночь, рассвет, утро, день, закат, вечер.
// win — стекло окна (у улицы — небо): своя картинка на каждое время, 64 цвета, LZ4.
// blanket — верх одеяла по столбцам с blanketX0 (255 — нет), bed — где спит, screen — мониторы,
// desk — где стоит у стола, outdoor — улица (кровати и стола нет).
struct Room {
  const uint8_t* px;
  uint16_t pxLen;
  const uint16_t* pal;
  uint8_t win[4];
  const uint8_t* winPx[6];
  uint16_t winLen[6];
  const uint16_t* winPal[6];
  uint8_t blanketX0, blanketN;
  const uint8_t* blanket;
  uint8_t bed;
  uint8_t screens;
  uint8_t screen[3][4];
  uint8_t desk;
  bool outdoor;
  uint8_t tree;  // где стоит ёлка на Новый год (и торт, тыква в другие праздники)
};
extern const Room ROOM[11];

// Распаковка LZ4: число байт или -1, если данные битые.
int unpack(const uint8_t* src, int n, uint8_t* dst, int cap);

}  // namespace rooms
