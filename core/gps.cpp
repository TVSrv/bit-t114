#include "gps.h"

namespace pet {

uint32_t daysFrom2000(int y, int m, int d) {
  static const uint16_t cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  uint32_t days = 0;
  for (int yy = 2000; yy < y; yy++) days += (yy % 4 == 0) ? 366 : 365;
  days += cum[(m - 1) % 12] + (d - 1);
  if (m > 2 && y % 4 == 0) days++;
  return days;
}

static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : (c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1); }

static double num(const char* s, const char* e) {
  double v = 0, k = 0;
  bool neg = false;
  for (; s < e; s++) {
    if (*s == '-') neg = true;
    else if (*s == '.') k = 1;
    else if (*s >= '0' && *s <= '9') {
      if (k > 0) { k *= 0.1; v += (*s - '0') * k; }
      else v = v * 10 + (*s - '0');
    }
  }
  return neg ? -v : v;
}

static double coord(const char* s, const char* e, char hemi) {
  if (s == e) return 0;
  double v = num(s, e);
  int deg = (int)(v / 100);
  double r = deg + (v - deg * 100) / 60.0;
  return (hemi == 'S' || hemi == 'W') ? -r : r;
}

bool Nmea::feed(char c) {
  if (c == '$') { len = 0; buf[len++] = c; return false; }
  if (len == 0) return false;
  if (c == '\r' || c == '\n') {
    buf[len] = 0;
    bool ok = parse();
    len = 0;
    return ok;
  }
  if (len < sizeof(buf) - 1) buf[len++] = c;
  else len = 0;
  return false;
}

bool Nmea::parse() {
  // контрольная сумма
  int star = -1;
  for (int i = 1; i < len; i++) if (buf[i] == '*') { star = i; break; }
  if (star < 0 || star + 2 >= len) return false;
  uint8_t sum = 0;
  for (int i = 1; i < star; i++) sum ^= (uint8_t)buf[i];
  int h1 = hexv(buf[star + 1]), h2 = hexv(buf[star + 2]);
  if (h1 < 0 || h2 < 0 || sum != (h1 << 4 | h2)) return false;
  // поля
  const char* f[20];
  const char* fe[20];
  int n = 0;
  const char* p = buf + 1;
  f[0] = p;
  for (const char* q = p; q <= buf + star; q++) {
    if (*q == ',' || q == buf + star) {
      fe[n] = q;
      if (++n >= 20) break;
      f[n] = q + 1;
    }
  }
  if (n < 7) return false;
  int tl = (int)(fe[0] - f[0]);
  if (tl < 5) return false;
  const char* type = f[0] + 2;  // GPRMC, GNRMC, ...
  if (type[0] == 'R' && type[1] == 'M' && type[2] == 'C' && n >= 10) {
    // $..RMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy
    // время берём только при статусе A: до привязки модуль отдаёт свои внутренние часы,
    // которые после включения питания идут с 00:00 и ничего не значат
    // L76K бывает присылает «A» и при 0 спутников в работе, поэтому сверяем с GGA:
    // привязка есть и в работе не меньше 4 спутников
    bool fixed = fe[2] > f[2] && *f[2] == 'A' && ggaQuality > 0 && ggaSats >= 4;
    if (!fixed) goodRmc = 0;
    if (fixed && fe[9] - f[9] >= 6 && fe[1] - f[1] >= 6) {
      const char* t = f[1];
      const char* d = f[9];
      int hh = (t[0] - '0') * 10 + t[1] - '0', mi = (t[2] - '0') * 10 + t[3] - '0', ss = (t[4] - '0') * 10 + t[5] - '0';
      int dd = (d[0] - '0') * 10 + d[1] - '0', mo = (d[2] - '0') * 10 + d[3] - '0', yy = (d[4] - '0') * 10 + d[5] - '0';
      if (mo >= 1 && mo <= 12 && dd >= 1 && yy >= 24 && yy < 80) {  // до привязки приёмник может отдавать дату по умолчанию (1980 и т. п.)
        uint32_t t = daysFrom2000(2000 + yy, mo, dd) * 86400u + hh * 3600u + mi * 60u + ss;
        // три сообщения подряд, и время в них идёт вперёд не больше чем на 5 с
        if (lastRmcUtc && t > lastRmcUtc && t - lastRmcUtc <= 5) { if (goodRmc < 255) goodRmc++; }
        else goodRmc = 0;
        lastRmcUtc = t;
        fix.utc = t;
        fix.timeValid = goodRmc >= 2;
      }
    }
    // координаты — только при настоящей привязке (как и время): «A» + GGA с привязкой и 4+ спутниками
    bool valid = fe[2] > f[2] && *f[2] == 'A' && ggaQuality > 0 && ggaSats >= 4;
    if (valid && fe[3] > f[3] && fe[5] > f[5]) {
      fix.lat = coord(f[3], fe[3], *f[4]);
      fix.lon = coord(f[5], fe[5], *f[6]);
      fix.posValid = true;
      fix.speedKmh = fe[7] > f[7] ? (float)num(f[7], fe[7]) * 1.852f : 0;
      fix.courseValid = fe[8] > f[8] && fix.speedKmh > 2;
      if (fix.courseValid) fix.course = (float)num(f[8], fe[8]);
    } else if (!valid) {
      fix.posValid = false;
    }
    return true;
  }
  if (type[0] == 'G' && type[1] == 'S' && type[2] == 'V' && n >= 4) {
    // $xxGSV,всего сообщений,номер,спутников в поле зрения,...
    char t0 = f[0][0], t1 = f[0][1];
    int k = (t0 == 'G' && t1 == 'P') ? 0 : (t0 == 'G' && t1 == 'L') ? 1 : ((t0 == 'B' && t1 == 'D') || (t0 == 'G' && t1 == 'B')) ? 2 : (t0 == 'G' && t1 == 'A') ? 3 : -1;
    if (k >= 0) {
      view[k] = (uint8_t)num(f[3], fe[3]);
      int sum = view[0] + view[1] + view[2] + view[3];
      fix.inView = (uint8_t)(sum > 255 ? 255 : sum);
      // спутники в сообщении: по 4 поля (номер, высота, азимут, SNR); пустой SNR — не слышен
      int total = (int)num(f[1], fe[1]), msg = (int)num(f[2], fe[2]);
      if (msg == 1) { heardAcc[k] = 0; snrAcc[k] = 0; }
      for (int i = 7; i < n; i += 4) {
        if (fe[i] > f[i]) {
          int v = (int)num(f[i], fe[i]);
          if (v > 0) { heardAcc[k]++; if (v > snrAcc[k]) snrAcc[k] = (uint8_t)v; }
        }
      }
      if (msg >= total) {
        heard[k] = heardAcc[k]; snr[k] = snrAcc[k];
        int h = 0, b = 0;
        for (int i = 0; i < 4; i++) { h += heard[i]; if (snr[i] > b) b = snr[i]; }
        fix.heard = (uint8_t)h;
        fix.bestSnr = (uint8_t)b;
      }
    }
    return true;
  }
  if (type[0] == 'G' && type[1] == 'G' && type[2] == 'A' && n >= 9) {
    fix.sats = (uint8_t)num(f[7], fe[7]);
    ggaSats = fix.sats;
    ggaQuality = (uint8_t)num(f[6], fe[6]);
    if (!ggaQuality || ggaSats < 4) { fix.timeValid = false; goodRmc = 0; }
    if (fe[8] > f[8]) fix.hdop = (float)num(f[8], fe[8]);
    return true;
  }
  return false;
}

}  // namespace pet
