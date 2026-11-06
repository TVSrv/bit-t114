package ru.bitpet.app

import android.util.Base64

/**
 * Lossless RGB565 frame assembled from firmware tiles over Nordic UART.
 *
 * Each tile is F|<tile index>|<Base64 RGB565 little-endian>; unknown lines are
 * ignored. The protocol is stable across firmware graphics/room changes.
 * No downloaded executable code and no dependence on firmware sprite assets.
 */
internal class MirrorTiles {
    companion object {
        const val WIDTH = 240
        const val HEIGHT = 135
        private const val TW = 16
        private const val TH = 4
        private const val COLS = (WIDTH + TW - 1) / TW
        private const val ROWS = (HEIGHT + TH - 1) / TH
        const val COUNT = COLS * ROWS
    }

    private val pixels = IntArray(WIDTH * HEIGHT) { 0xff101b31.toInt() }
    private val seen = BooleanArray(COUNT)
    private var received = 0
    private var lastPublish = 0L

    fun clear() {
        seen.fill(false)
        received = 0
        lastPublish = 0
    }

    /**
     * Returns a frame only after every tile has been received at least once.
     * Invalid packets are dropped without affecting a pending BLE command.
     */
    fun accept(line: String, now: Long = System.currentTimeMillis()): IntArray? {
        if (!line.startsWith("F|")) return null
        val separator = line.indexOf('|', 2)
        if (separator < 0) return null
        val id = line.substring(2, separator).toIntOrNull() ?: return null
        if (id !in 0 until COUNT) return null
        val tileX = id % COLS * TW
        val tileY = id / COLS * TH
        val width = minOf(TW, WIDTH - tileX)
        val height = minOf(TH, HEIGHT - tileY)
        val raw = try {
            Base64.decode(line.substring(separator + 1), Base64.NO_WRAP)
        } catch (_: IllegalArgumentException) {
            return null
        }
        if (raw.size != width * height * 2) return null

        var p = 0
        for (y in 0 until height) {
            val row = (tileY + y) * WIDTH + tileX
            for (x in 0 until width) {
                val rgb = (raw[p].toInt() and 255) or ((raw[p + 1].toInt() and 255) shl 8)
                p += 2
                var r = (rgb shr 11) shl 3
                var g = ((rgb shr 5) and 63) shl 2
                var b = (rgb and 31) shl 3
                r = r or (r shr 5)
                g = g or (g shr 6)
                b = b or (b shr 5)
                pixels[row + x] = (0xff000000.toInt() or (r shl 16) or (g shl 8) or b)
            }
        }
        if (!seen[id]) {
            seen[id] = true
            ++received
        }
        if (received == COUNT && now - lastPublish >= 150L) {
            lastPublish = now
            return pixels.copyOf()
        }
        return null
    }
}
