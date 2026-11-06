// Разбор NMEA от GPS (L76K на T114): время, дата, координаты, спутники.
#pragma once
#include <stdint.h>

namespace pet {

struct GpsFix {
  bool on = false;        // приёмник включён
  bool alive = false;     // от модуля идут данные (последние 3 с)
  uint8_t inView = 0;     // спутников видно (все системы, из GSV)
  uint8_t heard = 0;      // из них принимается сигнал (SNR > 0)
  uint8_t bestSnr = 0;    // лучший сигнал, дБ·Гц
  bool timeValid = false;
  bool posValid = false;
  uint8_t sats = 0;
  float hdop = 99;
  double lat = 0, lon = 0;
  uint32_t utc = 0;       // секунды с 2000-01-01 UTC
  float speedKmh = 0;     // скорость по RMC
  float course = 0;       // курс по RMC, градусы от севера
  bool courseValid = false;  // курс надёжен (идём быстрее 2 км/ч)
  uint32_t moduleUp = 0;  // сколько секунд модуль под питанием (заполняет платформа)
};

class Nmea {
 public:
  GpsFix fix;
  bool feed(char c);  // true — пришла законченная строка с данными
 private:
  char buf[96];
  uint8_t len = 0;
  uint8_t view[4] = {0, 0, 0, 0};  // GPS, ГЛОНАСС, BeiDou, Galileo
  uint8_t heard[4] = {0, 0, 0, 0}, heardAcc[4] = {0, 0, 0, 0};
  uint8_t snr[4] = {0, 0, 0, 0}, snrAcc[4] = {0, 0, 0, 0};
  uint8_t ggaQuality = 0, ggaSats = 0;  // из последнего GGA: есть ли привязка и сколько спутников в работе
  uint8_t goodRmc = 0;                  // подряд согласованных RMC
  uint32_t lastRmcUtc = 0;
  bool parse();
};

uint32_t daysFrom2000(int y, int m, int d);

}  // namespace pet
