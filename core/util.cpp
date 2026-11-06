#include "util.h"

namespace pet {

int sfmt(char* out, int n, const char* f, ...) {
  __builtin_va_list ap;
  __builtin_va_start(ap, f);
  int o = 0;
  auto put = [&](char c) { if (o < n - 1) out[o++] = c; };
  for (; *f; f++) {
    if (*f != '%') { put(*f); continue; }
    f++;
    bool zero = false;
    int width = 0;
    if (*f == '0') { zero = true; f++; }
    while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
    if (*f == 'd' || *f == 'u') {
      long v = *f == 'd' ? (long)__builtin_va_arg(ap, int) : (long)__builtin_va_arg(ap, unsigned);
      char tmp[12];
      int k = 0;
      bool neg = v < 0;
      unsigned long u = neg ? -v : v;
      do { tmp[k++] = (char)('0' + u % 10); u /= 10; } while (u);
      if (neg) put('-');
      for (int i = k; i < width; i++) put(zero ? '0' : ' ');
      while (k) put(tmp[--k]);
    } else if (*f == 'X') {
      unsigned v = __builtin_va_arg(ap, unsigned);
      char tmp[9];
      int k = 0;
      do { tmp[k++] = "0123456789ABCDEF"[v & 15]; v >>= 4; } while (v);
      for (int i = k; i < width; i++) put(zero ? '0' : ' ');
      while (k) put(tmp[--k]);
    } else if (*f == 's') {
      const char* s = __builtin_va_arg(ap, const char*);
      while (s && *s) put(*s++);
    } else if (*f == 'c') {
      put((char)__builtin_va_arg(ap, int));
    } else if (*f == '%') {
      put('%');
    } else if (!*f) {
      break;
    }
  }
  out[o] = 0;
  __builtin_va_end(ap);
  return o;
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int b64encode(const uint8_t* in, int n, char* out, int outSize) {
  int o = 0;
  for (int i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)in[i] << 16 | (i + 1 < n ? (uint32_t)in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
    if (o + 4 >= outSize) return -1;
    out[o++] = B64[(v >> 18) & 63];
    out[o++] = B64[(v >> 12) & 63];
    out[o++] = i + 1 < n ? B64[(v >> 6) & 63] : '=';
    out[o++] = i + 2 < n ? B64[v & 63] : '=';
  }
  out[o] = 0;
  return o;
}

int b64decode(const char* in, uint8_t* out, int outSize) {
  uint32_t v = 0;
  int bits = 0, o = 0;
  for (; *in; in++) {
    char c = *in;
    int d;
    if (c >= 'A' && c <= 'Z') d = c - 'A';
    else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
    else if (c >= '0' && c <= '9') d = c - '0' + 52;
    else if (c == '+') d = 62;
    else if (c == '/') d = 63;
    else if (c == '=' || c == ' ' || c == '\r' || c == '\n') continue;
    else return -1;
    v = v << 6 | (uint32_t)d;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (o >= outSize) return -1;
      out[o++] = (uint8_t)(v >> bits);
    }
  }
  return o;
}

}  // namespace pet
