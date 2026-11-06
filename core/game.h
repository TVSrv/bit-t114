// Тамагочи «Бит»: логика, экраны и анимации. Не знает о железе: платформа даёт время,
// кнопку, батарею и GPS, забирает кадр, цвет светодиодов и сохранение.
#pragma once
#include "art.h"
#include "gps.h"

namespace pet {

enum Call : uint8_t { C_NONE, C_HUNGER, C_HAPPY, C_LIGHT, C_SICK, C_BUGS, C_TIRED, C_FAKE };
enum Death : uint8_t { D_NONE, D_SICK, D_STARVE, D_OLD };

inline int stageOf(Form f) {
  switch (f) {
    case EGG: return 0;
    case BABY: return 1;
    case JUNIOR: return 2;
    case MIDDLE: case FREELANCER: return 3;
    default: return 4;
  }
}
inline int baseWeight(Form f) {
  static const int w[] = {0, 5, 10, 20, 30};
  return w[stageOf(f)];
}

// Долгая жизнь: время каждой стадии считается отдельно, с момента предыдущей эволюции.
static constexpr uint32_t EVO_EGG_SECONDS = 10u * 60u;
static constexpr uint32_t EVO_BABY_MINUTES = 12u * 60u;
static constexpr uint32_t EVO_JUNIOR_MINUTES = 7u * 24u * 60u;
static constexpr uint32_t EVO_MIDDLE_MINUTES = 10u * 24u * 60u;
static constexpr uint32_t EVO_EXPERT_MINUTES = 14u * 24u * 60u;

// Пределы жизни увеличены вместе с взрослением, иначе питомец не доживёт до AI.
// Дополнительные 0..2 суток задаются seed, как и раньше.
inline uint32_t lifeDays(Form f) {
  switch (f) {
    case ARCHITECT: return 47;
    case HACKER: return 45;
    case SENIOR: return 38;
    case LEGACY: return 32;
    case AI: return 65;
    default: return 0;
  }
}

enum Btn : uint8_t { B_SHORT, B_LONG, B_HOLD };  // короткое, долгое (0,6 с), удержание (2 с) — домой

// то, что хранится во флеш-памяти
struct Save {
  uint32_t magic;
  uint16_t version;
  uint8_t form, alive;
  uint32_t ageMin, stageMin;
  int16_t hunger, happy, energy, discipline, weight;
  uint8_t bugs, sick, doses, mistakes;
  uint8_t lightOff, asleep, snacks, causeOfDeath;
  uint16_t sickMin, starveMin;
  uint8_t callKind, fakeCall;
  uint16_t callMin, nextBugMin, nextFakeMin;
  uint32_t epoch;        // местное время, секунды с 2000-01-01
  uint8_t clockSource;   // 0 не задано, 1 вручную, 2 GPS
  int8_t tz;             // часовой пояс, часы
  uint8_t brightness, leds, flip, gpsAuto;
  uint32_t walkMeters, totalMeters;
  uint16_t generation, gamesWon, gamesPlayed, careScore;
  uint32_t seed;
  // --- версия 2 ---
  uint32_t achievements;   // полученные достижения (биты Ach), общие для всех поколений
  uint16_t formsSeen;      // выращенные формы (биты Form), общие
  uint16_t totalWins;      // победы в играх за всё время
  uint16_t bugsCleaned;    // убрано багов за всё время
  uint16_t favFed;         // сколько раз кормили любимым
  uint8_t streak;          // дней подряд без ошибок ухода
  uint8_t mistakesToday;
  uint32_t lastDay;        // номер дня для подсчёта серии
  uint8_t alarmOn, alarmH, alarmM, alarmWakes;
  uint8_t favFood, favGame, trait;      // характер: любимая еда (0–2), игра (0–4), черта (Trait)
  uint8_t parentForm, parentTrait, inherited;  // наследство: от кого и что досталось
  uint8_t reserved[8];
  // --- версия 3 ---
  int32_t questLat, questLon;   // цель GPS-квеста, микроградусы
  uint8_t questKind;            // 0 нет, 1 баг, 2 кофе
  uint8_t placeHead;            // куда писать следующее место в кольцо places
  uint16_t questsDone, placesTotal;
  uint32_t places[24];          // посещённые места (ячейки ~200 м)
  uint8_t bleOn, loraOn, nightLight, bestReactCs;  // bestReactCs — лучшая реакция, сотые доли секунды
  uint16_t friendsMet, visits;
  uint32_t friends[8];          // кого встречали по LoRa
  uint32_t deviceId;
  uint8_t reserved3[8];
  // --- версия 4 ---
  uint32_t totalSteps, stepsToday;   // шаги с телефона
  uint32_t coins;                    // монеты
  uint32_t owned;                    // купленные вещи (биты Item)
  uint8_t hat, theme;                // надетая шапка (Item или 0), тема редактора (0 — стандартная)
  uint16_t bonusDay;                 // день последнего праздничного бонуса (номер дня от 2000 года, младшие 16 бит)
  uint16_t lastBirthdayWeek;         // последний отмеченный «день рождения» (полных недель жизни)
  uint8_t reserved4[10];       // [0]: режим ухода 0 спокойный (по умолчанию), 1 классический; [1]: занятия Бита
  uint32_t crc;
};

// первая версия сохранения — для переноса старого питомца
struct SaveV1 {
  uint32_t magic;
  uint16_t version;
  uint8_t form, alive;
  uint32_t ageMin, stageMin;
  int16_t hunger, happy, energy, discipline, weight;
  uint8_t bugs, sick, doses, mistakes;
  uint8_t lightOff, asleep, snacks, causeOfDeath;
  uint16_t sickMin, starveMin;
  uint8_t callKind, fakeCall;
  uint16_t callMin, nextBugMin, nextFakeMin;
  uint32_t epoch;
  uint8_t clockSource;
  int8_t tz;
  uint8_t brightness, leds, flip, gpsAuto;
  uint32_t walkMeters, totalMeters;
  uint16_t generation, gamesWon, gamesPlayed, careScore;
  uint32_t seed;
  uint32_t crc;
};

// черты характера
void nickName(uint32_t id, char* out, int n);

enum Trait : uint8_t { T_GLUTTON, T_FIDGET, T_SLEEPY, T_SMART, T_CALM, TRAIT_COUNT };
const char* traitName(uint8_t t);
const char* traitHint(uint8_t t);

// достижения; у некоторых есть аксессуар, который носят все питомцы
enum Ach : uint8_t {
  ACH_KM1, ACH_KM10, ACH_WEEK, ACH_GAMER, ACH_ADULT, ACH_OLD, ACH_COLLECTOR, ACH_AI, ACH_CLEAN, ACH_ALARM, ACH_GOURMET,
  ACH_EXPLORER, ACH_HUNTER, ACH_REFLEX, ACH_FRIEND, ACH_SOCIAL, ACH_STEPS, ACH_SHOPPER, ACH_COUNT
};
const char* achName(uint8_t a);
const char* achHint(uint8_t a);

// магазин: шапки (надевается одна) и темы редактора
enum Item : uint8_t {
  IT_NONE, IT_BEANIE, IT_CAP, IT_TOPHAT, IT_COWBOY, IT_WIZARD, IT_CATEARS,   // шапки 1–6
  IT_TH_DRACULA, IT_TH_MONOKAI, IT_TH_LIGHT, IT_TH_MATRIX, IT_COUNT           // темы 7–10
};
const char* itemName(uint8_t it);
uint16_t itemPrice(uint8_t it);
inline bool itemIsHat(uint8_t it) { return it >= IT_BEANIE && it <= IT_CATEARS; }
uint8_t shopItemAt(int row);

// праздник по дате
enum Holiday : uint8_t { HD_NONE, HD_NEWYEAR, HD_PROGRAMMER, HD_HALLOWEEN, HD_BIRTHDAY };

// игры
enum GameId : uint8_t { G_DEBUG, G_BIT, G_DEPLOY, G_PING, G_COMPILE, G_REACT, GAME_COUNT };

enum Scene : uint8_t {
  S_HOME, S_LIST, S_GAME_DEBUG, S_GAME_BIT, S_STATUS, S_WALK, S_CLOCK, S_EVOLVE, S_DEATH, S_HATCH, S_CONFIRM, S_GPS,
  S_GAME_DEPLOY, S_GAME_PING, S_GAME_COMPILE, S_ALARM, S_GAME_REACT, S_DUEL, S_TRAIN
};

enum Action : uint8_t { A_NONE, A_EAT, A_CLEAN, A_HEAL, A_REVIEW, A_REFUSE, A_LIGHT, A_PRAISE, A_POOP, A_NOHEAL };

struct Particle {
  float x, y, vx, vy, life, max;
  uint8_t kind;  // 0 искра, 1 сердце, 2 символ, 3 z, 4 крошка
  uint16_t col;
  char ch[4];
};

class Game {
 public:
  Save s;

  void init(uint32_t seed);          // новая игра (или после неудачной загрузки)
  bool load(const Save& saved);      // false — данные повреждены
  bool loadRaw(const uint8_t* data, size_t n);  // любая версия сохранения; старые переносятся
  void tick(uint32_t dtMs);          // время идёт; можно большими шагами (ускорение)
  void button(Btn b);
  void render(Canvas& c);

  // платформа -> игра
  void setBattery(int pct, bool charging) { batPct = pct; batCharging = charging; }
  void gpsFeed(const GpsFix& fix);
  void wake() { idleMs = 0; }
  void wakeToInfo();  // первое нажатие при погашенном дисплее: часы/погода, без игрового действия

  // игра -> платформа
  bool wantsGps() const;
  bool attention() const { return s.callKind != 0 && s.alive; }
  bool dirty = false;                // надо сохранить
  bool saveNow = false;              // сохранить немедленно (перед обновлением прошивки)
  uint32_t hwId = 0;                 // заводской номер чипа (задаёт платформа): ник и id для встреч
  const char* nick();
  uint32_t ledColor() const;         // 0xRRGGBB для двух светодиодов
  uint32_t nightLightColor() const;   // независимый тёплый свет ночника
  uint32_t ledColorScreenOff() const; // цвет светодиодов при погашенном экране
  uint32_t callColor() const;        // цвет просьбы (для вспышек при погашенном экране)
  uint32_t idleMs = 0;               // сколько без нажатий (для гашения экрана)
  bool screenAllowedOff() const { return (scene == S_HOME || scene == S_HATCH) && actionKind == A_NONE && !alarmActive; }
  uint32_t gpsOnSince = 0, gpsHintAt = 0;
  bool gpsWanted = false;
  bool clockSure = false;
  uint16_t stableFix = 0;            // сколько отчётов подряд есть привязка с 4+ спутниками
  uint8_t gpsTimeNote = 0;           // 0 нет, 1 ждём устойчивой привязки, 2 время похоже на «с включения», 3 принято            // время подтверждено после включения (GPS, вручную, телефон)           // когда игра попросила включить GPS (для «модуль не отвечает»)
  bool justEvolved = false;          // надо включить экран (эволюция, будильник)

  // Bluetooth: текстовые команды с телефона и события для отправки
  int command(const char* line, char* out, int outSize);
  // приложение: повторить на телефоне экран платы по строке «data» (поля без «D|3|»)
  void mirror(const int32_t* v, int n);
  uint16_t eventSeq = 0;             // растёт при каждом новом событии
  char eventText[72] = {0};
  void event(const char* text);

  // встречи по LoRa: игра решает, когда слать и слушать; платформа гоняет радио
  bool loraWantTx(uint8_t* buf, int& len);   // есть пакет для отправки
  void loraRx(const uint8_t* buf, int len, int rssi);
  bool loraListen() const;                   // держать приёмник включённым
  bool visiting() const { return s.loraOn && friendId && now < visitUntil; }

  // для демо на сайте
  void demo(int what);
  uint32_t speed = 1;                // множитель игрового времени

  static uint32_t crc32(const void* d, size_t n);

 private:
  Scene scene = S_HOME;
  uint32_t now = 0;        // мс с запуска, для анимаций
  uint32_t secAcc = 0, minAcc = 0;
  int batPct = 100;
  bool batCharging = false;
  uint32_t rng = 1;
  uint32_t rnd();
  int rndInt(int n) { return (int)(rnd() % (uint32_t)n); }

  // домашний экран
  // Нижний бар — временный оверлей, в обычном режиме полностью скрыт.
  static constexpr uint32_t MENU_ANIM_MS = 240u;
  static constexpr uint32_t MENU_IDLE_MS = 7000u;
  int cursor = -1;
  uint32_t cursorMs = 0;
  bool menuOpen = false;
  bool infoScreen = false;  // второй домашний экран: часы и погода (не в Save)
  uint32_t menuTransitionAt = 0;
  void closeMenu();
  int menuOffset() const;  // 0 — виден, 26 — целиком за экраном
  float petX = 120, petTarget = 120, petVX = 0;
  int petDir = 1;
  uint32_t nextBlink = 0, blinkUntil = 0, nextWander = 0, nextLook = 0;
  float lookX = 0, lookY = 0;
  float walkPh = 0;
  uint32_t jumpAt = 0;
  float bugX[4];
  uint32_t lightAnim = 0;

  // действия
  Action actionKind = A_NONE;
  uint32_t actionStart = 0;
  uint8_t actionArg = 0;
  const char* toast = nullptr;
  char toastBuf[64];
  uint32_t toastUntil = 0, toastStart = 0;
  Color toastCol = 0;

  // списки
  uint8_t listId = 0, listSel = 0, listCount = 0;
  // Экспериментальный спокойный режим: самостоятельные занятия без наказания за отсутствие внимания.
  uint8_t hobbyKind = 0; // 1 чтение, 2 проекты, 3 отдых, 4 танцы
  uint32_t hobbyUntil = 0, nextHobby = 0;
  // мини-игры
  uint8_t gRound = 0, gScore = 0, gPhase = 0, gGuess = 0, gBit = 0;
  uint8_t memoryPattern[5] = {0}, memoryCursor = 0;
  uint32_t gStart = 0;
  // Обучение дисциплине: пять заданий за ~23 секунды.
  uint8_t trainRound = 0, trainCorrect = 0, trainPrompt = 0, trainPhase = 0, trainLastOk = 0;
  uint32_t trainRoundAt = 0, trainReadyAt = 0;
  static constexpr uint32_t TRAIN_ROUND_MS = 4500u;
  static constexpr uint32_t TRAIN_COOLDOWN_MS = 30u * 60000u;
  float gZone = 0, gZoneW = 0, gSpeed = 0, gCaret = 0, gZoneX = 0;
  // новые игры
  float dLift = 0, dVy = 0, dBugs[3] = {0, 0, 0}, dSpeed = 0, dScroll = 0, pRing = 0, cProg = 0, cZone = 0, cZoneW = 0;
  uint8_t gMax = 5, gResult = 0;
  uint32_t rGoAt = 0, rTime = 0;
  // статус, часы, подтверждение, будильник
  uint8_t page = 0, clockField = 0, clockH = 12, clockM = 0, clockMode = 0, clockOn = 0;
  bool alarmActive = false;
  uint32_t ledFlashUntil = 0;
  uint32_t ledFlashColor = 0;
  // эволюция
  Form evoFrom = EGG, evoTo = EGG;
  uint32_t sceneStart = 0;
  // встречи
  uint32_t nextBeacon = 0, listenUntil = 0, visitUntil = 0, nextDuelTx = 0;
  uint32_t friendId = 0, friendSeenAt[8] = {0}, friendSeenId[8] = {0};
  uint8_t friendForm = 0, friendTrait = 0;
  uint16_t friendGen = 0;
  int friendRssi = 0;
  float friendX = 50, friendTarget = 50, friendWalk = 0;
  int friendDir = 1;
  bool giftPending = false;
  // дуэль по LoRa: «Реакция» на двух платах
  uint8_t duelState = 0;     // 0 нет, 1 вызвал и жду ответа, 2 меня вызвали, 3 жду зелёного, 4 нажал — жду соперника, 5 итог
  uint8_t duelTx = 0;        // какой пакет дуэли отправить
  uint8_t duelTxLeft = 0;    // сколько раз повторить (пакеты в эфире теряются)
  uint32_t duelGoAt = 0, duelSince = 0, duelMy = 0, duelTheir = 0, duelDelay = 0, duelPeer = 0;
  void duelStart();
  void duelAnswer(bool yes);
  void duelPress();
  void duelTick();
  void drawDuel(Canvas& c);
  void drawFriend(Canvas& c);
  // оценка батареи: замеры раз в 10 минут за последние 6 часов
  uint8_t batSamples[36] = {0};
  uint8_t batCount = 0;
  uint32_t batLastSample = 0;
  int batteryHoursLeft() const;  // -1 — пока неизвестно
  // прогулка и квесты
  float questDist = -1, questBearing = 0, walkCourse = 0;
  bool courseKnown = false;
  void questStep(const GpsFix& f);
  GpsFix fix;
  bool walking = false;
  uint32_t walkStartMeters = 0;
  double lastLat = 0, lastLon = 0;
  bool haveLast = false;
  uint32_t gpsSyncUntil = 0, nextGpsSync = 0;
  float walkScroll = 0;

  Particle parts[24] = {};

  // логика
  void minute();
  void second();
  void evolveCheck();
  void die(uint8_t cause);
  void setCall(uint8_t kind);
  void resolveCall(uint8_t kind);
  void updateCalls();
  bool sleepTime() const;
  int hourNow() const { return (int)((s.epoch / 3600) % 24); }
  int minNow() const { return (int)((s.epoch / 60) % 60); }

  void startAction(Action a, uint8_t arg = 0);
  void finishAction();
  void say(const char* t, Color c, uint32_t ms = 4500);
  void spawn(uint8_t kind, float x, float y, float vx, float vy, float life, Color col, const char* ch = nullptr);
  void burst(float x, float y, int n, Color col);
  void goHome();
  void openList(uint8_t id);
  void listChoose();
  void activate(int icon);
  bool refuses();
  void startTraining();
  void trainingTick();
  // характер, достижения, игры
  void checkAchievements();
  void unlock(uint8_t a);
  bool has(uint8_t a) const { return (s.achievements >> a) & 1; }
  void startGame(uint8_t id);
  void gameOver(uint8_t id, int score, int max);
  void triggerAlarm();
  void resetWalks();
  void addMeters(uint32_t d, bool rewards);   // прогулка: GPS или шаги с телефона
  void addSteps(uint32_t n);
  void addCoins(int n, bool show);
  Holiday holiday() const;
  // погода с телефона (не сохраняется: живёт 3 часа)
  uint8_t weather = 0;               // 0 нет, 1 ясно, 2 облачно, 3 дождь, 4 снег, 5 гроза, 6 туман
  int8_t tempC = 0;
  uint32_t weatherUntil = 0;
  bool hasWeather() const { return weather && now < weatherUntil; }
  void drawWeather(Canvas& c, bool dark);
  void holidayCheck();
  uint8_t confirmMode = 0;
           // 0 — новый питомец, 1 — сброс прогулок
  float lifeFrac() const;

  // отрисовка
  void drawBackground(Canvas& c, bool dark);
  void drawTopBar(Canvas& c, bool vitals, bool showTime = true);
  void drawMenuBar(Canvas& c);
  void drawHome(Canvas& c);
  void drawInfoScreen(Canvas& c);
  void drawPetHome(Canvas& c);
  bool artMeal(uint32_t at);
  void drawAction(Canvas& c);
  void drawParticles(Canvas& c, uint32_t dt);
  void drawList(Canvas& c);
  void drawStatus(Canvas& c);
  void drawGameDebug(Canvas& c);
  void drawGameBit(Canvas& c);
  void drawTraining(Canvas& c);
  void drawWalk(Canvas& c);
  void drawClock(Canvas& c);
  void drawEvolve(Canvas& c);
  void drawDeath(Canvas& c);
  void drawHatch(Canvas& c);
  void drawConfirm(Canvas& c);
  void drawGps(Canvas& c);
  void drawGameDeploy(Canvas& c);
  void drawGamePing(Canvas& c);
  void drawGameCompile(Canvas& c);
  void drawGameReact(Canvas& c);
  void drawGameResult(Canvas& c, const char* title, const char* good, const char* bad);
  void drawAlarm(Canvas& c);
  void drawAchievements(Canvas& c, int first, int count);
  void drawCollection(Canvas& c);
  void gpsPanel(Canvas& c, int x, int y, int w, bool big);
  void drawToast(Canvas& c);
  void bar(Canvas& c, int x, int y, int w, int v, Color col, const char* label);
  Pose basePose();
  uint32_t lastRender = 0;
  uint32_t lastPetMs = 0, lastWalkMs = 0, lastFriendMs = 0;  // свои часы анимации у каждого экземпляра игры
  uint8_t trick = 0;  // чем занят без дела: см. drawPetHome
  uint32_t trickAt = 0, nextTrick = 0;
  uint32_t deskUntil = 0, nextDesk = 0;  // вечером садится за компьютер: до какого времени и когда снова
};

}  // namespace pet
