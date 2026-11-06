#!/bin/sh
# Сборка ядра в WebAssembly (clang + wasm-ld, без стандартной библиотеки)
set -e
cd "$(dirname "$0")/.."
mkdir -p build
clang --target=wasm32 -O2 -std=c++17 -nostdlib -ffreestanding -fno-exceptions -fno-rtti \
  -fno-threadsafe-statics -Wall -Icore \
  -Wl,--no-entry -Wl,--export=__wasm_call_ctors -Wl,--strip-all -Wl,-z,stack-size=65536 \
  -o build/bit.wasm web/emu.cpp core/*.cpp
ls -l build/bit.wasm
