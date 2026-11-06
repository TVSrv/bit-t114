// Ник платы: 8 знаков из заводского номера чипа (около 4 млрд вариантов), не зависит от питомца.
// Тот же код — в имени Bluetooth (Bit-XXXXXXXX).
#include "game.h"
#include "util.h"

namespace pet {

void nickName(uint32_t id, char* out, int n) { sfmt(out, n, "%08X", (unsigned)id); }

}  // namespace pet
