#!/usr/bin/env python3
"""Готовит картинки арт-пака для прошивки: комнату на шесть времён суток, крышу для прогулки
и одиннадцать персонажей. Пишет core/artdata.cpp (8-битные индексы + палитры RGB565).

Запуск из корня репозитория: python3 tools/artconv.py [папка-для-превью]
Нужны Pillow и numpy.
"""
import json
import math
import sys
from collections import deque

import numpy as np
from PIL import Image

ART = 'bit-art/'
PREVIEW = sys.argv[1] if len(sys.argv) > 1 else None
W, H = 240, 87  # сцена между верхней и нижней панелью

def load(path):
    return np.asarray(Image.open(ART + path).convert('RGB'), float)

def save(name, a):
    if PREVIEW:
        Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(f'{PREVIEW}/{name}.png')

def rgb565(c):
    r, g, b = (int(round(v)) for v in c)
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def blur(a, s):
    r = int(s * 3)
    k = np.exp(-np.arange(-r, r + 1) ** 2 / (2 * s * s))
    k /= k.sum()
    f = lambda v: np.convolve(np.pad(v, r, mode='edge'), k, 'valid')
    return np.apply_along_axis(f, 1, np.apply_along_axis(f, 0, a))

# ---------- мастерская ----------

sheet = load('environments/workshop-time-of-day.png')
BOX = {'dawn': (40, 30), 'morning': (789, 30), 'day': (40, 356),
       'sunset': (789, 356), 'evening': (40, 684), 'night': (789, 684)}
P = {n: sheet[y:y + 276, x:x + 706] for n, (x, y) in BOX.items()}
PH, PW = 276, 706

# Дневная комната без питомца: дверь берём из вечерней панели, левую половину
# (там стул и питомец) отражаем от оси двери x=364 и подгоняем цвет под день.
D, E = P['day'], P['evening']
Es = np.zeros_like(E); Es[2:] = E[:-2]; Es[:2] = E[:2]
M = Es.copy()
for x in range(23, 364):
    M[:, x] = Es[:, 728 - x]
x0, x1, y0, y1 = 284, 445, 78, 262
clean = D.copy()
fit = (slice(22, 76), slice(x0, x1))
for ch in range(3):
    a = M[fit][..., ch].ravel(); b = D[fit][..., ch].ravel()
    g, o = np.linalg.lstsq(np.vstack([a, np.ones_like(a)]).T, b, rcond=None)[0]
    clean[y0:y1, x0:x1, ch] = np.clip(M[y0:y1, x0:x1, ch] * g + o, 0, 255)
for y in range(240, y1):  # пол перед дверью — плавно между соседними столбцами
    l, r = D[y, x0 - 1], D[y, x1]
    for x in range(x0, x1):
        t = (x - x0 + 1) / (x1 - x0 + 1)
        clean[y, x] = l * (1 - t) + r * t
# Экран терминала пустой: текст рисуется в цвете выбранной темы.
TXT = (86, 118, 140, 152)
scr = clean[112:165, 83:153].reshape(-1, 3)
lum = scr.mean(1)
clean[TXT[1]:TXT[3], TXT[0]:TXT[2]] = np.median(scr[lum < np.percentile(lum, 60)], 0)

# Свет каждой панели переносим на чистую комнату отношением размытых яркостей.
# Места, где на панели стоит питомец или предметы, заполняются гармонически.
MASKS = {'dawn': [(500, 118, 650, 215), (390, 190, 480, 258)],
         'night': [(500, 118, 650, 215), (390, 185, 485, 258)],
         'morning': [(228, 95, 400, 258)], 'sunset': [(265, 95, 475, 266)],
         'evening': [(195, 100, 340, 258)], 'day': []}
GLASS = (543, 39, 630, 84)

def edges(a):
    g = a.mean(2)
    return np.abs(np.diff(g, axis=0))[:, :-1] + np.abs(np.diff(g, axis=1))[:-1]

def shift(p, reg):
    rx0, ry0, rx1, ry1 = reg
    eD = edges(clean)[ry0:ry1, rx0:rx1]; eP = edges(p); best = None
    for dy in range(-7, 8):
        for dx in range(-7, 8):
            s = (eD * eP[ry0 + dy:ry1 + dy, rx0 + dx:rx1 + dx]).sum()
            if best is None or s > best[0]: best = (s, dx, dy)
    return best[1:]

def move(p, dx, dy):
    return np.roll(np.roll(p, -dy, 0), -dx, 1)

def fill(R, hole):
    if hole.sum() == 0: return R
    h, w = hole.shape
    if min(h, w) > 16:
        sf = fill(R[::2, ::2].copy(), hole[::2, ::2])
        up = np.repeat(np.repeat(sf, 2, 0), 2, 1)[:h, :w]
        R = R.copy(); R[hole] = up[hole]
    for _ in range(60):
        Q = np.pad(R, ((1, 1), (1, 1), (0, 0)), mode='edge')
        avg = (Q[:-2, 1:-1] + Q[2:, 1:-1] + Q[1:-1, :-2] + Q[1:-1, 2:]) / 4
        R[hole] = avg[hole]
    return R

ORDER = ['night', 'dawn', 'morning', 'day', 'sunset', 'evening']  # как LIGHT в scenes.cpp
rooms = {}
SHIFT = {}
for n in ORDER:
    p = P[n]
    dx, dy = shift(p, (20, 20, 690, 250)); wdx, wdy = shift(p, (520, 15, 660, 110))
    SHIFT[n] = (dx, dy)
    if n == 'day':
        O = clean.copy()
    else:
        pa = move(p, dx, dy)
        w = np.ones((PH, PW))
        for (a, b, c, d) in MASKS[n]: w[b:d, a:c] = 0
        w[GLASS[1] - 3:GLASS[3] + 3, GLASS[0] - 3:GLASS[2] + 3] = 0
        w[TXT[1] - 2:TXT[3] + 2, TXT[0] - 2:TXT[2] + 2] = 0
        wb = blur(w, 2.5) + 1e-6
        R = np.zeros_like(clean)
        for ch in range(3):
            num = blur(pa[..., ch] * w, 2.5) / wb; den = blur(clean[..., ch] * w, 2.5) / wb
            R[..., ch] = np.log((num + 3) / (den + 3))
        R = np.exp(fill(R, wb < 0.6))
        O = np.clip(clean * R, 0, 255)
        for (a, b, c, d) in MASKS[n]:
            if d > 238 and n in ('sunset', 'morning', 'evening'):
                for y in range(max(b, 238), min(d + 4, PH)):
                    l, r = O[y, a - 1].copy(), O[y, c].copy()
                    for x in range(a, c):
                        t = (x - a + 1) / (c - a + 1)
                        O[y, x] = l * (1 - t) + r * t
    pw = move(p, wdx, wdy)
    O[GLASS[1]:GLASS[3], GLASS[0]:GLASS[2]] = pw[GLASS[1]:GLASS[3], GLASS[0]:GLASS[2]]
    small = Image.fromarray(O.astype(np.uint8)).crop((0, 20, PW, PH)).resize((W, H), Image.LANCZOS)
    rooms[n] = np.asarray(small, float)
    save('room_' + n, rooms[n])
    # мастерская Джуна — вход для tools/roomconv.py вместе с комнатами из bit-art/rooms
    import os
    os.makedirs('build/workshop', exist_ok=True)
    Image.fromarray(rooms[n].astype(np.uint8)).save(f'build/workshop/room-workshop-{n}.png')

# Одеяло: для каждого столбца кровати — верх синего одеяла. После питомца эти строки фона
# рисуются заново, и спящий оказывается под одеялом.
day = rooms['day']
BED_X0 = 160
blanket = []
for x in range(BED_X0, W):
    col = day[:, x]
    m = np.nonzero((col[:, 2] > col[:, 0] + 70) & (col[:, 2] > 150))[0]
    m = m[(m > 40) & (m < 78)]
    blanket.append(int(m.min()) if len(m) else 255)

def comps(m, diag=False):
    lab = np.zeros(m.shape, int); n = 0; h, w = m.shape; out = []
    nb = ((1, 0), (-1, 0), (0, 1), (0, -1)) + (((1, 1), (-1, -1), (1, -1), (-1, 1)) if diag else ())
    for y in range(h):
        for x in range(w):
            if m[y, x] and not lab[y, x]:
                n += 1; st = [(y, x)]; lab[y, x] = n; pts = []
                while st:
                    yy, xx = st.pop(); pts.append((yy, xx))
                    for a, b in nb:
                        a += yy; b += xx
                        if 0 <= a < h and 0 <= b < w and m[a, b] and not lab[a, b]:
                            lab[a, b] = n; st.append((a, b))
                out.append(np.array(pts))
    return out

# ---------- крыша ----------

big = load('environments/workshop-and-rooftop.png')
roof = big[478:876, 852:1645].copy()
src = roof.copy()

def paste(ry0, ry1, rx0, rx1, sh, feather=6):
    s = src[ry0:ry1, rx0 + sh:rx1 + sh]
    for i, x in enumerate(range(rx0, rx1)):
        t = min(1, (i + 1) / feather, (rx1 - x) / feather)
        roof[ry0:ry1, x] = roof[ry0:ry1, x] * (1 - t) + s[:, i] * t

paste(130, 258, 294, 412, -100)  # хакер закрывал дома, перила и пол
paste(130, 258, 404, 518, 150)
paste(258, 350, 294, 518, -200, 14)
roofS = np.asarray(Image.fromarray(roof.astype(np.uint8)).crop((4, 58, 789, 343)).resize((W, H), Image.BOX), float)

# Небо — всё, что связано с верхом кадра через цвета неба, облаков и луны.
def skyAt(y): return np.array([19, 32, 62]) + y / 45 * np.array([4, 8, 18])
cand = np.zeros((H, W), bool); moon = np.zeros((H, W), bool)
for y in range(H):
    for x in range(W):
        c = roofS[y, x]; s = skyAt(y)
        if np.abs(c - s).sum() < 18 or (c[2] > s[2] + 4 and c[2] < 110 and c[0] < 45): cand[y, x] = True
        if c[0] > 90 and c[1] > 80 and c[2] < c[0] and y < 40 and x > 150: moon[y, x] = True; cand[y, x] = True
sky = np.zeros((H, W), bool); q = deque((0, x) for x in range(W) if cand[0, x])
while q:
    y, x = q.popleft()
    if sky[y, x] or not cand[y, x]: continue
    sky[y, x] = True
    for a, b in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
        if 0 <= a < H and 0 <= b < W and not sky[a, b]: q.append((a, b))
# облака остаются в картинке, чистое небо и луна — прозрачные
cloud = sky & (roofS[..., 2] > np.array([skyAt(y)[2] for y in range(H)])[:, None] + 6) & ~moon
for k in comps(cloud):  # мелкие крапинки у краёв домов — не облака
    if len(k) < 15: cloud[k[:, 0], k[:, 1]] = False
clear = sky & ~cloud
# луна отдельным спрайтом: ночью рисуется поверх неба
def dilate(m):
    o = m.copy()
    o[1:] |= m[:-1]; o[:-1] |= m[1:]; o[:, 1:] |= m[:, :-1]; o[:, :-1] |= m[:, 1:]
    return o
mx0, my0, mx1, my1 = (lambda ys, xs: (xs.min() - 2, ys.min() - 2, xs.max() + 3, ys.max() + 3))(*np.nonzero(moon))
mm = np.zeros((H, W), bool); mm[max(my0, 0):my1, mx0:mx1] = True  # вокруг луны только небо
ys, xs = np.nonzero(mm)
MOON_BOX = (int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1)
moonRGB = roofS[MOON_BOX[1]:MOON_BOX[3], MOON_BOX[0]:MOON_BOX[2]]
moonA = np.array([[np.abs(roofS[y, x] - skyAt(y)).sum() > 14 for x in range(MOON_BOX[0], MOON_BOX[2])]
                  for y in range(MOON_BOX[1], MOON_BOX[3])])
clear |= mm; cloud &= ~mm
solid = ~clear & ~cloud

def quant(pixels, n):
    """Список цветов -> (палитра, индексы)."""
    im = Image.fromarray(pixels.reshape(1, -1, 3).astype(np.uint8))
    q = im.quantize(colors=n, method=Image.Quantize.MEDIANCUT, kmeans=3)
    pal = np.array(q.getpalette()[:n * 3], float).reshape(-1, 3)
    return pal, np.asarray(q).ravel()

# Днём огни не горят: окна тёмные, плафон фонаря серый, ореола нет.
r_, g_, b_ = roofS[..., 0], roofS[..., 1], roofS[..., 2]
unlit = roofS.copy()
warm = solid & (r_ - b_ > -15)
lum = roofS.mean(2)
head = warm & (lum >= 110)
unlit = fill(unlit, warm)  # ореол на стенах и перилах — заполняем окружением
unlit[head] = np.stack([lum * 0.42, lum * 0.5, lum * 0.75], -1)[head]
cyan = solid & (b_ - r_ > 60) & (g_ > 80)
unlit[cyan] = (22, 32, 60)

def indexed(img):
    idx = np.zeros((H, W), np.uint8)
    cp, ci = quant(img[cloud], 16); idx[cloud] = ci + 1
    sp, si = quant(img[solid], 238); idx[solid] = si + 17
    pal = np.zeros((256, 3)); pal[1:17] = cp; pal[17:255] = sp
    return idx, pal

LIT = ('night', 'dawn', 'evening')  # в эти часы в окнах свет
# Пол прокручивается под шагами: нижние строки делаем бесшовными с периодом ROOF_TILE.
FLOOR_ROW, ROOF_TILE, SEAM = round((320 - 58) * H / 285), 216, 24
def tileable(img):
    img = img.copy()
    for x in range(SEAM):
        t = x / SEAM
        img[FLOOR_ROW:, x] = img[FLOOR_ROW:, x] * t + img[FLOOR_ROW:, ROOF_TILE + x] * (1 - t)
    return img
roofS = tileable(roofS); unlit = tileable(unlit)
roofImg = {True: indexed(roofS), False: indexed(unlit)}

LIGHT = {'night': (0x101932, 0x243958), 'dawn': (0x575083, 0xf1a082), 'morning': (0x438abf, 0xd7e3eb),
         'day': (0x439fd1, 0xb9dded), 'sunset': (0x664979, 0xf58263), 'evening': (0x26244f, 0x845b86)}
def hx(v): return np.array([(v >> 16) & 255, (v >> 8) & 255, v & 255], float)
N = rooms['night'].reshape(-1, 3)
roofPals = []
for n in ORDER:
    lit = n in LIT
    roofIdx, basePal = roofImg[lit]
    X = rooms[n].reshape(-1, 3)
    A = np.hstack([N, np.ones((len(N), 1))]); Mx = np.linalg.lstsq(A, X, rcond=None)[0]
    pal = basePal.copy()
    if n != 'night':
        skyc, hor = hx(LIGHT[n][0]), hx(LIGHT[n][1])
        for i in range(17, 255):
            c = basePal[i]
            d = np.abs(N - c).sum(1); sel = d < 30
            m = np.r_[c, 1] @ Mx
            if sel.sum() >= 40: m = 0.5 * X[sel].mean(0) + 0.5 * m
            if lit and (c[2] - c[0] > 70 or c[0] - c[2] > 40): m = c  # огни не перекрашиваем
            pal[i] = np.clip(m, 0, 255)
        for i in range(1, 17):  # облака — светлые, в тон неба
            c = basePal[i]
            k = np.clip((c.mean() - 40) / 30, 0, 1)
            pal[i] = (skyc * 0.35 + hor * 0.65) * (0.65 - 0.25 * k) + 255 * (0.35 + 0.25 * k)
    roofPals.append(pal)
    if PREVIEW:
        prev = pal[roofIdx]
        top, bot = hx(LIGHT[n][0]), hx(LIGHT[n][1])
        for y in range(H):
            prev[y][clear[y]] = top + (bot - top) * y / (H - 1)
        if n == 'night':
            prev[MOON_BOX[1]:MOON_BOX[3], MOON_BOX[0]:MOON_BOX[2]][moonA] = moonRGB[moonA]
        save('roof_' + n, prev)
RAIL_Y = int(np.nonzero(solid.all(1))[0].min())  # с этой строки перила и пол — ближний слой

# ---------- персонажи ----------

chars = load('characters/bit-11-forms-angular.png')
CELLS = [(24, 16, 382, 262), (402, 16, 758, 262), (778, 16, 1134, 262), (1154, 16, 1510, 262),
         (24, 341, 382, 592), (402, 341, 758, 592), (778, 341, 1134, 592), (1154, 341, 1510, 592),
         (84, 672, 526, 930), (549, 672, 986, 930), (1009, 672, 1451, 930)]
BG = np.array([12, 21, 44])
SC = 0.2235  # Джун с пропеллером ~48 px, как питомец на картинке комнаты

sprites = []
for i, (cx0, cy0, cx1, cy1) in enumerate(CELLS):
    c = chars[cy0:cy1, cx0:cx1]; h, w, _ = c.shape
    d = np.abs(c - BG).sum(2)
    bgm = np.zeros((h, w), bool); q = deque()
    for x in range(w): q.append((0, x)); q.append((h - 1, x))
    for y in range(h): q.append((y, 0)); q.append((y, w - 1))
    while q:
        y, x = q.popleft()
        if bgm[y, x] or d[y, x] > 16: continue
        bgm[y, x] = True
        for yy, xx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
            if 0 <= yy < h and 0 <= xx < w and not bgm[yy, xx]: q.append((yy, xx))
    fg = ~bgm
    dark = c.sum(2) < 75
    bright = fg & ~dark
    foot = np.nonzero(bright.any(1))[0].max()
    for x in range(w):  # тень под ногами убираем — её рисует игра
        col = np.nonzero(bright[:, x])[0]
        lim = (col.max() + 5) if len(col) else -1
        for y in range(max(foot - 14, 0), h):
            if fg[y, x] and dark[y, x] and (lim < 0 or y > lim): fg[y, x] = False
    cs = comps(fg); sizes = [len(k) for k in cs]; big_ = max(sizes)
    for k, pts in enumerate(cs):
        m = np.zeros((h, w), bool); m[pts[:, 0], pts[:, 1]] = True
        edge = m[0].any() or m[-1].any() or m[:, 0].any() or m[:, -1].any()
        if sizes[k] < big_ * 0.004 or (sizes[k] < big_ * 0.05 and edge): fg[m] = False
    ys, xs = np.nonzero(fg)
    c = c[ys.min():ys.max() + 1, xs.min():xs.max() + 1]; f = fg[ys.min():ys.max() + 1, xs.min():xs.max() + 1].astype(float)
    h, w = f.shape; w2, h2 = round(w * SC), round(h * SC)
    A = np.asarray(Image.fromarray((f * 255).astype(np.uint8)).resize((w2, h2), Image.BOX), float) / 255
    C = np.dstack([np.asarray(Image.fromarray((c[..., k] * f).astype(np.uint8)).resize((w2, h2), Image.BOX), float) for k in range(3)])
    C = np.clip(C / np.maximum(A, 1e-3)[..., None], 0, 255)
    sprites.append((C, A >= 0.5))

# Глаза: белые пятна (у Хакера зелёные, у Нейросети голубые). Где поиск путается — заданы вручную.
EYES = {4: [(23, 30, 30, 34), (37, 29, 45, 35)], 5: [(20, 23, 27, 28), (41, 23, 49, 28)],
        9: [(25, 27, 32, 33), (42, 27, 49, 33)], 10: [(17, 28, 27, 34), (33, 28, 44, 34)]}
# Кулаки и ноги (рамки x0,y0,x1,y1) — размечены по спрайтам вручную; их двигает игра.
PARTS = {1: [(0, 24, 9, 39), (32, 24, 41, 39), (5, 38, 16, 41), (25, 38, 36, 41)],
         2: [(0, 31, 11, 42), (48, 31, 59, 42), (6, 42, 21, 48), (40, 42, 54, 48)],
         3: [(3, 33, 13, 44), (46, 33, 56, 44), (9, 41, 21, 45), (39, 41, 51, 45)],
         4: [(6, 35, 16, 46), (49, 34, 59, 45), (10, 45, 24, 52), (41, 45, 53, 52)],
         5: [(1, 26, 10, 38), (58, 26, 67, 38), (7, 37, 25, 47), (43, 37, 61, 47)],
         6: [(1, 32, 10, 45), (55, 32, 64, 45), (8, 45, 21, 53), (44, 45, 58, 53)],
         7: [(1, 39, 12, 49), (50, 39, 60, 49), (5, 49, 19, 54), (42, 49, 57, 54)],
         8: [(0, 34, 10, 47), (56, 34, 65, 47), (5, 49, 20, 55), (43, 49, 59, 55)],
         9: [(10, 25, 20, 38), (55, 25, 64, 38), (0, 0, 0, 0), (0, 0, 0, 0)],    # парит — без ног
         10: [(0, 27, 9, 39), (50, 27, 59, 39), (0, 0, 0, 0), (0, 0, 0, 0)]}
# Колпак и другие шапки заменяют головной убор формы: строки выше hatCut не рисуются,
# мех колпака ложится на строку hatY. Размечено по спрайтам вручную.
HAT = {1: (17, 16), 2: (20, 19), 3: (11, 9), 4: (14, 12), 5: (8, 0), 6: (21, 20), 7: (12, 10), 8: (15, 14), 9: (13, 11)}
meta = []
for i, (C, A) in enumerate(sprites):
    h, w = A.shape
    if i == 0:
        meta.append(dict(eyes=[(0, 0, 0, 0)] * 2, mouth=(0, 0, 0, 0), skin=(0, 0, 0), head=0)); continue
    if i in EYES:
        eyes = EYES[i]
    else:
        r, g, b = C[..., 0], C[..., 1], C[..., 2]
        m = ((g > 150) & (r < 170) & (b < 120)) if i == 7 else ((C.min(2) > 175) & (np.ptp(C, 2) < 60))
        m &= A; m[:int(h * 0.25)] = False; m[int(h * 0.8):] = False
        cs = sorted([k for k in comps(m, True) if len(k) >= 3], key=lambda k: -len(k))[:2]
        cs = sorted(cs, key=lambda k: k[:, 1].mean())
        eyes = [(int(k[:, 1].min()), int(k[:, 0].min()), int(k[:, 1].max()), int(k[:, 0].max())) for k in cs]
    eb = max(e[3] for e in eyes); mx = (eyes[0][2] + eyes[1][0]) / 2
    # рот — тёмное пятно под глазами посередине
    reg = np.zeros((h, w), bool)
    reg[eb + 1:min(h, eb + 10), int(mx - 6):int(mx + 7)] = True
    dk = reg & A & (C.sum(2) < 160)
    if i == 4:  # у Фрилансера оскал — светлые зубы
        dk = reg & A & ((C.sum(2) < 160) | (C.min(2) > 190))
    if dk.any():
        my, mxs_ = np.nonzero(dk)
        mouth = (int(mxs_.min()), int(my.min()), int(mxs_.max()), int(my.max()))
    else:
        mouth = (int(mx - 2), eb + 3, int(mx + 2), eb + 4)
    # кожа — медиана светлых пикселей под глазами, не задевая рта
    band = np.zeros((h, w), bool)
    for (ex0, ey0, ex1, ey1) in eyes: band[ey1 + 1:ey1 + 4, ex0:ex1 + 1] = True
    band &= A & (C.sum(2) > 120) & ~((C.min(2) > 190) & (i != 10))
    if i == 7: band = A & (C.sum(2) < 120); band[:eyes[0][1]] = False; band[eb + 3:] = False
    skin = np.median(C[band], 0) if band.any() else C[A].mean(0)
    rows = A.sum(1); head = int(np.nonzero(rows > rows.max() * 0.45)[0].min())
    meta.append(dict(eyes=eyes, mouth=mouth, skin=tuple(skin), head=head))
    if PREVIEW:
        v = C.copy(); v[~A] = (70, 80, 110)
        for (ex0, ey0, ex1, ey1) in eyes: v[ey0:ey1 + 1, ex0:ex1 + 1] = skin
        save(f'spr_{i}_lids', np.repeat(np.repeat(v, 6, 0), 6, 1))

def cutDiff(n, x0, y0, x1, y1, thr=45, solo=True):
    """Кусок панели как есть: только пиксели, отличные от пустой комнаты того же часа (рамка в сцене)."""
    dx, dy = SHIFT[n]
    full = np.asarray(Image.fromarray(move(P[n], dx, dy).astype(np.uint8)).crop((0, 20, PW, PH)).resize((W, H), Image.LANCZOS), float)
    m = np.abs(full - rooms[n]).sum(2) > thr
    keep = np.zeros_like(m); keep[y0:y1, x0:x1] = True; m &= keep
    for k in comps(m):
        if len(k) < 6: m[k[:, 0], k[:, 1]] = False
    # оставляем саму фигуру и мелочь рядом с ней (пар, крошки), лучи света — нет
    cs = sorted(comps(m), key=lambda k: -len(k)) if solo else [np.zeros((1, 2), int)]
    main = cs[0]; my0, mx0, my1, mx1 = main[:, 0].min(), main[:, 1].min(), main[:, 0].max(), main[:, 1].max()
    for k in cs[1:]:
        near = k[:, 0].min() <= my1 + 4 and k[:, 0].max() >= my0 - 6 and k[:, 1].min() <= mx1 + 6 and k[:, 1].max() >= mx0 - 6
        if len(k) > 25 or not near: m[k[:, 0], k[:, 1]] = False
    m = dilate(m) & keep
    for k in comps(~m):
        if len(k) < 40: m[k[:, 0], k[:, 1]] = True
    ys, xs = np.nonzero(m)
    b = (int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1)
    if PREVIEW:
        v = rooms[n].copy(); v[~m] = v[~m] * 0.4; v[m] = full[m]
        save('cut_%s_%d' % (n, x0), np.repeat(np.repeat(v, 3, 0), 3, 1))
    return full[b[1]:b[3], b[0]:b[2]], m[b[1]:b[3], b[0]:b[2]], b

# середина питомца на панелях (по рамкам масок, в сцене)
PET_MID = {'morning': (250 + 395) / 2 * W / PW, 'sunset': (300 + 420) / 2 * W / PW}
# Спящий Джун (одеяло, шапка на полу), Джун с кофе и Джун с пиццей — куски панелей как есть.
sleepSpr = {n: cutDiff(n, 161, 24, 236, H, solo=False) for n in ('dawn', 'night')}
# кепка с пропеллером на полу у кровати — отдельно: на Новый год вместо неё лежит колпак
capSpr = {n: cutDiff(n, 128, 60, 161, H, solo=False) for n in ('dawn', 'night')}
coffeeSpr = cutDiff('morning', 74, 24, 138, 82)
pizzaSpr = cutDiff('sunset', 86, 24, 165, 85)
# Кружка и коробка пиццы отдельно — для остальных форм.
mugSpr = cutDiff('morning', 79, 40, 88, 66)
boxSpr = cutDiff('sunset', 90, 72, 125, 85)
# Джун за компьютером на стуле — вечерняя панель
deskSpr = cutDiff('evening', 64, 24, 118, 84)

# ---------- вывод ----------

def arr(name, data, typ='uint8_t', per=24):
    lines = []
    for k in range(0, len(data), per):
        lines.append('  ' + ','.join(str(int(v)) for v in data[k:k + per]) + ',')
    return f'static const {typ} {name}[] = {{\n' + '\n'.join(lines) + '\n};\n'

out = ['// Создано tools/artconv.py из bit-art/ — не править руками.', '#include "artdata.h"', '',
       'namespace artdata {', '']
out.append(arr('roof_lit', roofImg[True][0].ravel()))
out.append(arr('roof_dark', roofImg[False][0].ravel()))
out.append('const uint8_t* const ROOF_PX[6] = {' + ', '.join('roof_lit' if n in LIT else 'roof_dark' for n in ORDER) + '};\n')
out.append('const uint16_t ROOF_PAL[6][256] = {\n' + ''.join(
    '  {' + ','.join(str(rgb565(c)) for c in p) + '},\n' for p in roofPals) + '};\n')
out.append(arr('blanket', blanket).replace('static const uint8_t blanket', 'const uint8_t BLANKET_TOP'))

def sprite(name, C, A, ncol=63):
    pix = C[A]
    pal, idx = quant(pix, ncol)
    px = np.zeros(A.shape, np.uint8); px[A] = idx + 1
    out.append(arr(name + '_pal', [0] + [rgb565(c) for c in pal], 'uint16_t', 16))
    out.append(arr(name + '_px', px.ravel()))

for i, (C, A) in enumerate(sprites): sprite(f'form{i}', C, A)
sprite('moon', moonRGB, moonA, 15)
for nm, sp_ in (('sleep_dawn', sleepSpr['dawn']), ('sleep_night', sleepSpr['night']), ('coffee', coffeeSpr),
                ('pizza', pizzaSpr), ('mug', mugSpr), ('box', boxSpr), ('desk', deskSpr),
                ('cap_dawn', capSpr['dawn']), ('cap_night', capSpr['night'])):
    sprite(nm, sp_[0], sp_[1], 127)
def box(b): return '{' + ','.join(str(int(v)) for v in b) + '}'
out.append('const Sprite FORM[11] = {')
for i, (C, A) in enumerate(sprites):
    m = meta[i]
    pr = PARTS.get(i, [(0, 0, 0, 0)] * 4)
    hy, hc = HAT.get(i, (0, 0))
    row = A[min(hy + 2, A.shape[0] - 1)]
    hw = int(np.ptp(np.nonzero(row)[0]) + 1) if row.any() else 0
    out.append(f'  {{{A.shape[1]}, {A.shape[0]}, form{i}_pal, form{i}_px, {{{box(m["eyes"][0])}, {box(m["eyes"][1])}}}, '
               f'{box(m["mouth"])}, {rgb565(m["skin"])}, {m["head"]}, {{{box(pr[0])}, {box(pr[1])}}}, {{{box(pr[2])}, {box(pr[3])}}}, {hy}, {hc}, {hw}}},')
out.append('};')
out.append(f'const Sprite MOON = {{{moonA.shape[1]}, {moonA.shape[0]}, moon_pal, moon_px, {{}}, {{}}, 0, 0, {{}}, {{}}}};')
for nm, sp_ in (('sleep_dawn', sleepSpr['dawn']), ('sleep_night', sleepSpr['night']), ('coffee', coffeeSpr),
                ('pizza', pizzaSpr), ('mug', mugSpr), ('box', boxSpr), ('desk', deskSpr),
                ('cap_dawn', capSpr['dawn']), ('cap_night', capSpr['night'])):
    a_ = sp_[1]
    out.append(f'const Sprite {nm.upper()} = {{{a_.shape[1]}, {a_.shape[0]}, {nm}_pal, {nm}_px, {{}}, {{}}, 0, 0, {{}}, {{}}}};')
out.append('\n}  // namespace artdata\n')
open('core/artdata.cpp', 'w').write('\n'.join(out))

hdr = f"""// Картинки арт-пака для прошивки (данные — в artdata.cpp, создаётся tools/artconv.py).
#pragma once
#include <stdint.h>

namespace artdata {{

// Сцена 240x87 между верхней и нижней панелью; индекс — номер цвета в палитре RGB565.
constexpr int SCENE_W = {W}, SCENE_H = {H};
// Комнаты — в rooms.cpp (tools/roomconv.py); здесь крыша, персонажи и сцены Джуна в мастерской.
extern const uint8_t* const ROOF_PX[6];   // крыша; 0 — небо (рисуется градиентом по часам)
extern const uint16_t ROOF_PAL[6][256];
constexpr int ROOF_FLOOR_Y = {FLOOR_ROW}, ROOF_TILE = {ROOF_TILE};  // с этой строки пол, бесшовный с периодом ROOF_TILE
constexpr int MOON_X = {MOON_BOX[0]}, MOON_Y = {MOON_BOX[1]};
// Верх одеяла по столбцам начиная с BLANKET_X0 (255 — одеяла нет).
constexpr int BLANKET_X0 = {BED_X0};
extern const uint8_t BLANKET_TOP[];
// Экран терминала: место для «>_» (координаты в сцене).
// Стекло окна в сцене: погода показывается только в нём.
constexpr int WIN_X0 = {math.ceil(GLASS[0] * W / PW)}, WIN_Y0 = {math.ceil((GLASS[1] - 20) * H / (PH - 20))};
constexpr int WIN_X1 = {int(GLASS[2] * W / PW)}, WIN_Y1 = {int((GLASS[3] - 20) * H / (PH - 20))};
constexpr int TERM_X = {round(TXT[0] * W / PW)}, TERM_Y = {round((TXT[1] - 20) * H / (PH - 20))};

// Персонаж: индекс 0 — прозрачно. Глаза и рот — рамки x0,y0,x1,y1 включительно,
// skin — цвет кожи для век, head — верх головы (под шапку), arm и leg — кулаки и ноги
// (левый, правый; пустая рамка — части нет).
struct Sprite {{
  uint8_t w, h;
  const uint16_t* pal;
  const uint8_t* px;
  uint8_t eye[2][4];
  uint8_t mouth[4];
  uint16_t skin;
  uint8_t head;
  uint8_t arm[2][4];
  uint8_t leg[2][4];
  uint8_t hatY, hatCut, hatW;  // шапка: строка меха, выше какой строки убор формы скрыт, ширина головы
}};
extern const Sprite FORM[11];
extern const Sprite MOON;
// Спящий Джун с панелей «рассвет» и «ночь» (с одеялом и шапкой на полу) — левый верхний угол в сцене.
extern const Sprite SLEEP_DAWN, SLEEP_NIGHT;
constexpr int SLEEP_DAWN_X = {sleepSpr['dawn'][2][0]}, SLEEP_DAWN_Y = {sleepSpr['dawn'][2][1]};
constexpr int SLEEP_NIGHT_X = {sleepSpr['night'][2][0]}, SLEEP_NIGHT_Y = {sleepSpr['night'][2][1]};
// кепка Джуна на полу у кровати (во сне); на Новый год вместо неё лежит колпак
extern const Sprite CAP_DAWN, CAP_NIGHT;
constexpr int CAP_DAWN_X = {capSpr['dawn'][2][0]}, CAP_DAWN_Y = {capSpr['dawn'][2][1]};
constexpr int CAP_NIGHT_X = {capSpr['night'][2][0]}, CAP_NIGHT_Y = {capSpr['night'][2][1]};
// Джун с кофе (утро) и с пиццей (закат): питомец вместе с едой, как на картинке.
// *_DX — где середина питомца относительно левого края, низ спрайта — пол.
extern const Sprite COFFEE, PIZZA, MUG, BOX;
// Джун на стуле за компьютером (вечер) — левый верхний угол в сцене.
extern const Sprite DESK;
constexpr int DESK_X = {deskSpr[2][0]}, DESK_Y = {deskSpr[2][1]};
constexpr int COFFEE_DX = {round(PET_MID['morning'] - coffeeSpr[2][0])}, PIZZA_DX = {round(PET_MID['sunset'] - pizzaSpr[2][0])};
constexpr int COFFEE_Y = {coffeeSpr[2][1]}, PIZZA_Y = {pizzaSpr[2][1]};  // верх в сцене

}}  // namespace artdata
"""
open('core/artdata.h', 'w').write(hdr)
print('ok', 'rail', RAIL_Y, 'moon', MOON_BOX, 'blanket', blanket)
for i, m in enumerate(meta): print(i, sprites[i][1].shape, m)
