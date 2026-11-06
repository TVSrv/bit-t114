// Тамагочи «Бит» на Heltec Mesh Node T114.
// Кнопка: коротко — дальше, долго (0,6 с) — выбрать, удержание (2 с) — домой.
#include <Arduino.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <Adafruit_NeoPixel.h>
#include "display.h"
#include "bt_link.h"
#include "lora_link.h"
#include "game.h"

using namespace Adafruit_LittleFS_Namespace;

static gfx::Color fb[gfx::W * gfx::H];
static gfx::Canvas canvas(fb);
static Display tft;
static pet::Game game;
static pet::Nmea nmea;
static Adafruit_NeoPixel pixels(NEOPIXEL_NUM, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
static BleLink ble;
static LoraLink lora;

static const char* SAVE_FILE = "/bit.sav";
static constexpr uint32_t SCREEN_TIMEOUT = 30000;  // гасить экран без нажатий
static constexpr uint32_t SAVE_EVERY = 60000;

static uint32_t lastTick, lastSave, lastBattery, lastGpsByte, gpsOnAt;
static bool gpsOn = false, gpsSwapped = false;
static uint8_t flipState = 0, brightState = 0;

// --- сохранение ---

static void saveGame() {
  game.s.crc = pet::Game::crc32(&game.s, offsetof(pet::Save, crc));
  InternalFS.remove(SAVE_FILE);
  File f(InternalFS);
  if (f.open(SAVE_FILE, FILE_O_WRITE)) {
    f.write((const uint8_t*)&game.s, sizeof(game.s));
    f.close();
  }
  game.dirty = false;
  lastSave = millis();
}

static bool loadGame() {
  File f(InternalFS);
  if (!f.open(SAVE_FILE, FILE_O_READ)) return false;
  static uint8_t buf[sizeof(pet::Save)];
  int n = f.read(buf, sizeof(buf));
  f.close();
  return n > 0 && game.loadRaw(buf, (size_t)n);  // старая версия переносится
}

// --- радиомодуль: усыпить, чтобы не ел батарею (≈0,6 мА в ожидании) ---

static void loraSleep() {
  pinMode(LORA_CS, OUTPUT); digitalWrite(LORA_CS, HIGH);
  pinMode(LORA_SCK, OUTPUT); digitalWrite(LORA_SCK, LOW);
  pinMode(LORA_MOSI, OUTPUT);
  pinMode(LORA_BUSY, INPUT);
  pinMode(LORA_RESET, OUTPUT);
  digitalWrite(LORA_RESET, LOW); delay(2);
  digitalWrite(LORA_RESET, HIGH);
  uint32_t t = millis();
  while (digitalRead(LORA_BUSY) && millis() - t < 50) {}
  digitalWrite(LORA_CS, LOW);
  const uint8_t cmd[2] = {0x84, 0x00};  // SetSleep, холодный старт
  for (uint8_t b : cmd)
    for (int i = 7; i >= 0; i--) {
      digitalWrite(LORA_MOSI, (b >> i) & 1);
      digitalWrite(LORA_SCK, HIGH);
      digitalWrite(LORA_SCK, LOW);
    }
  digitalWrite(LORA_CS, HIGH);
}

// --- батарея ---

static void readBattery() {
  digitalWrite(ADC_CTRL, HIGH);
  delay(2);
  analogReference(AR_INTERNAL_3_0);
  analogReadResolution(12);
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(BATTERY_PIN);
  digitalWrite(ADC_CTRL, LOW);
  float v = sum / 8.0f / 4096.0f * 3.0f * ADC_MULTIPLIER;
  // приблизительная кривая разряда LiPo
  int pct;
  if (v >= 4.15f) pct = 100;
  else if (v >= 3.85f) pct = 70 + (int)((v - 3.85f) / 0.30f * 30);
  else if (v >= 3.70f) pct = 30 + (int)((v - 3.70f) / 0.15f * 40);
  else if (v >= 3.40f) pct = (int)((v - 3.40f) / 0.30f * 30);
  else pct = 0;
  bool usb = NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk;
  game.setBattery(pct, usb);
}

// --- GPS ---

// команда приёмнику L76K (протокол CASIC): $<тело>*<XOR>\r\n
static void gpsSend(const char* body) {
  uint8_t x = 0;
  for (const char* p = body; *p; p++) x ^= (uint8_t)*p;
  static const char hexd[] = "0123456789ABCDEF";
  char tail[6] = {'*', hexd[x >> 4], hexd[x & 15], '\r', '\n', 0};
  Serial1.write('$');
  Serial1.write(body);
  Serial1.write(tail);
}

static uint32_t gpsOffAt = 0, gpsConfAt = 0, gpsPoweredAt = 0;
static bool gpsPowered = false;
static constexpr uint32_t GPS_KEEP_WARM = 30 * 60000;  // столько держим в режиме ожидания для горячего старта

static void gpsPower(bool on) {
  if (on == gpsOn) return;
  gpsOn = on;
  if (on) {
    bool cold = !gpsPowered;
    if (cold) gpsPoweredAt = millis();
    digitalWrite(GPS_POWER, HIGH);
    digitalWrite(GPS_STANDBY, HIGH);  // разбудить
    gpsPowered = true;
    if (gpsSwapped) Serial1.setPins(PIN_SERIAL1_TX, PIN_SERIAL1_RX);
    Serial1.begin(9600);
    nmea = pet::Nmea();
    gpsOnAt = lastGpsByte = millis();
    gpsConfAt = millis() + (cold ? 800 : 200);  // настройку шлём, когда модуль проснётся
  } else {
    Serial1.end();
    digitalWrite(GPS_STANDBY, LOW);   // ожидание: эфемериды сохраняются, следующий старт быстрее
    gpsOffAt = millis();
  }
  pet::GpsFix f = nmea.fix;
  f.on = on;
  f.alive = false;
  if (!on) { f.posValid = false; f.sats = 0; f.inView = 0; }
  game.gpsFeed(f);
}

static void gpsConfigure() {
  gpsSend("PCAS04,7");                          // GPS + BeiDou + ГЛОНАСС
  delay(50);
  gpsSend("PCAS03,1,0,0,1,1,0,0,0,0,0,,,0,0");  // выдавать GGA, GSV, RMC
  delay(50);
  gpsSend("PCAS11,2");                          // динамика «пешеход»
}

static uint32_t lastGpsReport = 0;
static void gpsPoll() {
  bool any = false;
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    any = true;
    nmea.feed(c);
  }
  uint32_t t = millis();
  if (any) lastGpsByte = t;
  if (gpsConfAt && t >= gpsConfAt) { gpsConfigure(); gpsConfAt = 0; }
  if (t - lastGpsReport >= 500) {  // дважды в секунду: состояние связи и спутники
    lastGpsReport = t;
    pet::GpsFix f = nmea.fix;
    f.on = true;
    f.alive = t - lastGpsByte < 3000;
    f.moduleUp = (t - gpsPoweredAt) / 1000;
    game.gpsFeed(f);
  }
  // молчит 6 с после включения — пробуем поменять RX/TX местами
  if (!any && t - lastGpsByte > 6000 && t - gpsOnAt > 6000) {
    gpsSwapped = !gpsSwapped;
    gpsPower(false);
    gpsPower(true);
  }
}

static void gpsIdle() {
  // долго не нужен — снимаем питание совсем
  if (!gpsOn && gpsPowered && millis() - gpsOffAt > GPS_KEEP_WARM) {
    digitalWrite(GPS_POWER, LOW);
    gpsPowered = false;
  }
}

// --- кнопка ---

// нажатие ловим прерыванием: пока экран погашен, цикл спит по 50 мс и короткое нажатие можно пропустить
static volatile bool pressLatch = false;
static void onButton() { pressLatch = true; }

struct Button {
  bool down = false, held = false, wakeOnly = false;
  uint32_t since = 0;
};
static Button btn;

static void pollButton(bool screenOn, bool& woke) {
  bool pressed = digitalRead(PIN_BUTTON1) == LOW;
  uint32_t t = millis();
  if (pressLatch && !pressed && !btn.down) {  // нажали и отпустили между опросами
    pressLatch = false;
    game.wake();
    if (!screenOn) woke = true;
    else game.button(pet::B_SHORT);
    return;
  }
  pressLatch = false;
  if (pressed && !btn.down) {
    btn.down = true; btn.held = false; btn.since = t;
    btn.wakeOnly = !screenOn;
    if (!screenOn) woke = true;
    game.wake();
  } else if (pressed && btn.down) {
    if (!btn.held && !btn.wakeOnly && t - btn.since >= 2000) {
      btn.held = true;
      game.button(pet::B_HOLD);
    }
  } else if (!pressed && btn.down) {
    btn.down = false;
    uint32_t d = t - btn.since;
    if (btn.wakeOnly || btn.held || d < 30) return;  // дребезг
    game.button(d >= 600 ? pet::B_LONG : pet::B_SHORT);
  }
}

// --- светодиоды ---

static uint32_t lastLed = 0xFFFFFFFF;
static void leds(bool screenOn) {
  // Единую логику сна и ночника проверяем также в симуляторе.
  uint32_t c = screenOn ? game.ledColor() : game.ledColorScreenOff();
  if (c == lastLed) return;
  lastLed = c;
  for (int i = 0; i < NEOPIXEL_NUM; i++) pixels.setPixelColor(i, c);
  pixels.show();
}

static int brightness() {
  static const int lv[] = {0, 40, 120, 255};
  return lv[game.s.brightness > 3 ? 3 : game.s.brightness];
}

void setup() {
  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  attachInterrupt(PIN_BUTTON1, onButton, FALLING);
  pinMode(ADC_CTRL, OUTPUT); digitalWrite(ADC_CTRL, LOW);
  pinMode(GPS_POWER, OUTPUT); digitalWrite(GPS_POWER, LOW);
  pinMode(GPS_STANDBY, OUTPUT); digitalWrite(GPS_STANDBY, LOW);
  loraSleep();
  pixels.begin();
  pixels.clear();
  pixels.show();

  InternalFS.begin();
  game.hwId = NRF_FICR->DEVICEID[0] ^ (NRF_FICR->DEVICEID[1] * 2654435761u);
  uint32_t seed = NRF_FICR->DEVICEADDR[0] ^ micros();
  if (!loadGame()) {
    memset(&game.s, 0, sizeof(game.s));
    game.init(seed);
  }
  game.hwId = NRF_FICR->DEVICEID[0] ^ (NRF_FICR->DEVICEID[1] * 2654435761u);  // заводской номер: ник и id не меняются при смене питомца
  if (game.s.deviceId != game.hwId) { game.s.deviceId = game.hwId; game.dirty = true; }
  ble.begin(game);
  ble.enable(game.s.bleOn);
  readBattery();
  tft.begin(game.s.flip);
  flipState = game.s.flip;
  game.render(canvas);
  tft.push(fb);
  tft.backlight(brightness());
  brightState = game.s.brightness;
  lastTick = millis();
  // сторожевой таймер: если программа зависнет, плата перезапустится сама
  NRF_WDT->CONFIG = (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos) | (WDT_CONFIG_SLEEP_Pause << WDT_CONFIG_SLEEP_Pos);
  NRF_WDT->CRV = 90 * 32768;
  NRF_WDT->RREN = 1;
  NRF_WDT->TASKS_START = 1;
}

void loop() {
  NRF_WDT->RR[0] = WDT_RR_RR_Reload;
  uint32_t t = millis();
  uint32_t dt = t - lastTick;
  lastTick = t;
  bool woke = false;
  pollButton(tft.on, woke);
  if (woke && !tft.on) {
    tft.wake();
    game.render(canvas);
    tft.push(fb);
    tft.backlight(brightness());
  }
  game.tick(dt);

  ble.enable(game.s.bleOn);
  ble.poll();
  // встречи по LoRa: включили — поднимаем радио, выключили — в сон
  if (game.s.loraOn) {
    if (!lora.ready && !lora.begin()) { game.s.loraOn = 0; game.dirty = true; }
    lora.poll(game);
  } else if (lora.ready) {
    lora.off();
  }
  if (gpsOn) gpsPoll();
  else gpsIdle();
  if (game.wantsGps() != gpsOn) gpsPower(game.wantsGps());

  if (t - lastBattery > 30000) { readBattery(); lastBattery = t; }
  if (game.saveNow || (game.dirty && t - lastSave > SAVE_EVERY)) { saveGame(); game.saveNow = false; }
  if (game.s.flip != flipState) { flipState = game.s.flip; tft.setFlip(flipState); }
  if (game.s.brightness != brightState) { brightState = game.s.brightness; tft.backlight(brightness()); }

  // гасим экран, если долго не трогали и ничего не происходит
  bool canSleep = game.screenAllowedOff() && game.idleMs > SCREEN_TIMEOUT && !game.wantsGps() && !game.visiting();
  if (tft.on && canSleep) {
    tft.sleep();
    if (game.dirty) saveGame();
  }
  if (game.justEvolved && !tft.on) {  // эволюцию надо показать
    tft.wake();
    tft.backlight(brightness());
    game.wake();
  }
  game.justEvolved = false;

  leds(tft.on);

  if (tft.on) {
    game.render(canvas);
    tft.push(fb);
    uint32_t spent = millis() - t;
    if (spent < 33) delay(33 - spent);  // ~30 кадров/с
  } else {
    // спим между опросами кнопки; ночью, когда питомец спит, — реже (нажатие всё равно ловит прерывание)
    delay(gpsOn ? 20 : (game.s.asleep && !game.attention() ? 250 : 50));
  }
}
