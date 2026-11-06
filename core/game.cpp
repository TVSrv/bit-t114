// Логика: потребности, болезни, сон, звонки внимания, взросление, действия, кнопка.
#include "game.h"
#include "util.h"

namespace pet {
using namespace gfx;

static constexpr uint32_t MAGIC = 0x42495431;  // "BIT1"
static constexpr uint16_t VERSION = 4;


uint32_t Game::crc32(const void* d, size_t n) {
  const uint8_t* p = (const uint8_t*)d;
  uint32_t c = 0xFFFFFFFF;
  while (n--) {
    c ^= *p++;
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320 & (0 - (c & 1)));
  }
  return ~c;
}

uint32_t Game::rnd() {
  rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
  return rng;
}

void Game::init(uint32_t seed) {
  uint16_t gen = s.magic == MAGIC ? s.generation : 0;
  uint32_t ep = s.magic == MAGIC ? s.epoch : 0;
  uint8_t src = s.magic == MAGIC ? s.clockSource : 0;
  int8_t tz = s.magic == MAGIC ? s.tz : 3;
  uint8_t br = s.magic == MAGIC ? s.brightness : 2, leds = s.magic == MAGIC ? s.leds : 1;
  uint8_t flip = s.magic == MAGIC ? s.flip : 0, ga = s.magic == MAGIC ? s.gpsAuto : 1;
  uint32_t tm = s.magic == MAGIC ? s.totalMeters : 0;
  Save prev = s;
  bool hadPrev = s.magic == MAGIC && s.form != EGG;
  memset(&s, 0, sizeof(s));
  if (prev.magic == MAGIC) {  // общее для всех поколений
    s.achievements = prev.achievements; s.formsSeen = prev.formsSeen; s.totalWins = prev.totalWins;
    s.bugsCleaned = prev.bugsCleaned; s.favFed = prev.favFed;
    s.alarmOn = prev.alarmOn; s.alarmH = prev.alarmH; s.alarmM = prev.alarmM; s.alarmWakes = prev.alarmWakes;
    s.questLat = prev.questLat; s.questLon = prev.questLon; s.questKind = prev.questKind;
    s.questsDone = prev.questsDone; s.placesTotal = prev.placesTotal; s.placeHead = prev.placeHead;
    memcpy(s.places, prev.places, sizeof s.places);
    s.bleOn = prev.bleOn; s.loraOn = prev.loraOn; s.nightLight = prev.nightLight; s.bestReactCs = prev.bestReactCs;
    s.friendsMet = prev.friendsMet; s.visits = prev.visits; memcpy(s.friends, prev.friends, sizeof s.friends);
    s.deviceId = prev.deviceId;
    s.reserved4[0] = prev.reserved4[0] == 1 ? 1 : 0;
    s.reserved4[1] = prev.reserved4[1];
    s.totalSteps = prev.totalSteps; s.coins = prev.coins; s.owned = prev.owned; s.hat = prev.hat; s.theme = prev.theme;
  } else {
    s.alarmH = 7; s.alarmM = 30;
    s.bleOn = 1; s.nightLight = 1;
  }
  s.magic = MAGIC;
  s.version = VERSION;
  s.form = EGG;
  s.alive = 1;
  s.hunger = 60; s.happy = 60; s.energy = 100; s.discipline = 0; s.weight = 5;
  s.nextBugMin = 30; s.nextFakeMin = 40;
  s.epoch = ep ? ep : daysFrom2000(2026, 1, 1) * 86400u + 12 * 3600u;
  s.clockSource = src; s.tz = tz; s.brightness = br; s.leds = leds; s.flip = flip; s.gpsAuto = ga;
  s.totalMeters = tm;
  s.generation = gen + 1;
  s.seed = seed ? seed : 0x1234567;
  rng = s.seed | 1;
  // характер и наследство
  s.favFood = (uint8_t)rndInt(3);
  s.favGame = (uint8_t)rndInt(GAME_COUNT);
  s.trait = (uint8_t)rndInt(TRAIT_COUNT);
  if (hadPrev) {
    s.parentForm = prev.form == GHOST ? prev.parentForm : prev.form;
    s.parentTrait = prev.trait;
    if (rndInt(10) < 7) { s.trait = prev.trait; s.inherited |= 1; }
    if (rndInt(2) == 0) { s.favFood = prev.favFood; s.inherited |= 2; }
    if (prev.discipline >= 75) { s.discipline = 20; s.inherited |= 4; }  // воспитанный родитель
  }
  s.lastDay = s.epoch / 86400;
  scene = S_HATCH;
  sceneStart = now;
  cursor = -1;
  menuOpen = false;
  menuTransitionAt = now - MENU_ANIM_MS;
  nextHobby = now + 45u * 60000u;
  hobbyKind = 0;
  dirty = true;
}

bool Game::load(const Save& saved) {
  if (saved.magic != MAGIC || saved.version != VERSION) return false;
  if (crc32(&saved, offsetof(Save, crc)) != saved.crc) return false;
  s = saved;
  if (hwId) s.deviceId = hwId;  // номер платы свой, даже если питомец восстановлен из копии с другой платы
  rng = s.seed ^ s.epoch;
  if (!rng) rng = 7;
  scene = s.alive ? (s.form == EGG ? S_HATCH : S_HOME) : S_DEATH;
  sceneStart = now;
  cursor = -1;
  menuOpen = false;
  menuTransitionAt = now - MENU_ANIM_MS;
  // После перезагрузки millis() начинается с нуля; старый момент включения
  // ночника не сохраняется. Если Бит уже спит в темноте — запускаем окно заново.
  lightAnim = now;
  clockSure = false;  // плата была выключена — часы отстают на это время
  if (s.reserved4[0] > 1) s.reserved4[0] = 0;
  nextHobby = now + 40u * 60000u; hobbyKind = 0;
  if (s.clockSource) say("Был перезапуск — проверь часы", pal::yellow, 8000);
  checkAchievements();
  return true;
}

bool Game::sleepTime() const {
  int h = hourNow();
  int from, to;
  switch ((Form)s.form) {
    case EGG: case BABY: case GHOST: return false;
    case JUNIOR: from = 21; to = 8; break;
    case MIDDLE: case FREELANCER: from = 22; to = 8; break;
    case HACKER: from = 2; to = 10; break;
    case LEGACY: from = 0; to = 9; break;
    default: from = 23; to = 8; break;
  }
  return from > to ? (h >= from || h < to) : (h >= from && h < to);
}

// --- время ---

void Game::tick(uint32_t dt) {
  uint64_t g = (uint64_t)dt * speed;  // игровое время
  now += dt;
  idleMs += dt;
  secAcc += (uint32_t)(g > 0xFFFFFFF ? 0xFFFFFFF : g);
  int guard = 0;
  while (secAcc >= 1000 && guard++ < 200000) {
    secAcc -= 1000;
    second();
  }
  // синхронизация часов по GPS раз в сутки
  if (s.gpsAuto && now >= nextGpsSync && s.alive) {
    gpsSyncUntil = now + 5 * 60000;
    nextGpsSync = now + 3600000u;  // не получилось — снова через час
  }
  bool want = wantsGps();
  if (want && !gpsWanted) gpsOnSince = now;
  gpsWanted = want;
  // замер батареи раз в 10 минут (кольцо на 6 часов) — для оценки, на сколько хватит
  if (now - batLastSample >= 600000 || !batLastSample) {
    batLastSample = now;
    if (batCharging) batCount = 0;  // на зарядке оценка сбрасывается
    else {
      for (int i = 35; i > 0; i--) batSamples[i] = batSamples[i - 1];
      batSamples[0] = (uint8_t)clampi(batPct, 0, 100);
      if (batCount < 36) batCount++;
    }
  }
  // Меню плавно закрывается через 7 секунд после последнего нажатия.
  if (scene == S_HOME && menuOpen && now - cursorMs >= MENU_IDLE_MS) closeMenu();
  // забытые меню закрываются сами, чтобы экран мог погаснуть
  if (idleMs > 45000 && (scene == S_LIST || scene == S_STATUS || scene == S_CLOCK || scene == S_CONFIRM)) goHome();
  if (scene == S_TRAIN) trainingTick();
  if (actionKind != A_NONE) {
    static const uint16_t dur[] = {0, 2700, 1900, 2300, 2300, 1400, 900, 1600, 1300, 1500};
    if (now - actionStart >= dur[actionKind]) finishAction();
  }
}

void Game::second() {
  s.epoch++;
  if (!s.alive) return;
  if (scene == S_HATCH || s.form == EGG) {
    if (scene == S_HATCH && now - sceneStart > 0) {
      // яйцо «компилируется» 10 игровых минут
      if (++s.stageMin >= EVO_EGG_SECONDS) {
        s.stageMin = 0;
        evoFrom = EGG; evoTo = BABY;
        s.form = BABY;
        s.formsSeen |= 1 << BABY;
        s.hunger = 50; s.happy = 50; s.weight = 5;
        scene = S_EVOLVE; sceneStart = now;
        justEvolved = true; // разбудить экран и показать вылупление после 10 минут
        dirty = true;
      }
    }
    return;
  }
  if (++minAcc >= 60) {
    minAcc = 0;
    minute();
  }
}

void Game::minute() {
  s.ageMin++;
  s.stageMin++;
  Form f = (Form)s.form;
  int st = stageOf(f);
  static const uint8_t hungerEvery[] = {0, 3, 5, 6, 8};
  static const uint8_t happyEvery[] = {0, 3, 4, 5, 7};
  bool sleeping = s.asleep != 0;
  const bool calmCare = s.reserved4[0] == 0;

  // сон по расписанию
  bool night = sleepTime();
  if (night && s.asleep != 1) {
    s.asleep = 1;
    if (s.lightOff && s.nightLight) lightAnim = now;
    say("Пора спать… выключи свет", pal::purple, 3000);
  } else if (!night && s.asleep == 1) {
    s.asleep = 0;
    s.lightOff = 0;
    s.snacks = 0;
    say("Доброе утро!", pal::yellow, 2500);
    jumpAt = now;
  }
  if (s.asleep == 2 && !s.lightOff && s.energy >= 60) s.asleep = 0;  // проснулся после дрёмы
  if (s.asleep == 2 && s.lightOff && s.energy >= 100) { s.asleep = 0; say("Выспался, включи свет", pal::yellow); }
  if (!s.asleep && s.lightOff && s.energy < 100) { s.asleep = 2; }  // в темноте засыпает
  if (!s.asleep && s.energy <= 0) { s.asleep = 2; say("Задремал…", pal::dim); }
  sleeping = s.asleep != 0;

  // потребности
  uint32_t m = s.ageMin;
  int he = hungerEvery[st] - (s.trait == T_GLUTTON ? 1 : 0);
  if (calmCare) he *= 2;
  if (m % (sleeping ? he * 4 : he) == 0 && s.hunger > 0) s.hunger--;
  int hap = happyEvery[st] + (s.trait == T_CALM ? 2 : 0) - (s.trait == T_FIDGET ? 1 : 0);
  int hk = hap * (s.sick ? 1 : 2) / 2;
  if (calmCare) hk *= 2;
  if (hk < 1) hk = 1;
  if (!sleeping && m % hk == 0 && s.happy > 0) s.happy--;
  if (!calmCare && !sleeping && s.bugs >= 2 && m % 10 == 0 && s.happy > 0) s.happy--;
  if (!sleeping && s.lightOff && s.happy > 0) s.happy--;  // сидит в темноте
  if (sleeping) {
    if (m % (s.lightOff ? 2 : 5) == 0 && s.energy < 100) s.energy++;
    if (s.trait == T_SLEEPY && m % 4 == 0 && s.energy < 100) s.energy++;
  } else if (m % (s.trait == T_SLEEPY ? 4 : 6) == 0 && s.energy > 0) {
    s.energy--;
  }
  // погода чуть влияет на питомца (раз в полчаса, пока не спит)
  if (hasWeather() && !sleeping && m % 30 == 0) {
    if ((weather == 3 || weather == 5) && s.happy > 0) s.happy--;  // дождь — грустно
    if (tempC >= 28 && s.energy > 0) s.energy--;                     // жара — устаёт
    if (tempC <= -15 && s.hunger > 0) s.hunger--;                    // мороз — голодает
  }
  // новый день: серия дней без ошибок ухода
  uint32_t day = s.epoch / 86400;
  if (day != s.lastDay) {
    if (s.mistakesToday == 0) { if (s.streak < 255) s.streak++; addCoins(10, false); }  // день без ошибок ухода
    else s.streak = 0;
    s.stepsToday = 0;
    s.mistakesToday = 0;
    s.lastDay = day;
  }
  // будильник
  if (s.alarmOn && !alarmActive && hourNow() == s.alarmH && minNow() == s.alarmM) triggerAlarm();

  // баги (как «какашки»)
  if (!sleeping) {
    if (s.nextBugMin > 0) s.nextBugMin--;
    if (s.nextBugMin == 0) {
      if (s.bugs < 4) {
        bugX[s.bugs] = 0;
        s.bugs++;
        sfmt(eventText, sizeof eventText, "Появился баг на полу (%u) — нужна уборка", s.bugs);
        event(eventText);
        if (scene == S_HOME && actionKind == A_NONE) startAction(A_POOP);
      }
      s.nextBugMin = (uint16_t)(calmCare ? 180 + rndInt(120) : 45 + rndInt(60));
    }
  }

  // болезнь
  if (!s.sick && !calmCare) {
    int chance = 0;
    if (!calmCare && s.bugs >= 3) chance += 3;
    if (s.weight > baseWeight(f) * 2 + 5) chance += 2;
    if (s.hunger == 0) chance += 2;
    if (chance && rndInt(400) < chance) {
      s.sick = 1;
      s.doses = (uint8_t)(1 + rndInt(2));
      say("Segmentation fault!", pal::red, 2500);
    }
  }
  if (s.sick) {
    if (s.sickMin < 65535) s.sickMin++;
    if (calmCare && s.sickMin >= 240) { s.sick = 0; s.doses = 0; s.sickMin = 0; say("Бит восстановился после отдыха", pal::green); dirty = true; }
    else if (!calmCare && s.sickMin >= 12 * 60) { die(D_SICK); return; }
  } else {
    s.sickMin = 0;
  }
  if (s.hunger == 0 && !calmCare) {
    if (s.starveMin < 65535) s.starveMin++;
    if (s.starveMin >= 12 * 60) { die(D_STARVE); return; }
  } else {
    s.starveMin = 0; // в спокойном режиме голод не накапливает скрытый смертельный таймер
  }

  // капризы для воспитания
  if (calmCare && s.callKind == C_FAKE) resolveCall(C_FAKE);
  if (!calmCare && !sleeping && s.callKind == C_NONE && st >= 2) {
    if (s.nextFakeMin > 0) s.nextFakeMin--;
    if (s.nextFakeMin == 0) {
      setCall(C_FAKE);
      s.nextFakeMin = (uint16_t)(90 + rndInt(180));
    }
  }
  // Дисциплинированный Бит иногда сам убирает один баг.
  // Только спокойный режим: в классике ручная уборка остаётся частью механики.
  if (calmCare && !sleeping && st >= 2 && s.discipline >= 60 &&
      s.bugs > 0 && m % 45 == 0 && rndInt(2) == 0) {
    --s.bugs;
    if (s.bugsCleaned < 65535) ++s.bugsCleaned;
    say("Бит сам навёл порядок!", pal::green);
    dirty = true;
  }
  updateCalls();
  // Самостоятельные занятия без штрафов и назойливых уведомлений.
  if (calmCare && !sleeping && !s.sick && st >= 2 && now >= nextHobby) {
    hobbyKind = (uint8_t)(1 + rndInt(4));
    if (s.discipline >= 80 && rndInt(3) == 0) hobbyKind = 2;
    if (s.trait == T_SMART && rndInt(2) == 0) hobbyKind = 2;
    else if (s.trait == T_SLEEPY && rndInt(2) == 0) hobbyKind = 3;
    else if (s.trait == T_FIDGET && rndInt(2) == 0) hobbyKind = 4;
    hobbyUntil = now + 32000u;
    nextHobby = now + (70u + (uint32_t)rndInt(70)) * 60000u;
    if (s.reserved4[1] < 255) s.reserved4[1]++;
    switch (hobbyKind) {
      case 1: s.discipline = (int16_t)clampi(s.discipline + 2, 0, 100);
              say("Бит читает и учится", pal::cyan); break;
      case 2: s.discipline = (int16_t)clampi(s.discipline + 3, 0, 100);
              addCoins(s.discipline >= 80 ? 3 : 2, false);
              say("Бит придумал новый проект", pal::green); break;
      case 3: s.energy = (int16_t)clampi(s.energy + 4, 0, 100);
              say("Бит решил отдохнуть", pal::yellow); break;
      default: s.happy = (int16_t)clampi(s.happy + 4, 0, 100);
               say("Бит танцует от радости", pal::pink); break;
    }
    dirty = true;
  }
  evolveCheck();
  holidayCheck();
  checkAchievements();
  if (m % 10 == 0) dirty = true;
}

void Game::setCall(uint8_t kind) {
  if (s.callKind == kind) return;
  if (kind == C_FAKE) event("Бит зовёт: капризничает (нужно код-ревью)");
  s.callKind = kind;
  s.callMin = 0;
  s.fakeCall = kind == C_FAKE;
}

void Game::resolveCall(uint8_t kind) {
  if (s.callKind == kind) { s.callKind = C_NONE; s.callMin = 0; s.fakeCall = 0; }
}

void Game::updateCalls() {
  // что сейчас нужно, по важности
  uint8_t need = C_NONE;
  if (s.sick) need = C_SICK;
  else if (s.asleep == 1 && !s.lightOff) need = C_LIGHT;
  else if (s.hunger <= 5) need = C_HUNGER;
  else if (s.happy <= 5 && !s.asleep) need = C_HAPPY;
  else if (s.bugs >= 3) need = C_BUGS;
  if (s.callKind == C_FAKE && need == C_NONE) {
    if (++s.callMin >= 15) resolveCall(C_FAKE);  // каприз прошёл незамеченным
    return;
  }
  if (need != s.callKind) {
    s.callKind = need;
    s.callMin = 0;
    s.fakeCall = 0;
    static const char* why[] = {"", "хочет есть", "скучает", "пора выключить свет", "болеет", "баги на полу", "", ""};
    if (need) { sfmt(eventText, sizeof eventText, "Бит зовёт: %s", why[need]); event(eventText); }
    return;
  }
  if (need != C_NONE) {
    s.callMin++;
    if (s.callMin == 15 && s.reserved4[0] == 1) {  // не ответили за 15 минут — ошибка ухода
      if (s.mistakes < 255) s.mistakes++;
      if (s.mistakesToday < 255) s.mistakesToday++;
      dirty = true;
    }
  }
}

void Game::evolveCheck() {
  Form f = (Form)s.form, to = f;
  uint32_t sm = s.stageMin;
  switch (f) {
    case BABY: if (sm >= EVO_BABY_MINUTES) to = JUNIOR; break;
    case JUNIOR: if (sm >= EVO_JUNIOR_MINUTES) to = s.mistakes <= 3 ? MIDDLE : FREELANCER; break;
    case MIDDLE: case FREELANCER:
      if (sm >= EVO_MIDDLE_MINUTES) {
        if (s.mistakes <= 2 && s.discipline >= 75 && f == MIDDLE) to = ARCHITECT;
        else if (s.mistakes <= 2) to = HACKER;
        else if (s.mistakes <= 6) to = SENIOR;
        else to = LEGACY;
      }
      break;
    case HACKER: case ARCHITECT:
      if (sm >= EVO_EXPERT_MINUTES && s.mistakes <= 1 && s.discipline >= 100) to = AI;
      break;
    default: break;
  }
  if (to != f) {
    evoFrom = f; evoTo = to;
    s.form = to;
    s.formsSeen |= (uint16_t)(1 << to);
    s.stageMin = 0;
    s.careScore += (uint16_t)(s.mistakes < 10 ? 10 - s.mistakes : 0);
    s.mistakes = 0;
    if (s.weight < baseWeight(to)) s.weight = (int16_t)baseWeight(to);
    scene = S_EVOLVE; sceneStart = now;
    actionKind = A_NONE;
    justEvolved = true;
    sfmt(eventText, sizeof eventText, "Бит вырос: %s", formName(to));
    event(eventText);
    dirty = true;
    return;
  }
  // Старость отсрочена под удлинённые стадии; модель жизни общая с визуальным старением.
  uint32_t life = lifeDays(f);
  if (life && s.ageMin >= (life + (s.seed % 3u)) * 24u * 60u) die(D_OLD);
}

void Game::die(uint8_t cause) {
  if (cause == D_OLD) unlock(ACH_OLD);
  s.alive = 0;
  s.causeOfDeath = cause;
  s.callKind = C_NONE;
  scene = S_DEATH; sceneStart = now;
  actionKind = A_NONE;
  walking = false;
  event(cause == D_OLD ? "Бит прожил долгую жизнь и ушёл" : "Бит погиб: нужен новый питомец");
  dirty = true;
}

// --- GPS ---

bool Game::wantsGps() const { return walking || scene == S_GPS || (s.gpsAuto && now < gpsSyncUntil); }

void Game::gpsFeed(const GpsFix& f) {
  fix = f;
  if (walking && f.alive && !f.posValid && now - gpsOnSince > 120000 && now - gpsHintAt > 120000) {
    gpsHintAt = now;
    say(f.inView < 4 ? "Мало спутников — выйди на улицу" : "Ловлю сигнал, подожди", pal::yellow, 5000);
  }
  // Отключение времени по GPS действует везде, включая экран проверки GPS и прогулку.
  bool useTime = s.gpsAuto && f.timeValid && (now < gpsSyncUntil || scene == S_GPS);
  if (f.timeValid) {
    // L76K в первые минуты после привязки может присылать не время UTC, а сколько он включён
    // (на плате было 03:03 и 03:08 при UTC+3). Такое время отбрасываем.
    uint32_t up = f.moduleUp ? f.moduleUp : (now - gpsOnSince) / 1000;
    uint32_t tod = f.utc % 86400;
    uint32_t diff = tod > up ? tod - up : up - tod;
    // устойчивость считаем только по отчётам с привязкой и правдоподобным временем
    bool good = f.posValid && f.sats >= 4 && diff >= 180;
    stableFix = good ? (uint16_t)(stableFix < 60000 ? stableFix + 1 : stableFix) : 0;
    if (diff < 180) { gpsTimeNote = 2; useTime = false; }
    else if (stableFix < 60) { gpsTimeNote = 1; useTime = false; }  // ~30 с устойчивой привязки
    else if (useTime) gpsTimeNote = 3;
  } else {
    gpsTimeNote = 0;
    stableFix = 0;
  }
  if (useTime) {
    uint32_t local = f.utc + (int32_t)s.tz * 3600;
    s.epoch = local;
    s.clockSource = 2;
    clockSure = true;
    if (!walking) gpsSyncUntil = 0;  // время получено — приёмник можно выключить
    nextGpsSync = now + 24u * 3600000u;
    dirty = true;
  }
  if (!walking || !f.posValid || f.hdop > 4) return;
  questStep(f);
  // метры — только когда модуль сам видит движение: стоящая плата с «прыгающими» координатами не гуляет
  if (f.speedKmh < 1.5f || f.hdop > 3 || f.sats < 4) { lastLat = f.lat; lastLon = f.lon; haveLast = true; return; }
  if (!haveLast) { lastLat = f.lat; lastLon = f.lon; haveLast = true; return; }
  // равнопромежуточная проекция — для шагов в десятки метров точна
  float dLat = (float)(f.lat - lastLat) * 111195.0f;
  float dLon = (float)(f.lon - lastLon) * 111195.0f * fcos((float)f.lat * 0.0174533f);
  float d = fsqrt(dLat * dLat + dLon * dLon);
  if (d < 4) return;                           // дрожание на месте
  if (d > 120) { lastLat = f.lat; lastLon = f.lon; return; }  // скачок
  walkCourse = fatan2(dLon, dLat) * 57.29578f;  // курс по последнему шагу
  if (walkCourse < 0) walkCourse += 360;
  courseKnown = true;
  lastLat = f.lat; lastLon = f.lon;
  addMeters((uint32_t)d, true);
}

void Game::addMeters(uint32_t d, bool rewards) {
  uint32_t before = s.walkMeters;
  s.walkMeters += d;
  s.totalMeters += d;
  // каждые 50 м: настроение, вес, немного сил
  for (uint32_t k = before / 50; rewards && k < s.walkMeters / 50; k++) {
    if (s.happy < 100) s.happy = (int16_t)(s.happy + 3 > 100 ? 100 : s.happy + 3);
    if (s.weight > baseWeight((Form)s.form)) s.weight--;
    if (s.energy > 5) s.energy--;
    if ((k + 1) % 10 == 0) { say("+500 м! Отличная прогулка", pal::green); s.careScore++; addCoins(5, false); }
  }
  if (rewards) resolveCall(C_HAPPY);
  dirty = true;
}

// --- действия ---

void Game::event(const char* text) {
  if (text != eventText) {
    int i = 0;
    while (text[i] && i < (int)sizeof eventText - 1) { eventText[i] = text[i]; i++; }
    eventText[i] = 0;
  }
  eventSeq++;
}

void Game::say(const char* t, Color c, uint32_t ms) {
  toast = t;
  toastCol = c;
  toastStart = now;
  toastUntil = now + (ms < 4500 ? 4500 : ms);
}

void Game::spawn(uint8_t kind, float x, float y, float vx, float vy, float life, Color col, const char* ch) {
  for (auto& p : parts) {
    if (p.life > 0) continue;
    p.x = x; p.y = y; p.vx = vx; p.vy = vy; p.life = life; p.max = life; p.kind = kind; p.col = col;
    p.ch[0] = 0;
    if (ch) { int i = 0; while (ch[i] && i < 3) { p.ch[i] = ch[i]; i++; } p.ch[i] = 0; }
    return;
  }
}

void Game::burst(float x, float y, int n, Color col) {
  for (int i = 0; i < n; i++) {
    float a = (rndInt(628)) * 0.01f, v = 20 + rndInt(50);
    spawn(0, x, y, fcos(a) * v, fsin(a) * v - 20, 0.6f + rndInt(50) * 0.01f, col);
  }
}

bool Game::refuses() {
  // В спокойном режиме Бит не отвергает заботу пользователя.
  if (s.reserved4[0] == 0) return false;
  int chance = (100 - s.discipline) / 5; // 0..20%, дисциплина снижает отказы
  return rndInt(100) < chance;
}

void Game::startTraining() {
  if (s.asleep || !s.alive || s.form == EGG) {
    say("Пусть Бит сначала проснётся", pal::purple);
    return;
  }
  if (s.energy < 10) {
    say("Мало энергии для обучения", pal::orange);
    return;
  }
  if (s.discipline >= 100) {
    say("Дисциплина уже 100!", pal::green);
    return;
  }
  if (trainReadyAt && (int32_t)(now - trainReadyAt) < 0) {
    uint32_t remaining = (trainReadyAt - now + 59999u) / 60000u;
    sfmt(toastBuf, sizeof toastBuf, "Обучение через %u мин", remaining);
    say(toastBuf, pal::yellow);
    return;
  }
  trainRound = trainCorrect = trainLastOk = 0;
  trainPrompt = (uint8_t)rndInt(2);
  trainPhase = 0;
  trainRoundAt = now;
  scene = S_TRAIN;
  sceneStart = now;
  cursor = -1;
}

void Game::trainingTick() {
  if (trainPhase == 2) return;
  // Промпт длится 4,5 секунды: на пять заданий даётся 22,5 секунды.
  if (now - trainRoundAt < TRAIN_ROUND_MS) return;
  if (trainRound < 4) {
    ++trainRound;
    trainPrompt = (uint8_t)rndInt(2);
    trainPhase = 0;
    trainRoundAt = now;
    return;
  }
  trainPhase = 2;
  // Награда за внимание, а не за бездействие: минимум два верных ответа.
  int reward = trainCorrect >= 2 ? 5 + trainCorrect : 0;
  s.discipline = (int16_t)clampi(s.discipline + reward, 0, 100);
  s.energy = (int16_t)clampi(s.energy - 5, 0, 100);
  trainReadyAt = now + TRAIN_COOLDOWN_MS;
  sfmt(toastBuf, sizeof toastBuf, "Дисциплина +%d", reward);
  say(toastBuf, reward ? pal::green : pal::orange, 5000);
  if (reward) burst(120, 70, 10, pal::cyan);
  dirty = true;
}

void Game::startAction(Action a, uint8_t arg) {
  actionKind = a;
  actionArg = arg;
  actionStart = now;
}

void Game::finishAction() {
  Action a = actionKind;
  actionKind = A_NONE;
  Form f = (Form)s.form;
  switch (a) {
    case A_EAT:
      if (actionArg == 0) {         // кофе
        s.hunger = (int16_t)clampi(s.hunger + 10, 0, 100);
        s.energy = (int16_t)clampi(s.energy + 25, 0, 100);
        say("+энергия: кофе выпит", pal::orange);
      } else if (actionArg == 1) {  // пицца
        s.hunger = (int16_t)clampi(s.hunger + 35, 0, 100);
        s.weight++;
        if (s.nextBugMin > 25) s.nextBugMin = (uint16_t)(10 + rndInt(15));
        say("+сытость: пицца съедена", pal::yellow);
      } else {                      // печенька
        s.happy = (int16_t)clampi(s.happy + 20, 0, 100);
        s.hunger = (int16_t)clampi(s.hunger + 5, 0, 100);
        s.weight += 2;
        if (++s.snacks > 3 && !s.sick && rndInt(3) == 0) {
          s.sick = 1; s.doses = 1;
          say("Переел печенек — баг!", pal::red);
        } else {
          say("Cookie принят ✓", pal::pink);
        }
      }
      if (s.trait == T_GLUTTON) s.hunger = (int16_t)clampi(s.hunger + 5, 0, 100);
      if (actionArg == s.favFood) {
        s.happy = (int16_t)clampi(s.happy + 10, 0, 100);
        if (s.favFed < 65535) s.favFed++;
        say("Любимое! +настроение", pal::pink);
        for (int i = 0; i < 5; i++) spawn(1, petX - 12 + i * 6, 60, (rndInt(20) - 10), -30, 1.4f, pal::pink);
      }
      resolveCall(C_HUNGER);
      burst(petX, 80, 6, pal::yellow);
      break;
    case A_CLEAN:
      s.bugsCleaned = (uint16_t)(s.bugsCleaned + s.bugs);
      s.bugs = 0;
      resolveCall(C_BUGS);
      say("git clean: баги удалены", pal::green);
      break;
    case A_HEAL:
      if (s.doses > 0) s.doses--;
      if (s.doses == 0) {
        s.sick = 0;
        resolveCall(C_SICK);
        say("Баг исправлен!", pal::green);
        burst(petX, 80, 10, pal::green);
      } else {
        say("Нужен ещё один прогон", pal::yellow);
      }
      break;
    case A_REVIEW:
      if (s.fakeCall) {
        s.discipline = (int16_t)clampi(s.discipline + (s.trait == T_SMART ? 35 : 25), 0, 100);
        resolveCall(C_FAKE);
        say("Ревью принято. Дисциплина +", pal::blue);
      } else {
        s.happy = (int16_t)clampi(s.happy - 10, 0, 100);
        say("Зря придрался… обиделся", pal::dim);
      }
      break;
    case A_LIGHT:
      resolveCall(C_LIGHT);
      break;
    default: break;
  }
  (void)f;
  dirty = true;
}

int Game::menuOffset() const {
  constexpr int height = gfx::H - 109;
  uint32_t passed = now - menuTransitionAt;
  if (passed >= MENU_ANIM_MS) return menuOpen ? 0 : height;
  // Нелинейная кривая: мягкое торможение при появлении и разгон при скрытии.
  // Зеркальная функция сохраняет положение при смене направления анимации.
  uint32_t phase = menuOpen ? MENU_ANIM_MS - passed : passed;
  return (int)((phase * phase * height + MENU_ANIM_MS * MENU_ANIM_MS / 2u) /
               (MENU_ANIM_MS * MENU_ANIM_MS));
}

void Game::closeMenu() {
  if (menuOpen) {
    // Если пользователь закрыл панель до конца появления, она не должна подпрыгнуть.
    // Фаза анимации зеркально отражается, чтобы позиция сохранилась.
    uint32_t elapsed = now - menuTransitionAt;
    if (elapsed < MENU_ANIM_MS)
      menuTransitionAt = now - (MENU_ANIM_MS - elapsed);
    else
      menuTransitionAt = now;
  }
  menuOpen = false;
  cursor = -1;
}

void Game::goHome() {
  scene = S_HOME;
  closeMenu();
  sceneStart = now;
}

void Game::openList(uint8_t id) {
  scene = S_LIST;
  listId = id;
  listSel = 0;
  listCount = id == 0 ? (visiting() ? 5 : 4) : (id == 1 ? (visiting() ? 8 : 7) : (id == 3 ? 13 : 15));
  sceneStart = now;
}

void Game::activate(int icon) {
  cursorMs = now;
  bool sleeping = s.asleep != 0;
  switch (icon) {
    case I_FOOD:
      if (sleeping) { say("Тсс… спит", pal::purple); return; }
      openList(0);
      break;
    case I_GAME:
      if (sleeping) { say("Тсс… спит", pal::purple); return; }
      if (s.energy < 10) { say("Нет сил играть", pal::dim); return; }
      openList(1);
      break;
    case I_LIGHT:
      s.lightOff ^= 1;
      lightAnim = now;
      startAction(A_LIGHT);
      if (s.lightOff && !s.asleep) {
        if (s.energy < 100) { s.asleep = 2; say("Уснул", pal::purple); }
        else say("Не хочет спать — полон сил", pal::yellow);
      } else if (!s.lightOff && s.asleep == 2) {
        s.asleep = 0; say("Проснулся", pal::yellow);
      } else if (!s.lightOff && s.asleep == 1) {
        say("Ночь — свет мешает спать", pal::yellow);
      }
      dirty = true;
      break;
    case I_CLEAN:
      if (!s.bugs) { say("Багов нет, чисто", pal::green); return; }
      startAction(A_CLEAN);
      break;
    case I_HEAL:
      if (!s.sick) { startAction(A_NOHEAL); say("Здоров, отладчик не нужен", pal::green); return; }
      startAction(A_HEAL);
      break;
    case I_STATUS:
      scene = S_STATUS; page = 0; sceneStart = now;
      break;
    case I_REVIEW:
      if (s.callKind == C_FAKE) startAction(A_REVIEW); // классика: реагируем на каприз
      else startTraining(); // в остальных случаях — полезная тренировка
      break;
    case I_WALK:
      if (sleeping) { say("Тсс… спит", pal::purple); return; }
      scene = S_WALK; sceneStart = now;
      walking = true; haveLast = false;
      walkStartMeters = s.walkMeters;
      break;
    case I_SHOP:
      openList(3);
      break;
    case I_SETTINGS:
      openList(2);
      break;
  }
}

void Game::listChoose() {
  if (listId == 0) {
    if (listSel == listCount - 1) { goHome(); return; }
    if (listSel == 3) {  // угостить гостя
      goHome();
      giftPending = true;
      s.happy = (int16_t)clampi(s.happy + 5, 0, 100);
      sfmt(toastBuf, sizeof toastBuf, "Угостил: %s", formName((Form)friendForm));
      say(toastBuf, pal::pink);
      jumpAt = now;
      return;
    }
    goHome();
    if (s.hunger >= 100 && listSel != 0) { startAction(A_REFUSE); say("Не лезет, сыт", pal::dim); return; }
    if (refuses()) { startAction(A_REFUSE); say("Не хочу! (нужно ревью)", pal::red); return; }
    startAction(A_EAT, listSel);
    return;
  }
  if (listId == 1) {
    if (listSel == listCount - 1) { goHome(); return; }
    if (listSel == 6) { duelStart(); return; }  // «Дуэль с гостем» — только когда гость рядом
    if (refuses()) { goHome(); startAction(A_REFUSE); say("Не хочу играть!", pal::red); return; }
    startGame(listSel);
    return;
  }
  if (listId == 3) {  // магазин: купить или надеть
    if (listSel == listCount - 1) { goHome(); return; }
    uint8_t it = shopItemAt(listSel);
    if (it == IT_NONE) {
      if (listSel == 0) { s.hat = 0; say("Без шапки", pal::fg); }
      else { s.theme = 0; say("Стандартная тема", pal::fg); }
      dirty = true;
      return;
    }
    bool own = (s.owned >> it) & 1;
    if (!own) {
      if (s.coins < itemPrice(it)) {
        sfmt(toastBuf, sizeof toastBuf, "Не хватает %u монет", (unsigned)(itemPrice(it) - s.coins));
        say(toastBuf, pal::orange);
        return;
      }
      s.coins -= itemPrice(it);
      s.owned |= 1u << it;
      sfmt(toastBuf, sizeof toastBuf, "Куплено: %s", itemName(it));
      say(toastBuf, pal::green);
      burst(120, 60, 12, pal::yellow);
      checkAchievements();
    }
    if (itemIsHat(it)) s.hat = it; else s.theme = it;
    dirty = true;
    return;
  }
  // настройки
  switch (listSel) {
    case 0:
      scene = S_CLOCK; clockMode = 0; clockField = 0; clockH = (uint8_t)hourNow(); clockM = (uint8_t)minNow(); sceneStart = now;
      break;
    case 1:
      scene = S_CLOCK; clockMode = 1; clockField = 0; clockH = s.alarmH; clockM = s.alarmM; clockOn = 1; sceneStart = now;
      break;
    case 2: s.tz = (int8_t)(s.tz >= 12 ? -1 : s.tz + 1); dirty = true; break;
    case 3: s.brightness = (uint8_t)(s.brightness % 3 + 1); dirty = true; break;
    case 4: s.leds ^= 1; dirty = true; break;
    case 5:
      s.nightLight ^= 1;
      // Включение ночника в уже тёмной спальне должно сразу давать свет.
      if (s.nightLight && s.lightOff && s.asleep) lightAnim = now;
      dirty = true;
      break;
    case 6: s.flip ^= 1; dirty = true; break;
    case 7:
      s.gpsAuto ^= 1;
      // Не оставляем старое окно синхронизации после выключения.
      if (s.gpsAuto) { gpsSyncUntil = now + 5 * 60000; stableFix = 0; }
      else { gpsSyncUntil = 0; stableFix = 0; gpsTimeNote = 0; }
      dirty = true;
      break;
    case 8:
      s.reserved4[0] ^= 1;
      s.starveMin = 0; s.sickMin = 0; // смена сложности не должна мгновенно убить питомца
      nextHobby = now + 30u * 60000u; hobbyUntil = 0; hobbyKind = 0;
      say(s.reserved4[0] ? "Классический уход" : "Спокойный уход", pal::cyan);
      dirty = true;
      break;
    case 9: scene = S_GPS; sceneStart = now; break;
    case 10: s.bleOn ^= 1; dirty = true; break;
    case 11: s.loraOn ^= 1; dirty = true; break;
    case 12: scene = S_CONFIRM; confirmMode = 1; listSel = 1; sceneStart = now; break;
    case 13: scene = S_CONFIRM; confirmMode = 0; listSel = 1; sceneStart = now; break;
    default: goHome(); break;
  }
}

void Game::button(Btn b) {
  idleMs = 0;
  if (scene == S_ALARM) {  // любое нажатие гасит будильник
    alarmActive = false;
    event("Будильник выключен");
    if (s.alarmWakes < 255) s.alarmWakes++;
    goHome();
    jumpAt = now;
    say("Доброе утро!", pal::yellow);
    checkAchievements();
    dirty = true;
    return;
  }
  if (b == B_HOLD && scene == S_DUEL) duelState = 0;
  if (b == B_HOLD && scene == S_HOME) {
    closeMenu();
    return;
  }
  if (b == B_HOLD && scene != S_DEATH && scene != S_HATCH && scene != S_EVOLVE) {
    walking = scene == S_WALK ? false : walking;
    goHome();
    return;
  }
  switch (scene) {
    case S_HOME:
      if (actionKind != A_NONE && actionKind != A_POOP) return;
      if (b == B_SHORT || b == B_LONG) {
        if (!menuOpen) {
          menuOpen = true;
          cursor = 0;
          // Переход из ещё не завершённого скрытия — без скачка позиции.
          uint32_t elapsed = now - menuTransitionAt;
          menuTransitionAt = elapsed < MENU_ANIM_MS ?
              now - (MENU_ANIM_MS - elapsed) : now;
        } else if (b == B_SHORT) {
          cursor = (cursor + 1) % ICON_COUNT;
        } else {
          int selected = cursor;
          closeMenu();
          activate(selected);
        }
        cursorMs = now;
      }
      break;
    case S_LIST:
      if (b == B_SHORT) listSel = (uint8_t)((listSel + 1) % listCount);
      else listChoose();
      break;
    case S_STATUS:
      if (b == B_SHORT) page = (uint8_t)((page + 1) % 12);
      else goHome();
      break;
    case S_GAME_DEBUG:
      if (gPhase == 0) {  // попадание?
        float d = gCaret - gZoneX;  // обе координаты в пикселях экрана
        bool hit = d > -gZoneW / 2 - 2 && d < gZoneW / 2 + 2;
        if (hit) { gScore++; burst(gZone + 10, 70, 10, pal::green); }
        gPhase = hit ? 1 : 2;
        gStart = now;
      } else if (gPhase == 3) {
        goHome();
      }
      break;
    case S_TRAIN:
      if (trainPhase == 0 && (b == B_SHORT || b == B_LONG)) {
        trainLastOk = (uint8_t)((b == B_LONG ? 1 : 0) == trainPrompt);
        if (trainLastOk) ++trainCorrect;
        trainPhase = 1;
      } else if (trainPhase == 2) {
        goHome();
      }
      break;
    case S_GAME_BIT:
      if (gPhase == 1) { // запомни последовательность: коротко = 0, долго = 1
        uint8_t pressed = b == B_SHORT ? 0 : 1;
        if (pressed != memoryPattern[memoryCursor]) {
          gResult = 0; gPhase = 2; gStart = now;
        } else if (++memoryCursor >= gRound + 1) {
          gResult = 1; gScore++; gPhase = 2; gStart = now;
        }
      } else if (gPhase == 3) {
        goHome();
      }
      break;
    case S_GAME_DEPLOY:
      if (gPhase == 0) { if (dLift <= 0.01f) { dVy = -190; dLift = 0.5f; } }
      else if (gPhase == 3) goHome();
      break;
    case S_GAME_PING:
      if (gPhase == 0) {
        float d = fabs_(pRing - 14);
        gResult = d <= 3 ? 2 : (d <= 8 ? 1 : 0);
        gScore += gResult;
        if (gResult == 2) burst(160, 72, 10, pal::green);
        gPhase = 1; gStart = now;
      } else if (gPhase == 3) goHome();
      break;
    case S_DUEL:
      if (duelState == 2) duelAnswer(b == B_LONG);
      else if (duelState == 3) duelPress();
      else if (duelState == 5) { duelState = 0; goHome(); }
      break;
    case S_GAME_REACT:
      if (gPhase == 0) {
        if (now < rGoAt) { gResult = 0; rTime = 0; }  // фальстарт
        else {
          rTime = now - rGoAt;
          gResult = rTime < 250 ? 2 : (rTime < 400 ? 1 : 0);
          gScore += gResult;
          uint32_t cs = rTime / 10;
          if (cs < 1) cs = 1;
          if (!s.bestReactCs || cs < s.bestReactCs) s.bestReactCs = (uint8_t)(cs > 255 ? 255 : cs);
        }
        gPhase = 1; gStart = now;
      } else if (gPhase == 3) goHome();
      break;
    case S_GAME_COMPILE:
      if (gPhase == 0) {
        if (cProg < cZone) gResult = 0;
        else if (cProg <= cZone + cZoneW) { gResult = 1; gScore++; burst(18 + 204 * cProg, 66, 10, pal::green); }
        else gResult = 2;
        gPhase = 1; gStart = now;
      } else if (gPhase == 3) goHome();
      break;
    case S_WALK:
      if (b == B_LONG) { walking = false; goHome(); }
      break;
    case S_GPS:
      if (b == B_LONG) goHome();
      break;
    case S_CLOCK: {
      int fields = clockMode ? 3 : 2;
      if (b == B_SHORT) {
        if (clockField == 0) clockH = (uint8_t)((clockH + 1) % 24);
        else if (clockField == 1) clockM = (uint8_t)((clockM + (clockMode ? 5 : 1)) % 60);
        else clockOn ^= 1;
      } else if (clockField + 1 < fields) {
        clockField++;
      } else if (clockMode == 0) {
        uint32_t day = s.epoch / 86400;
        s.epoch = day * 86400 + clockH * 3600u + clockM * 60u;
        s.clockSource = 1;
        clockSure = true;
        dirty = true;
        goHome();
        say("Время установлено", pal::green);
      } else {
        s.alarmH = clockH; s.alarmM = clockM; s.alarmOn = clockOn;
        dirty = true;
        goHome();
        if (clockOn) {
          sfmt(toastBuf, sizeof toastBuf, "Будильник на %02d:%02d", clockH, clockM);
          say(toastBuf, pal::green);
        } else {
          say("Будильник выключен", pal::dim);
        }
      }
      break;
    }
    case S_CONFIRM:
      if (b == B_SHORT) listSel ^= 1;
      else if (listSel == 0 && confirmMode == 1) { resetWalks(); goHome(); say("Прогулки обнулены", pal::green); }
      else if (listSel == 0) init(rnd());
      else goHome();
      break;
    case S_DEATH:
      if (b == B_LONG && now - sceneStart > 3000) init(rnd());
      break;
    default: break;
  }
}

// у каждой просьбы свой цвет
uint32_t Game::callColor() const {
  switch (s.callKind) {
    case C_HUNGER: return 0xFF6000;
    case C_HAPPY: return 0x2060FF;
    case C_LIGHT: return 0xFFC000;
    case C_SICK: return 0xFF0010;
    case C_BUGS: return 0xA020FF;
    case C_FAKE: return 0x00C0C0;
    default: return 0;
  }
}

static uint32_t scaleRgb(uint32_t c, int k) {  // k 0..256
  uint32_t r = ((c >> 16) & 255) * k >> 8, g = ((c >> 8) & 255) * k >> 8, b = (c & 255) * k >> 8;
  return r << 16 | g << 8 | b;
}

uint32_t Game::nightLightColor() const {
  // Ночник — отдельная функция, не зависит от переключателя эффектов «Светодиоды».
  // Он светит 10 минут после выключения света и плавно гаснет ещё одну минуту.
  if (!s.nightLight || !s.lightOff || !s.asleep) return 0;
  const uint32_t t = now - lightAnim;
  static constexpr uint32_t WARM = 0x482000; // спокойный янтарный свет двух RGB LED
  if (t < 600000u) return WARM;
  if (t < 660000u) return scaleRgb(WARM, (int)((660000u - t) * 256u / 60000u));
  return 0;
}

uint32_t Game::ledColorScreenOff() const {
  uint32_t night = nightLightColor();
  if (night) return night; // никакие просьбы не прерывают свет ночника
  if (!s.leds) return 0;
  // При выключенном экране оставляем только короткую вспышку просьбы раз в 4 сек.
  if (attention() && (now % 4000u) < 120u) return callColor();
  return 0;
}

uint32_t Game::ledColor() const {
  if (scene == S_ALARM && s.leds) {  // будильник заметнее ночника
    static const uint32_t rb[] = {0xFF0000, 0xFF8000, 0xFFFF00, 0x00FF00, 0x00A0FF, 0x8000FF};
    return rb[(now / 150) % 6];
  }
  uint32_t night = nightLightColor();
  if (night) return night;
  if (!s.leds) return 0;
  if (scene == S_GAME_REACT && gPhase == 0) return now >= rGoAt ? 0x00FF20 : 0x400000;
  if (now < ledFlashUntil) return ((now / 200) & 1) ? ledFlashColor : 0;
  if (attention() && !(s.asleep && s.callKind != C_LIGHT)) {
    uint32_t c = callColor();
    if (s.callKind == C_SICK) return ((now / 150) & 1) ? c : 0;
    int k = (isin((int)(now / 4 % 360)) + 256) / 2;
    return scaleRgb(c, 30 + k * 200 / 256);
  }
  if (s.asleep) return 0;
  if (scene == S_EVOLVE) {
    int ph = (now / 120) % 3;
    return ph == 0 ? 0xFF40C0 : (ph == 1 ? 0x40C0FF : 0xFFFF40);
  }
  return 0;
}

// --- демонстрации для сайта ---

void Game::demo(int what) {
  if (s.magic != MAGIC) init(42);
  s.alive = 1; s.sick = 0; s.bugs = 0; s.asleep = 0; s.lightOff = 0;
  s.hunger = 80; s.happy = 80; s.energy = 80; s.callKind = 0;
  scene = S_HOME; actionKind = A_NONE; cursor = -1;
  menuOpen = false; menuTransitionAt = now - MENU_ANIM_MS;
  for (auto& p : parts) p.life = 0;
  toastUntil = 0;
  friendId = 0; visitUntil = 0;
  switch (what) {
    case 0: break;
    case 1: s.form = JUNIOR; s.hunger = 3; s.happy = 30; updateCalls(); break;          // голод
    case 2: s.form = MIDDLE; s.sick = 1; s.doses = 2; s.bugs = 3; updateCalls(); break;   // болезнь
    case 3: s.form = SENIOR; s.asleep = 1; s.lightOff = 1; break;                         // сон
    case 4: s.form = HACKER; s.happy = 4; updateCalls(); break;                           // грусть
    case 5: evoFrom = MIDDLE; evoTo = ARCHITECT; s.form = ARCHITECT; scene = S_EVOLVE; sceneStart = now; break;
    case 6: s.form = LEGACY; die(D_SICK); break;
    case 7: s.form = AI; break;
    case 8: s.form = FREELANCER; activate(I_WALK); fix.on = true; fix.sats = 7; fix.posValid = true; break;
    case 9: case 10: case 11:  // проверка GPS: нет связи / поиск / координаты есть
      s.form = MIDDLE; scene = S_GPS; sceneStart = now; gpsOnSince = now - 10000; gpsWanted = true;
      fix = GpsFix(); fix.on = true;
      if (what >= 10) { fix.alive = true; fix.inView = 11; fix.sats = 2; fix.timeValid = true; fix.utc = s.epoch - s.tz * 3600; }
      if (what == 11) { fix.sats = 9; fix.posValid = true; fix.hdop = 1.3f; fix.inView = 17; }
      break;
    case 12:  // прогулка, поиск спутников
      s.form = FREELANCER; activate(I_WALK); gpsOnSince = now - 10000; gpsWanted = true;
      fix = GpsFix(); fix.on = true; fix.alive = true; fix.inView = 6; fix.sats = 1;
      break;
    default: break;
  }
}

}  // namespace pet
