// Bluetooth: «UART по Bluetooth» (Nordic UART Service). Текстовые команды с телефона
// (приложение вроде Serial Bluetooth Terminal) и уведомления о событиях питомца.
#pragma once
#include <bluefruit.h>
#include "game.h"
#include "mirror_tiles.h"

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

  bool mirrorEnabled() const {
    return started && Bluefruit.connected() && mirrorActive && (int32_t)(mirrorUntil - millis()) > 0;
  }

  // Only called when the Android pet window is open. Tile fingerprints avoid
  // retransmitting the static room and keep radio/CPU demand modest.
  void streamPixels(const gfx::Color* pixels) {
    if (!mirrorEnabled()) return;
    int attempts = 0, sent = 0;
    while (attempts++ < 45 && sent < 4) {
      int id = nextTile;
      nextTile = (uint16_t)((nextTile + 1) % mirror_tiles::COUNT);
      uint32_t hash = mirror_tiles::fingerprint(pixels, id);
      if (tileSeen[id] && hash == tileHash[id]) continue;
      char packet[mirror_tiles::MAX_LINE];
      if (mirror_tiles::encode(pixels, id, packet, sizeof packet) < 0) break;
      if (!send(packet)) break; // retry the same tile after BLE congestion
      tileSeen[id] = true;
      tileHash[id] = hash;
      ++sent;
    }
  }

  void poll() {
    if (!started) return;
    bool conn = Bluefruit.connected();
    if (conn && !wasConnected) {
      // Messages from before connecting are history, not new notifications.
      lastSeq = game->eventSeq;
      send("\xC2\xBB Бит на связи. Напиши «помощь».");
    }
    if (!conn && wasConnected) {
      mirrorActive = false;
      mirrorUntil = 0;
      memset(tileSeen, 0, sizeof tileSeen);
    }
    wasConnected = conn;
    while (uart.available()) {
      int c = uart.read();
      if (c == '\n' || c == '\r') {
        if (len) {
          line[len] = 0;
          if (!strcmp(line, "mirror 1")) {
            if (!mirrorEnabled()) {
              memset(tileSeen, 0, sizeof tileSeen);
              nextTile = 0;
            }
            mirrorActive = true;
            mirrorUntil = millis() + 12000u;
            send("M|1");
          } else if (!strcmp(line, "mirror 0")) {
            mirrorActive = false;
            mirrorUntil = 0;
            send("M|0");
          } else {
            game->command(line, reply, sizeof reply);
            send(reply);
          }
          len = 0;
        }
      } else if (len < (int)sizeof(line) - 1) {
        line[len++] = (char)c;
      }
    }
    if (!conn) {
      lastSeq = game->eventSeq;
    } else {
      // A minute tick can trigger multiple events: a bug, sickness and a
      // care call. Previously only the last event was sent, silently losing
      // the other Android notifications. Replay all still-buffered alerts.
      uint16_t pending = (uint16_t)(game->eventSeq - lastSeq);
      if (pending > pet::Game::EVENT_HISTORY)
        lastSeq = (uint16_t)(game->eventSeq - pet::Game::EVENT_HISTORY);
      while (lastSeq != game->eventSeq) {
        const uint16_t next = (uint16_t)(lastSeq + 1);
        const auto& record = game->events[next % pet::Game::EVENT_HISTORY];
        if (record.seq != next) { lastSeq = next; continue; }
        char ev[88];
        snprintf(ev, sizeof ev, "\xC2\xBB %s", record.text);
        if (!send(ev)) break;  // preserve a pending alert for retry
        lastSeq = next;
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
  bool mirrorActive = false;
  uint32_t mirrorUntil = 0;
  uint16_t nextTile = 0;
  uint32_t tileHash[mirror_tiles::COUNT] = {};
  bool tileSeen[mirror_tiles::COUNT] = {};

  // Reliable NUS notifications; when blocked the tile must be retried.
  bool send(const char* s) {
    if (!Bluefruit.connected()) return false;
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
        if (!Bluefruit.connected() || millis() - t0 > 300) return false;
        delay(5);
      }
      i += k;
    }
    return true;
  }
};
