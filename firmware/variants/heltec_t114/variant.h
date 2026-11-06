// Вариант платы Heltec Mesh Node T114 для ядра Adafruit nRF52.
// Номера выводов: Pn.m = n * 32 + m (прямая нумерация, как в варианте Meshtastic).
#ifndef _VARIANT_HELTEC_T114_
#define _VARIANT_HELTEC_T114_

#define VARIANT_MCK (64000000ul)
#define USE_LFXO

#include "WVariant.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PINS_COUNT (48)
#define NUM_DIGITAL_PINS (48)
#define NUM_ANALOG_INPUTS (1)
#define NUM_ANALOG_OUTPUTS (0)

// светодиод (зелёный, горит при LOW) и кнопка
#define PIN_LED1 (32 + 3)
#define LED_BUILTIN PIN_LED1
#define LED_BLUE PIN_LED1
#define LED_RED PIN_LED1
#define LED_STATE_ON 0
#define PIN_BUTTON1 (32 + 10)

// два RGB-светодиода WS2812
#define PIN_NEOPIXEL (0 + 14)
#define NEOPIXEL_NUM 2

// экран ST7789 240x135 — на основной SPI (SPIM3, до 32 МГц)
#define TFT_CS (0 + 11)
#define TFT_DC (0 + 12)
#define TFT_RST (0 + 2)
#define TFT_PWR (0 + 3)   // питание панели, LOW — включено
#define TFT_BL (0 + 15)   // подсветка, LOW — включена
#define SPI_INTERFACES_COUNT 1
#define PIN_SPI_MOSI (32 + 9)
#define PIN_SPI_SCK (32 + 8)
#define PIN_SPI_MISO (0)  // экрану не нужен: вывод 0 в карте — «не подключён», P0.23 остаётся радиомодулю

// GPS L76K на Serial1
#define PIN_SERIAL1_RX (32 + 5)
#define PIN_SERIAL1_TX (32 + 7)
#define GPS_POWER (0 + 21)     // VEXT, HIGH — включено
#define GPS_STANDBY (32 + 2)   // HIGH — не спать
#define GPS_PPS (32 + 4)

// радиомодуль SX1262: встречи питомцев; выключен — усыпляется
#define LORA_CS (0 + 24)
#define LORA_SCK (0 + 19)
#define LORA_MOSI (0 + 22)
#define LORA_MISO (0 + 23)
#define LORA_BUSY (0 + 17)
#define LORA_DIO1 (0 + 20)
#define LORA_RESET (0 + 25)

// батарея
#define BATTERY_PIN (0 + 4)
#define ADC_CTRL (0 + 6)       // HIGH — делитель подключён
#define ADC_MULTIPLIER (4.916F)

// I2C (на разъёмах)
#define WIRE_INTERFACES_COUNT 1
#define PIN_WIRE_SDA (0 + 26)
#define PIN_WIRE_SCL (0 + 27)

#define PIN_A0 BATTERY_PIN
static const uint8_t A0 = PIN_A0;
#define ADC_RESOLUTION 14

#define PIN_SERIAL2_RX (0 + 9)
#define PIN_SERIAL2_TX (0 + 10)

#ifdef __cplusplus
}
#endif

#endif
