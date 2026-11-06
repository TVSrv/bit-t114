// Проверка разбора NMEA: контрольные суммы, GSV по системам, GGA, RMC, время.
#include "../core/gps.h"
#include <cstdio>
#include <cstring>
#include <cstdio>
using namespace pet;

static int fails = 0;
static void check(bool ok, const char* what) { printf("%s %s\n", ok ? "ок  " : "ОШИБКА", what); if (!ok) fails++; }

static void line(Nmea& n, const char* body, bool badSum = false) {
  unsigned char x = 0;
  for (const char* p = body; *p; p++) x ^= (unsigned char)*p;
  char s[128];
  snprintf(s, sizeof s, "$%s*%02X\r\n", body, badSum ? (x ^ 1) : x);
  for (const char* p = s; *p; p++) n.feed(*p);
}

int main() {
  Nmea n;
  // до привязки: спутники видны, координат нет
  line(n, "GPGSV,3,1,10,02,45,120,30,05,30,060,25,07,70,300,35,09,15,200,20");
  line(n, "BDGSV,2,1,06,01,40,140,28,03,50,180,31,06,20,100,18,08,60,260,33");
  line(n, "GLGSV,1,1,03,65,30,050,22,72,55,240,29,80,10,330,15");
  check(n.fix.inView == 19, "видно 10 GPS + 6 BeiDou + 3 ГЛОНАСС = 19");
  check(n.fix.heard == 3 && n.fix.bestSnr == 29, "неполные наборы GSV не считаются: слышно 3 (ГЛОНАСС), лучший 29 дБ");
  Nmea q;
  line(q, "GPGSV,2,1,08,02,45,120,,05,30,060,,07,70,300,,09,15,200,");
  line(q, "GPGSV,2,2,08,11,40,100,,13,20,220,,15,60,010,,17,10,330,");
  check(q.fix.inView == 8 && q.fix.heard == 0, "в помещении: видно 8 по альманаху, слышно 0");
  line(q, "GPGSV,2,1,08,02,45,120,18,05,30,060,,07,70,300,22,09,15,200,");
  line(q, "GPGSV,2,2,08,11,40,100,,13,20,220,,15,60,010,,17,10,330,");
  check(q.fix.heard == 2 && q.fix.bestSnr == 22, "сообщения из двух частей складываются: слышно 2, лучший 22");
  line(n, "GNGGA,083015.000,,,,,0,00,25.5,,,,,,");
  check(n.fix.sats == 0, "GGA без привязки: в работе 0");
  line(n, "GNRMC,083015.000,V,,,,,,,031126,,,N");
  check(!n.fix.posValid, "RMC статус V: координат нет");
  check(!n.fix.timeValid, "без привязки время модуля не принимается");
  Nmea r;
  line(r, "GNRMC,001500.000,V,,,,,,,031126,,,N");
  check(!r.fix.timeValid, "внутренние часы модуля (00:15 после включения) отброшены");
  // привязка
  line(n, "GNGGA,083120.000,5545.3480,N,03737.0380,E,1,08,1.2,152.0,M,14.0,M,,");
  check(n.fix.sats == 8 && n.fix.hdop > 1.19f && n.fix.hdop < 1.21f, "GGA: в работе 8, HDOP 1.2");
  line(n, "GNRMC,083120.000,A,5545.3480,N,03737.0380,E,0.50,90.0,031126,,,A");
  check(n.fix.posValid, "RMC статус A: координаты есть");
  check(!n.fix.timeValid, "одного сообщения мало — время ещё не принято");
  line(n, "GNRMC,083121.000,A,5545.3480,N,03737.0380,E,0.50,90.0,031126,,,A");
  line(n, "GNRMC,083122.000,A,5545.3480,N,03737.0380,E,0.50,90.0,031126,,,A");
  check(n.fix.timeValid && n.fix.utc == daysFrom2000(2026, 11, 3) * 86400u + 8 * 3600 + 31 * 60 + 22, "три согласованных сообщения — время 08:31:22 UTC принято");
  Nmea z;  // «A» при 0 спутниках в работе — внутренние часы модуля, как 03:03:03 на плате
  line(z, "GNGGA,000303.000,,,,,0,00,25.5,,,,,,");
  for (int i = 3; i < 8; i++) { char b[80]; snprintf(b, sizeof b, "GNRMC,00030%d.000,A,,,,,,,031126,,,N", i); line(z, b); }
  check(!z.fix.timeValid, "«A» при 0 спутниках в работе — время не принимается");
  check(n.fix.lat > 55.7557 && n.fix.lat < 55.7559 && n.fix.lon > 37.6172 && n.fix.lon < 37.6174, "55°45.348' N 37°37.038' E");
  // испорченная строка не меняет данные
  line(n, "GPGSV,1,1,01,02,45,120,30", true);
  check(n.fix.inView == 19, "строка с неверной суммой отброшена");
  // спутников стало меньше
  line(n, "GPGSV,2,1,05,02,45,120,30,05,30,060,25,07,70,300,35,09,15,200,20");
  check(n.fix.inView == 14, "GPS стало 5: всего 14");
  // южное и западное полушария
  line(n, "GNRMC,000000.000,A,3352.0000,S,15112.0000,W,0.0,0.0,010126,,,A");
  check(n.fix.lat < -33.86 && n.fix.lon < -151.19, "S и W дают минус");
  // дата по умолчанию до привязки (старый год) не принимается за время
  Nmea m;
  line(m, "GNRMC,000012.000,V,,,,,,,060180,,,N");
  check(!m.fix.timeValid, "дата 1980 года не считается временем");
  printf(fails ? "\nОшибок: %d\n" : "\nВсё верно\n", fails);
  return fails != 0;
}
