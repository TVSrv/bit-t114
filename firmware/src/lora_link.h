// LoRa: встречи питомцев. SX1262 на своей шине SPIM2; 869,1 МГц (диапазон 868,7–869,2 МГц),
// 14 дБм, SF9/125 кГц. Когда слать и когда слушать, решает игра (core/meet.cpp).
#pragma once
#include <SPI.h>
#include <RadioLib.h>
#include "game.h"

class LoraLink {
 public:
  bool ready = false;

  bool begin() {
    if (ready) return true;
    spi.begin();
    radio = new SX1262(new Module(LORA_CS, LORA_DIO1, LORA_RESET, LORA_BUSY, spi));
    int st = radio->begin(869.1, 125.0, 9, 7, 0x12, 14, 16, 1.8, false);  // TCXO 1,8 В — как у Meshtastic для T114
    if (st != RADIOLIB_ERR_NONE) return false;
    radio->setDio2AsRfSwitch(true);
    radio->setPacketReceivedAction(onIrq);
    radio->sleep();
    ready = true;
    rxOn = false;
    return true;
  }

  void off() {
    if (!ready) return;
    radio->sleep();
    rxOn = false;
  }

  void poll(pet::Game& game) {
    if (!ready) return;
    if (irq) {
      irq = false;
      if (rxOn) {
        uint8_t buf[64];
        size_t len = radio->getPacketLength();
        if (len > 0 && len <= sizeof buf && radio->readData(buf, len) == RADIOLIB_ERR_NONE)
          game.loraRx(buf, (int)len, (int)radio->getRSSI());
        radio->startReceive();
      }
    }
    uint8_t tx[32];
    int n = 0;
    if (game.loraWantTx(tx, n)) {
      radio->transmit(tx, n);  // ~0,2 с
      irq = false;              // прерывание «передача окончена» — не приём
      rxOn = false;
    }
    bool want = game.loraListen();
    if (want && !rxOn) { radio->startReceive(); rxOn = true; }
    else if (!want && rxOn) { radio->sleep(); rxOn = false; }
  }

 private:
  SPIClass spi{NRF_SPIM2, LORA_MISO, LORA_SCK, LORA_MOSI};
  SX1262* radio = nullptr;
  bool rxOn = false;
  static inline volatile bool irq = false;
  static void onIrq() { irq = true; }
};
