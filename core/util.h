// Мелочи без стандартной библиотеки: форматирование строк.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace pet {
// поддерживает %d, %u, %X, %s, %c, %%, ширину с нулями (%02d, %04X)
int sfmt(char* out, int n, const char* f, ...);
// base64 — для резервной копии питомца через Bluetooth
int b64encode(const uint8_t* in, int n, char* out, int outSize);
int b64decode(const char* in, uint8_t* out, int outSize);  // -1 при ошибке
}
