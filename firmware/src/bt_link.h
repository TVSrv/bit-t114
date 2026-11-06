// Bluetooth: «UART по Bluetooth» (Nordic UART Service). Текстовые команды с телефона
// (приложение вроде Serial Bluetooth Terminal) и уведомления о событиях питомца.
#pragma once
#include <bluefruit.h>
#include "game.h"

class BleLink {
 public:
  void begin(pet::Game& g) {
    game = &g;
    char name[16];
    snprintf(name, sizeof name, "Bit-%08X", (unsigned)g.hwId);  // тот же код, что ник платы
    Bluefruit.autoConnLed(false);  // светодиод платы не трогаем
    // большие пакеты (MTU до 247) и длинная очередь: иначе ответ режется на куски по 20 байт
    // и при занятой очереди куски пропадали (у телефона доходило «Время уста…о»)
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
    Bluefruit.begin();
    Bluefruit.setTxPower(0);
    Bluefruit.setName(name);
    dfu.begin();   // обновление прошивки по воздуху (Nordic DFU, загрузчик Adafruit)
    uart.begin();
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(uart);
    Bluefruit.ScanResponse.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    // первые 30 с после включения или разрыва — часто (20 мс), чтобы телефон находил сразу; потом раз в секунду
    Bluefruit.Advertising.setInterval(32, 1600);
    Bluefruit.Advertising.setFastTimeout(30);
    started = true;
    lastSeq = g.eventSeq;
  }

  void enable(bool on) {
    if (!started) return;
    if (on && !advertising) { Bluefruit.Advertising.start(0); advertising = true; }
    if (!on && advertising) {
      Bluefruit.Advertising.restartOnDisconnect(false);
      Bluefruit.Advertising.stop();
      if (Bluefruit.connected()) Bluefruit.disconnect(Bluefruit.connHandle());
      Bluefruit.Advertising.restartOnDisconnect(true);
      advertising = false;
    }
  }

  void poll() {
    if (!started) return;
    bool conn = Bluefruit.connected();
    if (conn && !wasConnected) send("\xC2\xBB Бит на связи. Напиши «помощь».");
    wasConnected = conn;
    while (uart.available()) {
      int c = uart.read();
      if (c == '\n' || c == '\r') {
        if (len) {
          line[len] = 0;
          game->command(line, reply, sizeof reply);
          send(reply);
          len = 0;
        }
      } else if (len < (int)sizeof(line) - 1) {
        line[len++] = (char)c;
      }
    }
    if (game->eventSeq != lastSeq) {
      lastSeq = game->eventSeq;
      if (conn) {  // события помечаем «» », чтобы приложение отличало их от ответов на команды
        char ev[80];
        snprintf(ev, sizeof ev, "\xC2\xBB %s", game->eventText);
        send(ev);
      }
    }
  }

 private:
  BLEUart uart;
  BLEDfu dfu;
  pet::Game* game = nullptr;
  bool started = false, advertising = false, wasConnected = false;
  uint16_t lastSeq = 0;
  char line[600];
  char reply[640];
  int len = 0;

  // по кускам размером с пакет; если очередь занята — подождать и повторить, ничего не теряя
  void send(const char* s) {
    if (!Bluefruit.connected()) return;
    uint16_t conn = Bluefruit.connHandle();
    BLEConnection* c = Bluefruit.Connection(conn);
    size_t chunk = c ? (size_t)(c->getMtu() - 3) : 20;
    if (chunk < 20) chunk = 20;
    size_t n = strlen(s);
    static char buf[700];
    if (n > sizeof(buf) - 2) n = sizeof(buf) - 2;
    memcpy(buf, s, n);
    buf[n++] = '\n';
    for (size_t i = 0; i < n;) {
      size_t k = n - i < chunk ? n - i : chunk;
      uint32_t t0 = millis();
      while (uart.write(conn, (const uint8_t*)buf + i, k) != k) {
        if (!Bluefruit.connected() || millis() - t0 > 500) return;
        delay(5);
      }
      i += k;
    }
  }
};
