#!/usr/bin/env python3
"""Собирает все тексты сообщений (say и форматы toastBuf) в sim/toasts.inc для проверки ширины."""
import re, glob
out = set()
for f in glob.glob("core/*.cpp"):
    src = open(f, encoding="utf-8").read()
    for m in re.finditer(r'say\(\s*"((?:[^"\\]|\\.)*)"', src):
        out.add(m.group(1))
    for m in re.finditer(r'say\(\s*[^"(),]+\?\s*"((?:[^"\\]|\\.)*)"\s*:\s*"((?:[^"\\]|\\.)*)"', src):
        out.update(m.groups())
    for m in re.finditer(r'sfmt\(toastBuf, sizeof toastBuf, "((?:[^"\\]|\\.)*)"', src):
        s = m.group(1).replace("%02d", "07").replace("%u", "9999").replace("%d", "9999").replace("%s", "Hello World")
        out.add(s)
with open("sim/toasts.inc", "w", encoding="utf-8") as o:
    for s in sorted(out):
        o.write('"%s",\n' % s)
print(len(out), "сообщений")
