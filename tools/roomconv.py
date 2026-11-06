#!/usr/bin/env python3
"""Комнаты по формам питомца: шесть времён суток каждой комнаты в одном сжатом виде.

Вход: bit-art/rooms/<NN-имя>/room-<имя>-<время>.png (240x87) и мастерская Джуна
build/workshop/room-workshop-<время>.png (её готовит tools/artconv.py — запускать первым).
Выход: core/rooms.cpp и core/rooms.h.

Каждый пиксель комнаты — номер одного из 256 «цветов на все времена»: у номера шесть цветов,
по одному на ночь, рассвет, утро, день, закат и вечер. Так одна карта пикселей служит всем
шести картинкам. Окно (у улицы — небо) хранится отдельно на каждое время: там луна, солнце
и облака разные. Карты сжаты LZ4. Кровать, одеяло, мониторы и окно находятся по картинкам.

Запуск из корня репозитория: python3 tools/roomconv.py [папка-для-превью]
"""
import glob
import os
import sys

import numpy as np
from PIL import Image

T = ['night', 'dawn', 'morning', 'day', 'sunset', 'evening']  # порядок как LIGHT в scenes.cpp
W, H = 240, 87
PREVIEW = sys.argv[1] if len(sys.argv) > 1 else None
# Комната каждой формы (порядок enum Form). Джун живёт в исходной мастерской арт-пака:
# на ней точно стоят его сцены сна, кофе, пиццы и работы за компьютером.
ONLY = os.environ.get('ROOM')
ROOMS = [('bit-art/rooms/01-compilation', 'compilation'), ('bit-art/rooms/02-baby', 'baby'),
         ('build/workshop', 'workshop'), ('bit-art/rooms/04-middle', 'middle'),
         ('bit-art/rooms/05-freelancer', 'freelancer'), ('bit-art/rooms/06-senior', 'senior'),
         ('bit-art/rooms/07-architect', 'architect'), ('bit-art/rooms/08-hacker', 'hacker'),
         ('bit-art/rooms/09-legacy', 'legacy'), ('bit-art/rooms/10-ai', 'ai'), ('bit-art/rooms/11-ghost', 'ghost')]
OUTDOOR = {'ghost'}  # улица: погода на всём небе, кровати и стола нет
# где поиск экранов не справился (чёрные мониторы на тёмном фоне) — рамки заданы вручную
SCREENS = {'senior': [(24, 34, 40, 51), (43, 34, 60, 51)]}
DESK = {'workshop': 92}  # Джун садится за стол там же, где на вечерней картинке


def rgb565(c):
    r, g, b = (int(round(v)) for v in np.clip(c, 0, 255))
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def comps(m):
    lab = np.zeros(m.shape, int); out = []; h, w = m.shape; n = 0
    for y in range(h):
        for x in range(w):
            if m[y, x] and not lab[y, x]:
                n += 1; st = [(y, x)]; lab[y, x] = n; pts = []
                while st:
                    yy, xx = st.pop(); pts.append((yy, xx))
                    for a, b in ((yy + 1, xx), (yy - 1, xx), (yy, xx + 1), (yy, xx - 1)):
                        if 0 <= a < h and 0 <= b < w and m[a, b] and not lab[a, b]:
                            lab[a, b] = n; st.append((a, b))
                out.append(np.array(pts))
    return out


def lz4(data):
    """Блок LZ4 (жадный поиск по хэшу четырёх байт)."""
    data = bytes(data); n = len(data); out = bytearray(); i = anchor = 0; table = {}
    def emit(lit, mlen, off):
        ll = len(lit); tok = (min(ll, 15) << 4) | (min(mlen - 4, 15) if mlen else 0)
        out.append(tok)
        if ll >= 15:
            r = ll - 15
            while r >= 255: out.append(255); r -= 255
            out.append(r)
        out.extend(lit)
        if mlen:
            out.extend((off & 255, off >> 8))
            if mlen - 4 >= 15:
                r = mlen - 4 - 15
                while r >= 255: out.append(255); r -= 255
                out.append(r)
    while i + 4 <= n - 5:
        key = data[i:i + 4]; j = table.get(key); table[key] = i
        if j is not None and i - j < 65536:
            m = 4
            while i + m < n - 5 and data[j + m] == data[i + m]: m += 1
            emit(data[anchor:i], m, i - j)
            for k in range(i + 1, min(i + m, n - 4)): table[data[k:k + 4]] = k
            i += m; anchor = i
        else:
            i += 1
    emit(data[anchor:], 0, 0)
    return bytes(out)


def unlz4(src):
    out = bytearray(); i = 0
    while i < len(src):
        t = src[i]; i += 1; l = t >> 4
        if l == 15:
            while True:
                b = src[i]; i += 1; l += b
                if b != 255: break
        out.extend(src[i:i + l]); i += l
        if i >= len(src): break
        off = src[i] | (src[i + 1] << 8); i += 2; m = (t & 15) + 4
        if (t & 15) == 15:
            while True:
                b = src[i]; i += 1; m += b
                if b != 255: break
        for _ in range(m): out.append(out[-off])
    return bytes(out)


def kmeans(X, k, iters=30, seed=7):
    """k-средних с начальными центрами k-means++ по выборке."""
    rng = np.random.default_rng(seed)
    S = X[rng.choice(len(X), min(len(X), 4000), replace=False)]
    C = [S[rng.integers(len(S))]]
    d2 = ((S - C[0]) ** 2).sum(1)
    for _ in range(1, k):
        p = d2 / d2.sum(); C.append(S[rng.choice(len(S), p=p)])
        d2 = np.minimum(d2, ((S - C[-1]) ** 2).sum(1))
    C = np.array(C, np.float32); X = X.astype(np.float32); xx = (X ** 2).sum(1)[:, None]
    for _ in range(iters):
        lab = (xx - 2 * X @ C.T + (C ** 2).sum(1)[None]).argmin(1)
        for j in range(k):
            m = lab == j
            C[j] = X[m].mean(0) if m.any() else X[rng.integers(len(X))]
    lab = (xx - 2 * X @ C.T + (C ** 2).sum(1)[None]).argmin(1)
    return C, lab


def quant(pixels, n):
    im = Image.fromarray(pixels.reshape(1, -1, 3).astype(np.uint8))
    q = im.quantize(colors=n, method=Image.Quantize.MEDIANCUT, kmeans=3)
    return np.array(q.getpalette()[:n * 3], float).reshape(-1, 3), np.asarray(q).ravel()


def analyse(name, A):
    """Окно, одеяло, место сна, мониторы и место у стола — по дневной картинке."""
    D = A[3]; r, g, b = D[..., 0], D[..., 1], D[..., 2]
    meta = {}
    if name in OUTDOOR:
        sky = (b > 170) & (b > r + 40)
        y = 0
        while y < H and sky[y].mean() > 0.3: y += 1
        meta['win'] = (0, 0, W, y + 1)
    else:
        m = (b > 200) & (r < 190) & (g > 150); m[50:] = False
        cs = [c for c in comps(m) if c[:, 1].mean() > 110]
        c = max(cs, key=len)
        meta['win'] = (int(c[:, 1].min()), int(c[:, 0].min()), int(c[:, 1].max()) + 1, int(c[:, 0].max()) + 1)
    blanket = np.full(W, 255, int)
    if name not in OUTDOOR:
        bm = (b > r + 70) & (b > 150); bm[:40] = False; bm[80:] = False; bm[:, :120] = False
        big = max(comps(bm), key=len)
        keep = np.zeros_like(bm); keep[big[:, 0], big[:, 1]] = True
        for x in range(W):
            ys = np.nonzero(keep[:, x])[0]
            if len(ys): blanket[x] = ys.min()
    xs = np.nonzero(blanket < 255)[0]
    meta['blanket'] = (int(xs.min()), blanket[xs.min():xs.max() + 1].tolist()) if len(xs) else (0, [])
    meta['bed'] = int((xs.min() + xs.max()) / 2) - 4 if len(xs) else 120
    # мониторы — тёмные ровные прямоугольники в левой половине (экран пустой)
    screens = []
    if name not in OUTDOOR:
        lum = D.mean(2)
        for thr, pt in ((30, 30), (48, 40), (60, 60)):  # экраны бывают чёрные и тёмно-синие
            dm = (lum < thr) & (np.ptp(D, 2) < pt); dm[:8] = False; dm[66:] = False; dm[:, 160:] = False
            for c in comps(dm):
                x0, y0, x1, y1 = c[:, 1].min(), c[:, 0].min(), c[:, 1].max() + 1, c[:, 0].max() + 1
                w_, h_ = x1 - x0, y1 - y0
                if 9 <= w_ < 70 and 6 <= h_ < 30 and y0 >= 10 and len(c) >= 0.85 * w_ * h_:
                    r_ = (int(x0), int(y0), int(x1), int(y1))
                    if not any(a < r_[2] and r_[0] < c_ and b < r_[3] and r_[1] < d for a, b, c_, d in screens):
                        screens.append(r_)
        screens = sorted(sorted(screens, key=lambda s: -(s[2] - s[0]) * (s[3] - s[1]))[:3])
    meta['screens'] = SCREENS.get(name, screens)
    screens = meta['screens']
    # ёлка на Новый год: самый левый угол пола, где она не закрывает мониторы и кровать
    tree = 14
    bx0, bl = meta['blanket']
    def hits(x):
        for r in range(79 - 40, 80):  # ёлка узкая вверху и широкая внизу, подарки по бокам у пола
            hw = 15 if r >= 74 else 3 + (r - 39) * 0.28
            for a, b, c_, d in screens:
                if b <= r < d and a < x + hw and x - hw < c_: return True
        return bool(bl) and bx0 - 8 < x + 15 and x - 15 < bx0 + len(bl) + 4
    for x in range(14, 200, 2):
        if not hits(x): tree = x; break
    meta['tree'] = tree
    meta['desk'] = DESK.get(name, min(screens[-1][2] + 22, 150) if screens else 0)
    return meta


def main():
    rooms = []
    for path, name in ROOMS:
        if ONLY and name != ONLY: continue
        A = np.stack([np.asarray(Image.open(f'{path}/room-{name}-{t}.png').convert('RGB'), float) for t in T], 0)
        assert A.shape[1:3] == (H, W), (name, A.shape)
        meta = analyse(name, A)
        x0, y0, x1, y1 = meta['win']
        X = A.transpose(1, 2, 0, 3).reshape(H * W, 18)
        fitMask = np.ones((H, W), bool); fitMask[y0:y1, x0:x1] = False
        C, _ = kmeans(X[fitMask.ravel()], 256)
        Xf = X.astype(np.float32)
        lab = ((Xf ** 2).sum(1)[:, None] - 2 * Xf @ C.T + (C ** 2).sum(1)[None]).argmin(1).astype(np.uint8)
        pals = C.reshape(256, 6, 3).transpose(1, 0, 2)  # [время][номер]
        px = lz4(lab.tobytes()); assert unlz4(px) == lab.tobytes()
        wins = []
        for t in range(6):
            crop = A[t, y0:y1, x0:x1]
            wp, wi = quant(crop.reshape(-1, 3), 24 if name in OUTDOOR else 64)
            wz = lz4(wi.astype(np.uint8).tobytes()); assert unlz4(wz) == wi.astype(np.uint8).tobytes()
            wins.append((wp, wz, wi))
        err = np.abs(pals.transpose(1, 0, 2)[lab].reshape(H, W, 6, 3).transpose(2, 0, 1, 3) - A)[:, fitMask].mean()
        print(f'{name:12s} карта {len(px):5d} Б, окна {sum(len(w[1]) for w in wins):5d} Б, ошибка {err:.2f}, '
              f'окно {meta["win"]}, кровать {meta["bed"]}, мониторы {meta["screens"]}, стол {meta["desk"]}, ёлка {meta["tree"]}')
        rooms.append((name, meta, pals, px, wins))
        if PREVIEW:
            os.makedirs(PREVIEW, exist_ok=True)
            out = []
            for t in range(6):
                img = pals[t][lab].reshape(H, W, 3)
                wp, _, wi = wins[t]
                img[y0:y1, x0:x1] = wp[wi].reshape(y1 - y0, x1 - x0, 3)
                for (a, b_, c, d) in meta['screens']: img[b_, a:c] = (255, 0, 255)
                bx0, bl = meta['blanket']
                for k, v in enumerate(bl):
                    if v < 255: img[v, bx0 + k] = (255, 255, 0)
                out.append(np.concatenate([A[t], img], 1))
            Image.fromarray(np.concatenate(out, 0).astype(np.uint8)).resize((960, H * 12), Image.NEAREST).save(f'{PREVIEW}/room-{name}.png')

    def arr(name, data, typ='uint8_t', per=32):
        body = '\n'.join('  ' + ','.join(str(int(v)) for v in data[k:k + per]) + ',' for k in range(0, len(data), per))
        return f'static const {typ} {name}[] = {{\n{body}\n}};\n'

    out = ['// Создано tools/roomconv.py из bit-art/rooms и мастерской арт-пака — не править руками.',
           '#include "rooms.h"', '', 'namespace rooms {', '']
    for name, meta, pals, px, wins in rooms:
        out.append(arr(f'{name}_px', px))
        out.append(arr(f'{name}_pal', [rgb565(c) for t in range(6) for c in pals[t]], 'uint16_t', 16))
        for t in range(6):
            wp, wz, _ = wins[t]
            out.append(arr(f'{name}_w{t}', wz))
            out.append(arr(f'{name}_wp{t}', [rgb565(c) for c in wp], 'uint16_t', 16))
        bx0, bl = meta['blanket']
        out.append(arr(f'{name}_bl', bl or [255]))
    out.append('const Room ROOM[11] = {')
    for name, meta, pals, px, wins in rooms:
        x0, y0, x1, y1 = meta['win']; bx0, bl = meta['blanket']
        sc = meta['screens'] + [(0, 0, 0, 0)] * (3 - len(meta['screens']))
        out.append(f'  {{{name}_px, {len(px)}, {name}_pal, {{{x0}, {y0}, {x1}, {y1}}},\n'
                   f'   {{{", ".join(f"{name}_w{t}" for t in range(6))}}},\n'
                   f'   {{{", ".join(str(len(wins[t][1])) for t in range(6))}}},\n'
                   f'   {{{", ".join(f"{name}_wp{t}" for t in range(6))}}},\n'
                   f'   {bx0}, {len(bl)}, {name}_bl, {meta["bed"]}, {len(meta["screens"])}, '
                   f'{{{", ".join("{" + ", ".join(map(str, s)) + "}" for s in sc)}}}, {meta["desk"]}, '
                   f'{"true" if name in OUTDOOR else "false"}, {meta["tree"]}}},')
    out.append('};')
    out.append(r'''
int unpack(const uint8_t* src, int n, uint8_t* dst, int cap) {
  int i = 0, o = 0;
  while (i < n) {
    uint8_t t = src[i++];
    int l = t >> 4;
    if (l == 15) { uint8_t b; do { b = src[i++]; l += b; } while (b == 255); }
    if (o + l > cap) return -1;
    for (int k = 0; k < l; k++) dst[o++] = src[i++];
    if (i >= n) break;
    int off = src[i] | (src[i + 1] << 8);
    i += 2;
    int m = (t & 15) + 4;
    if ((t & 15) == 15) { uint8_t b; do { b = src[i++]; m += b; } while (b == 255); }
    if (off <= 0 || off > o || o + m > cap) return -1;
    for (int k = 0; k < m; k++, o++) dst[o] = dst[o - off];
  }
  return o;
}

}  // namespace rooms
''')
    open('core/rooms.cpp', 'w').write('\n'.join(out))
    open('core/rooms.h', 'w').write('''// Комнаты по формам питомца (данные — rooms.cpp, создаёт tools/roomconv.py).
#pragma once
#include <stdint.h>

namespace rooms {

// Сцена 240x87. px — индексы (LZ4), pal — 6 палитр по 256 цветов: ночь, рассвет, утро, день, закат, вечер.
// win — стекло окна (у улицы — небо): своя картинка на каждое время, 64 цвета, LZ4.
// blanket — верх одеяла по столбцам с blanketX0 (255 — нет), bed — где спит, screen — мониторы,
// desk — где стоит у стола, outdoor — улица (кровати и стола нет).
struct Room {
  const uint8_t* px;
  uint16_t pxLen;
  const uint16_t* pal;
  uint8_t win[4];
  const uint8_t* winPx[6];
  uint16_t winLen[6];
  const uint16_t* winPal[6];
  uint8_t blanketX0, blanketN;
  const uint8_t* blanket;
  uint8_t bed;
  uint8_t screens;
  uint8_t screen[3][4];
  uint8_t desk;
  bool outdoor;
  uint8_t tree;  // где стоит ёлка на Новый год (и торт, тыква в другие праздники)
};
extern const Room ROOM[11];

// Распаковка LZ4: число байт или -1, если данные битые.
int unpack(const uint8_t* src, int n, uint8_t* dst, int cap);

}  // namespace rooms
''')
    tot = sum(len(r[3]) + 6 * 512 + sum(len(w[1]) + 128 for w in r[4]) for r in rooms)
    print('итого данных', tot, 'Б')


main()
