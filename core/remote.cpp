// Команды по Bluetooth (текстом, через любое приложение «UART по Bluetooth»).
#include "game.h"
#include "util.h"
#include "version.h"

namespace pet {
using namespace gfx;

// строчные буквы: латиница и кириллица в UTF-8
static void lower(const char* in, char* out, int n) {
  int o = 0;
  for (int i = 0; in[i] && o < n - 1; i++) {
    uint8_t c = (uint8_t)in[i];
    if (c >= 'A' && c <= 'Z') { out[o++] = (char)(c + 32); continue; }
    if (c == 0xD0 && in[i + 1]) {
      uint8_t d = (uint8_t)in[i + 1];
      if (d >= 0x90 && d <= 0x9F) { out[o++] = (char)0xD0; if (o < n - 1) out[o++] = (char)(d + 0x20); i++; continue; }  // А–П
      if (d >= 0xA0 && d <= 0xAF) { out[o++] = (char)0xD1; if (o < n - 1) out[o++] = (char)(d - 0x20); i++; continue; }  // Р–Я
      if (d == 0x81) { out[o++] = (char)0xD1; if (o < n - 1) out[o++] = (char)0x91; i++; continue; }                     // Ё
    }
    out[o++] = (char)c;
  }
  out[o] = 0;
}

static const char* word(const char* s, const char* w) {  // s начинается со слова w? — вернуть остаток
  int i = 0;
  while (w[i] && s[i] == w[i]) i++;
  if (w[i]) return nullptr;
  if (s[i] && s[i] != ' ') return nullptr;
  while (s[i] == ' ') i++;
  return s + i;
}

static int num(const char*& p, int maxDigits) {
  int v = -1, k = 0;
  while (*p >= '0' && *p <= '9' && k < maxDigits) { v = (v < 0 ? 0 : v) * 10 + (*p - '0'); p++; k++; }
  return v;
}

static const char* HELP =
    "Команды: статус, время 21:40 (или 03.11.2026 21:40), еда кофе|пицца|печенька, свет, уборка, "
    "лечить, ревью, копия, восстановить <код>, сброс прогулок";

int Game::command(const char* line, char* out, int n) {
  char l[96];
  lower(line, l, sizeof l);
  const char* a;
  out[0] = 0;
  if ((a = word(l, "помощь")) || (a = word(l, "help")) || (a = word(l, "?"))) return sfmt(out, n, "%s", HELP);

  // для приложения: всё состояние одной строкой, поля через «|» (порядок — в android/.../PetState.kt)
  if ((a = word(l, "data"))) {
    return sfmt(out, n,
                "D|3|%d|%d|%u|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%u|%d|%d|%d|%d|%d|%d|%u|%d|%d|%u|%d|%d|%d|%d|%d|%d|%d|%d|%u|%d|%d"
                "|%d|%d|%d|%d|%d|%u|%d|%d|%u|%u|%d|%d|%d|%d"
                "|%d|%d|%u|%u|%d",
                s.form, s.alive, s.ageMin, s.hunger, s.happy, s.energy, s.discipline, s.weight, s.bugs, s.sick, s.doses,
                s.asleep, s.lightOff, s.callKind, s.epoch, s.clockSource, clockSure ? 1 : 0, s.generation, s.trait, s.favFood,
                s.favGame, s.totalMeters, s.questsDone, s.friendsMet, s.achievements, s.formsSeen, s.streak, s.bestReactCs,
                batPct, batCharging ? 1 : 0, s.mistakes, (int)(lifeFrac() * 100), s.careScore, s.seed, s.gamesWon, s.gamesPlayed,
                // экран: где питомец и куда идёт, что он сейчас делает — телефон повторяет движение и анимации
                (int)(petX * 10), (int)(petTarget * 10), petDir, actionKind, actionArg,
                actionKind ? (unsigned)(now - actionStart) : 0u, (int)scene, s.lightOff,
                 s.stepsToday, s.coins, s.hat, s.theme, hasWeather() ? weather : 0, tempC,
                // 0.41: только добавленные поля; старые клиенты D|3 читают первые 50.
                menuOpen && scene == S_HOME ? 1 : 0, scene == S_HOME ? cursor : -1,
                (unsigned)((now - menuTransitionAt) < MENU_ANIM_MS ?
                    now - menuTransitionAt : MENU_ANIM_MS),
                (unsigned)((now - cursorMs) < MENU_IDLE_MS ? now - cursorMs : MENU_IDLE_MS),
                 infoScreen && scene == S_HOME ? 1 : 0);
  }

  if ((a = word(l, "статус")) || (a = word(l, "status"))) {
    static const char* why[] = {"", "хочет есть", "скучает", "выключи свет", "болеет", "баги на полу", "", "капризничает"};
    static const char* src[] = {"не заданы", "вручную", "GPS", "с телефона"};
    char call[40] = "";
    if (attention()) sfmt(call, sizeof call, " Зовёт: %s.", why[s.callKind]);
    return sfmt(out, n,
                "%s, %u дн %u ч%s. Сытость %d%%, настроение %d%%, энергия %d%%, дисциплина %d%%. Вес %d КБ.%s%s "
                "Багов: %u. Часы %02d:%02d (%s).%s",
                formName((Form)s.form), s.ageMin / 1440, (s.ageMin / 60) % 24, s.alive ? "" : " (завершён)", s.hunger,
                s.happy, s.energy, s.discipline, s.weight, s.sick ? " Болеет." : " Здоров.", s.asleep ? " Спит." : "",
                s.bugs, hourNow(), minNow(), src[s.clockSource & 3], call);
  }

  if ((a = word(l, "время")) || (a = word(l, "time"))) {
    // «21:40», «21:40:05», «03.11.2026 21:40», «2026-11-03 21:40»
    const char* p = a;
    int d = -1, mo = -1, y = -1;
    const char* save = p;
    int first = num(p, 4);
    if (*p == '.' || *p == '-') {
      char sep = *p++;
      int second = num(p, 2);
      if (*p != sep) return sfmt(out, n, "Не понял дату. Пример: время 03.11.2026 21:40");
      p++;
      int third = num(p, 4);
      if (sep == '.') { d = first; mo = second; y = third; }
      else { y = first; mo = second; d = third; }
      while (*p == ' ') p++;
    } else {
      p = save;
    }
    int hh = num(p, 2);
    if (hh < 0 || *p != ':') return sfmt(out, n, "Не понял время. Пример: время 21:40");
    p++;
    int mm = num(p, 2), ss = 0;
    if (*p == ':') { p++; ss = num(p, 2); }
    if (hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59) return sfmt(out, n, "Неверное время");
    uint32_t day = s.epoch / 86400;
    if (y > 0) {
      if (y < 100) y += 2000;
      if (y < 2024 || y > 2079 || mo < 1 || mo > 12 || d < 1 || d > 31) return sfmt(out, n, "Неверная дата");
      day = daysFrom2000(y, mo, d);
    }
    s.epoch = day * 86400 + (uint32_t)hh * 3600 + (uint32_t)mm * 60 + (uint32_t)ss;
    s.clockSource = 3;
    clockSure = true;
    dirty = true;
    say("Время с телефона", pal::green);
    return sfmt(out, n, "Время установлено: %02d:%02d", hh, mm);
  }

  if ((a = word(l, "версия")) || (a = word(l, "version"))) return sfmt(out, n, "V|%s", BIT_VERSION);
  if ((a = word(l, "ник")) || (a = word(l, "nick"))) return sfmt(out, n, "N|%s", nick());
  if ((a = word(l, "сохранить")) || (a = word(l, "save"))) { saveNow = true; dirty = true; return sfmt(out, n, "Сохраняю"); }

  if ((a = word(l, "погода")) || (a = word(l, "weather"))) {  // «погода 3 -2»: вид и температура
    const char* p = a;
    int k = num(p, 1);
    while (*p == ' ') p++;
    bool neg = *p == '-';
    if (neg) p++;
    int t = num(p, 2);
    if (k < 1 || k > 6 || t < 0) return sfmt(out, n, "W|0");
    weather = (uint8_t)k;
    tempC = (int8_t)(neg ? -t : t);
    weatherUntil = now + 3 * 3600000u;
    // При базовом обновлении сбрасываем расширенные показатели до следующего WX/FC.
    forecastCount = 0;
    weatherExtraUntil = 0;
    humidityPct = windMs = precipitationPct = 255;
    pressureMm = 0;
    return sfmt(out, n, "W|%d", k);
  }

  // Необязательные расширения погоды (0.43), короткие BLE-команды по отдельности.
  // wx: влажность %, ветер м/с, ощущается °C, давление мм рт.ст., осадки %.
  // Отрицательная единица означает отсутствие значения. Старые клиенты работают как раньше.
  if ((a = word(l, "wx"))) {
    const char* p = a;
    int values[5];
    for (int i = 0; i < 5; ++i) {
      while (*p == ' ') p++;
      bool minus = *p == '-';
      if (minus) ++p;
      int v = num(p, 4);
      if (v < 0 || (*p && *p != ' ')) return sfmt(out, n, "WX|0");
      values[i] = minus ? -v : v;
    }
    if (*p || values[0] < -1 || values[0] > 100 ||
        values[1] < -1 || values[1] > 99 || values[2] < -60 || values[2] > 70 ||
        values[3] < -1 || values[3] > 900 || values[4] < -1 || values[4] > 100)
      return sfmt(out, n, "WX|0");
    humidityPct = values[0] < 0 ? 255 : (uint8_t)values[0];
    windMs = values[1] < 0 ? 255 : (uint8_t)values[1];
    feelsC = (int8_t)values[2];
    pressureMm = values[3] < 0 ? 0 : (uint16_t)values[3];
    precipitationPct = values[4] < 0 ? 255 : (uint8_t)values[4];
    weatherExtraUntil = now + 3u * 3600000u;
    return sfmt(out, n, "WX|1");
  }
  // fc: 0..6 тройки (час, температура °C, WMO-тип 1..6).
  if ((a = word(l, "fc"))) {
    const char* p = a;
    int count = num(p, 1);
    if (count < 0 || count > 6) return sfmt(out, n, "FC|0");
    ForecastHour items[6] = {};
    for (int i = 0; i < count; ++i) {
      int vals[3];
      for (int j = 0; j < 3; ++j) {
        while (*p == ' ') p++;
        bool minus = *p == '-';
        if (minus) p++;
        int v = num(p, 3);
        if (v < 0 || (*p && *p != ' ')) return sfmt(out, n, "FC|0");
        vals[j] = minus ? -v : v;
      }
      if (vals[0] < 0 || vals[0] > 23 || vals[1] < -60 || vals[1] > 70 ||
          vals[2] < 1 || vals[2] > 6) return sfmt(out, n, "FC|0");
      items[i].hour = (uint8_t)vals[0];
      items[i].temp = (int8_t)vals[1];
      items[i].kind = (uint8_t)vals[2];
    }
    while (*p == ' ') p++;
    if (*p) return sfmt(out, n, "FC|0");
    forecastCount = (uint8_t)count;
    for (int i = 0; i < 6; ++i) forecast[i] = items[i];
    weatherExtraUntil = now + 3u * 3600000u;
    return sfmt(out, n, "FC|%d", count);
  }

  if ((a = word(l, "шаги")) || (a = word(l, "steps"))) {  // приложение присылает шаги с шагомера телефона
    const char* p = a;
    int k = num(p, 6);
    if (k <= 0) return sfmt(out, n, "S|0");
    addSteps((uint32_t)k);
    return sfmt(out, n, "S|%u|%u", s.stepsToday, s.coins);
  }

  if ((a = word(l, "сброс")) && word(a, "прогулок")) {
    resetWalks();
    say("Прогулки обнулены", pal::green);
    return sfmt(out, n, "Прогулки обнулены: метры, места, квесты и награды за них");
  }

  if ((a = word(l, "копия")) || (a = word(l, "backup"))) {
    Save c = s;
    c.crc = crc32(&c, offsetof(Save, crc));
    int k = sfmt(out, n, "BIT3 ");
    if (b64encode((const uint8_t*)&c, sizeof c, out + k, n - k) < 0) return sfmt(out, n, "Не хватило места для копии");
    return k;
  }

  if ((a = word(l, "восстановить")) || (a = word(l, "restore"))) {
    // код — из исходной строки: base64 чувствителен к регистру
    const char* p = line;
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    if ((p[0] == 'B' || p[0] == 'b') && p[3] == '3' && p[4] == ' ') p += 5;
    static uint8_t buf[sizeof(Save) + 4];
    int got = b64decode(p, buf, sizeof buf);
    if (got <= 0 || !loadRaw(buf, (size_t)got)) return sfmt(out, n, "Код повреждён — питомец не изменён");
    dirty = true;
    return sfmt(out, n, "Питомец восстановлен: %s, поколение %u", formName((Form)s.form), s.generation);
  }

  // действия — как с кнопки, только на главном экране
  if (!s.alive) return sfmt(out, n, "Питомец завершён. Начни новый на плате");
  bool home = scene == S_HOME && actionKind == A_NONE;
  const char* before = toast;
  uint32_t tBefore = toastStart;
  if ((a = word(l, "еда")) || (a = word(l, "feed"))) {
    if (!home) return sfmt(out, n, "Плата сейчас не на главном экране");
    if (s.asleep) return sfmt(out, n, "Тсс… спит");
    int kind = word(a, "кофе") || word(a, "coffee") ? 0 : (word(a, "пицца") || word(a, "pizza") ? 1 : (word(a, "печенька") || word(a, "cookie") ? 2 : -1));
    if (kind < 0) return sfmt(out, n, "Что дать? еда кофе, еда пицца или еда печенька");
    openList(0);
    listSel = (uint8_t)kind;
    listChoose();
  } else if ((a = word(l, "свет")) || (a = word(l, "light"))) {
    if (!home) return sfmt(out, n, "Плата сейчас не на главном экране");
    activate(I_LIGHT);
  } else if ((a = word(l, "уборка")) || (a = word(l, "clean"))) {
    if (!home) return sfmt(out, n, "Плата сейчас не на главном экране");
    activate(I_CLEAN);
  } else if ((a = word(l, "лечить")) || (a = word(l, "heal"))) {
    if (!home) return sfmt(out, n, "Плата сейчас не на главном экране");
    activate(I_HEAL);
  } else if ((a = word(l, "ревью")) || (a = word(l, "review"))) {
    if (!home) return sfmt(out, n, "Плата сейчас не на главном экране");
    activate(I_REVIEW);
  } else {
    return sfmt(out, n, "Не знаю такую команду. %s", HELP);
  }
  idleMs = 0;
  if (toast && (toast != before || toastStart != tBefore)) return sfmt(out, n, "%s", toast);
  if (actionKind == A_REFUSE) return sfmt(out, n, "Отказывается");
  return sfmt(out, n, "Готово");
}

void Game::mirror(const int32_t* v, int n) {
  if (n < 36) return;
  speed = 0;  // своя логика не идёт — только анимация по данным с платы
  Form prevForm = (Form)s.form;
  uint8_t prevAlive = s.alive;
  s.form = (uint8_t)v[0]; s.alive = (uint8_t)v[1]; s.ageMin = (uint32_t)v[2];
  s.hunger = (int16_t)v[3]; s.happy = (int16_t)v[4]; s.energy = (int16_t)v[5]; s.discipline = (int16_t)v[6];
  s.weight = (int16_t)v[7]; s.bugs = (uint8_t)v[8]; s.sick = (uint8_t)v[9]; s.doses = (uint8_t)v[10];
  s.asleep = (uint8_t)v[11]; s.lightOff = (uint8_t)v[12]; s.callKind = (uint8_t)v[13]; s.fakeCall = s.callKind == C_FAKE;
  s.epoch = (uint32_t)v[14]; s.clockSource = (uint8_t)v[15]; clockSure = v[16] != 0; s.generation = (uint16_t)v[17];
  s.trait = (uint8_t)v[18]; s.favFood = (uint8_t)v[19]; s.favGame = (uint8_t)v[20]; s.totalMeters = (uint32_t)v[21];
  s.questsDone = (uint16_t)v[22]; s.friendsMet = (uint16_t)v[23]; s.achievements = (uint32_t)v[24];
  s.formsSeen = (uint16_t)v[25]; s.streak = (uint8_t)v[26]; s.bestReactCs = (uint8_t)v[27];
  setBattery(v[28], v[29] != 0);
  s.mistakes = (uint8_t)v[30]; s.careScore = (uint16_t)v[32]; s.seed = (uint32_t)v[33];
  s.gamesWon = (uint16_t)v[34]; s.gamesPlayed = (uint16_t)v[35];
  if (n >= 50) { weather = (uint8_t)v[48]; tempC = (int8_t)v[49]; weatherUntil = weather ? now + 3600000u : 0; }
  if (n >= 48) { s.stepsToday = (uint32_t)v[44]; s.coins = (uint32_t)v[45]; s.hat = (uint8_t)v[46]; s.theme = (uint8_t)v[47]; }
  if (n >= 44) {
    // движение: своя случайная прогулка выключена — идём туда же, куда питомец на плате
    float bx = v[36] / 10.0f;
    petTarget = v[37] / 10.0f;
    petDir = v[38] < 0 ? -1 : 1;
    if (bx - petX > 8 || petX - bx > 8) petX = bx;  // сильно разошлись — переставить
    else petX += (bx - petX) * 0.5f;                 // немного — подтянуть плавно
    nextWander = now + 0x40000000u;
    // действие (еда, уборка, лечение…): запустить ту же анимацию с того же момента
    Action k = (Action)v[39];
    uint32_t age = (uint32_t)v[41];
    if (k != A_NONE) {
      bool fresh = actionKind != k || actionArg != (uint8_t)v[40] || (now - actionStart) > age + 800;
      if (fresh) { actionKind = k; actionArg = (uint8_t)v[40]; actionStart = now - age; }
    }
  }
  Scene want = !s.alive ? S_DEATH : (s.form == EGG ? S_HATCH : S_HOME);
  if (want != scene || (Form)s.form != prevForm || s.alive != prevAlive) {
    scene = want;
    sceneStart = now;
    actionKind = A_NONE;
  }
  // Расширение телеметрии D|3|...: сохранён прежний порядок первых 50 полей.
  // Клиент следует положению и времени анимации на настоящей плате.
  if (n >= 54 && want == S_HOME) {
    bool deviceOpen = v[50] != 0 && v[51] >= 0 && v[51] < ICON_COUNT;
    uint32_t animationAge = (uint32_t)clampi(v[52], 0, (int)MENU_ANIM_MS);
    uint32_t idleAge = (uint32_t)clampi(v[53], 0, (int)MENU_IDLE_MS);
    menuOpen = deviceOpen;
    cursor = deviceOpen ? v[51] : -1;
    menuTransitionAt = now - animationAge;
    cursorMs = now - idleAge;
    infoScreen = n >= 55 && v[54] == 1 && !deviceOpen;
  } else {
    // Старые платы не передают курсор; не имитируем фиктивные нажатия.
    menuOpen = false;
    cursor = -1;
    menuTransitionAt = now - MENU_ANIM_MS;
    infoScreen = false;
  }
}

}  // namespace pet
