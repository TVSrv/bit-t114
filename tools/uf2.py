# После сборки делает firmware.uf2 (для прошивки перетаскиванием на диск HT-n5262).
Import("env")
import os, struct

FAMILY_NRF52840 = 0xADA52840
START = 0x26000  # после SoftDevice S140 6.1.1


def make_uf2(src_hex, dst):
    data = {}
    base = 0
    for line in open(src_hex):
        line = line.strip()
        if not line.startswith(":"):
            continue
        b = bytes.fromhex(line[1:])
        n, addr, typ = b[0], (b[1] << 8) | b[2], b[3]
        payload = b[4:4 + n]
        if typ == 0:
            for i, v in enumerate(payload):
                data[base + addr + i] = v
        elif typ == 2:
            base = ((payload[0] << 8) | payload[1]) << 4
        elif typ == 4:
            base = ((payload[0] << 8) | payload[1]) << 16
    addrs = sorted(a for a in data if a >= START)
    if not addrs:
        raise SystemExit("пустой hex")
    lo, hi = addrs[0] & ~0xFF, addrs[-1]
    blocks = []
    for a in range(lo, hi + 1, 256):
        chunk = bytes(data.get(a + i, 0xFF) for i in range(256))
        if any(a + i in data for i in range(256)):
            blocks.append((a, chunk))
    out = bytearray()
    for i, (a, chunk) in enumerate(blocks):
        hdr = struct.pack("<IIIIIIII", 0x0A324655, 0x9E5D5157, 0x00002000, a, 256, i, len(blocks), FAMILY_NRF52840)
        out += hdr + chunk + bytes(476 - 256) + struct.pack("<I", 0x0AB16F30)
    open(dst, "wb").write(out)


def after_build(source, target, env):
    hexf = os.path.join(env.subst("$BUILD_DIR"), "firmware.hex")
    dst = os.path.join(env.subst("$BUILD_DIR"), "firmware.uf2")
    make_uf2(hexf, dst)
    print("UF2:", dst)


env.AddPostAction("$BUILD_DIR/firmware.hex", after_build)
