#!/bin/sh
# Собирает страницу с эмулятором: вставляет bit.wasm в base64
set -e
cd "$(dirname "$0")/.."
web/build.sh >/dev/null
python3 - <<'PY'
import base64
src = open("web/index.src.html", encoding="utf-8").read()
b = base64.b64encode(open("build/bit.wasm", "rb").read()).decode()
open("build/index.html", "w", encoding="utf-8").write(src.replace("__WASM__", b))
PY
ls -l build/index.html
