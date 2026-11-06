// Экран ST7789 240x135 на T114: инициализация как в Meshtastic (MADCTL MV|MY, инверсия,
// окно со смещением 40/52 внутри 320x240), вывод кадра через SPIM3 на 32 МГц.
#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "gfx.h"

class Display {
 public:
  void begin(bool flip) {
    pinMode(TFT_PWR, OUTPUT);
    digitalWrite(TFT_PWR, LOW);  // питание панели
    backlight(0);  // подсветка выкл, пока не нарисуем
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_RST, OUTPUT);
    SPI.begin();
    delay(10);
    digitalWrite(TFT_RST, HIGH); delay(5);
    digitalWrite(TFT_RST, LOW); delay(20);
    digitalWrite(TFT_RST, HIGH); delay(120);
    cmd(0x01); delay(150);         // программный сброс
    cmd(0x11); delay(120);         // выход из сна
    cmd(0x3A); data(0x55);         // 16 бит на пиксель
    setFlip(flip);
    clearAll();                    // вся память контроллера — чёрным, чтобы по краям не было мусора
    cmd(0x21);                     // инверсия (панель IPS)
    cmd(0x13); delay(10);          // нормальный режим
    cmd(0x29); delay(10);          // экран вкл
    on = true;
  }

  void setFlip(bool f) {
    flipped = f;
    cmd(0x36);
    data(f ? 0x60 : 0xA0);         // MV|MX или MV|MY
  }

  // залить чёрным всю память ST7789 (320x240), а не только видимое окно
  void clearAll() {
    cmd(0x2A); data16(0, 319);
    cmd(0x2B); data16(0, 239);
    cmd(0x2C);
    SPI.beginTransaction(SPISettings(32000000, MSBFIRST, SPI_MODE0));
    digitalWrite(TFT_DC, HIGH);
    digitalWrite(TFT_CS, LOW);
    memset(buf[0], 0, sizeof(buf[0]));
    for (int i = 0; i < 320 * 240; i += CHUNK) SPI.transfer(buf[0], nullptr, CHUNK * 2);
    digitalWrite(TFT_CS, HIGH);
    SPI.endTransaction();
  }

  void push(const gfx::Color* fb) {
    window(0, 0, gfx::W, gfx::H);
    SPI.beginTransaction(SPISettings(32000000, MSBFIRST, SPI_MODE0));
    digitalWrite(TFT_DC, HIGH);
    digitalWrite(TFT_CS, LOW);
    const int N = gfx::W * gfx::H;
    for (int i = 0; i < N; i += CHUNK) {
      int n = N - i < CHUNK ? N - i : CHUNK;
      uint16_t* out = buf[(i / CHUNK) & 1];
      for (int k = 0; k < n; k++) out[k] = __builtin_bswap16(fb[i + k]);
      SPI.transfer(out, nullptr, n * 2);
    }
    digitalWrite(TFT_CS, HIGH);
    SPI.endTransaction();
  }

  // Подсветка на отдельном модуле PWM3, который всегда включён: библиотека NeoPixel ищет
  // свободный (выключенный) PWM и раньше могла забрать модуль подсветки — экран не загорался.
  void backlight(int level) {      // 0..255
    static bool init = false;
    if (!init) {
      NRF_PWM3->PSEL.OUT[0] = TFT_BL;  // P0.15
      NRF_PWM3->PSEL.OUT[1] = 0xFFFFFFFF;
      NRF_PWM3->PSEL.OUT[2] = 0xFFFFFFFF;
      NRF_PWM3->PSEL.OUT[3] = 0xFFFFFFFF;
      NRF_PWM3->MODE = PWM_MODE_UPDOWN_Up;
      NRF_PWM3->PRESCALER = PWM_PRESCALER_PRESCALER_DIV_1;  // 16 МГц / 1000 = 16 кГц
      NRF_PWM3->COUNTERTOP = 1000;
      NRF_PWM3->LOOP = 0;
      NRF_PWM3->DECODER = (PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos) | (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);
      NRF_PWM3->SEQ[0].PTR = (uint32_t)&blSeq;
      NRF_PWM3->SEQ[0].CNT = 1;
      NRF_PWM3->SEQ[0].REFRESH = 0;
      NRF_PWM3->SEQ[0].ENDDELAY = 0;
      NRF_PWM3->ENABLE = 1;
      init = true;
    }
    level = level < 0 ? 0 : (level > 255 ? 255 : level);
    // старший бит 0 — доля низкого уровня равна значению (как inverted в HardwarePWM Adafruit);
    // подсветка горит при низком уровне, поэтому значение = яркость
    blSeq = (uint16_t)(level * 1000 / 255);
    NRF_PWM3->TASKS_SEQSTART[0] = 1;
  }

  void sleep() {
    backlight(0);
    cmd(0x28);
    cmd(0x10);
    on = false;
  }

  // полная инициализация заново: надёжнее, чем просто выход из сна
  void wake() {
    begin(flipped);
  }

  bool on = false;

 private:
  static constexpr int CHUNK = 1024;
  static inline uint16_t blSeq = 0;
  uint16_t buf[2][CHUNK];
  bool flipped = false;

  void cmd(uint8_t c) {
    SPI.beginTransaction(SPISettings(32000000, MSBFIRST, SPI_MODE0));
    digitalWrite(TFT_DC, LOW);
    digitalWrite(TFT_CS, LOW);
    SPI.transfer(c);
    digitalWrite(TFT_CS, HIGH);
    SPI.endTransaction();
  }
  void data(uint8_t d) {
    SPI.beginTransaction(SPISettings(32000000, MSBFIRST, SPI_MODE0));
    digitalWrite(TFT_DC, HIGH);
    digitalWrite(TFT_CS, LOW);
    SPI.transfer(d);
    digitalWrite(TFT_CS, HIGH);
    SPI.endTransaction();
  }
  void data16(uint16_t a, uint16_t b) {
    SPI.beginTransaction(SPISettings(32000000, MSBFIRST, SPI_MODE0));
    digitalWrite(TFT_DC, HIGH);
    digitalWrite(TFT_CS, LOW);
    uint8_t d[4] = {(uint8_t)(a >> 8), (uint8_t)a, (uint8_t)(b >> 8), (uint8_t)b};
    SPI.transfer(d, nullptr, 4);
    digitalWrite(TFT_CS, HIGH);
    SPI.endTransaction();
  }
  void window(int x, int y, int w, int h) {
    // панель 135 строк стоит не по центру 240: в обычном положении с 52-й строки,
    // перевёрнутая — с 53-й (так же у TFT_eSPI для этих экранов)
    x += 40;
    y += flipped ? 53 : 52;
    cmd(0x2A); data16(x, x + w - 1);
    cmd(0x2B); data16(y, y + h - 1);
    cmd(0x2C);
  }
};
