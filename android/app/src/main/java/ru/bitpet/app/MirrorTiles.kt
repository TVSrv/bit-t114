package ru.bitpet.app

import java.util.Base64

/**
 * Atomic BLE mirror decoder (protocol v2).
 *
 * B|seq|tileCount|baseSeq starts one immutable RGB565 frame delta.
 * T|seq|tileIndex|base64 carries one 16x4 RGB565 LE tile.
 * E|seq|checksum closes the frame. Publish only after ALL tiles and
 * the FNV-1a checksum match. A corrupt or missing chunk requests a new
 * full snapshot; no mixed-timestamp pixels can reach Compose.
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

    private var committed = ByteArray(WIDTH * HEIGHT * 2)
    private var committedSequence = 0
    private var hasCommitted = false
    private var working: ByteArray? = null
    private var incomingSequence = -1
    private var expectedCount = 0
    private var receivedCount = 0
    private val received = BooleanArray(COUNT)
    private var resyncRequired = false

    fun clear() {
        committed = ByteArray(WIDTH * HEIGHT * 2)
        committedSequence = 0
        hasCommitted = false
        resetTransaction()
        resyncRequired = false
    }

    fun takeResyncRequest(): Boolean {
        val flag = resyncRequired
        resyncRequired = false
        return flag
    }

    /** Retain last good screen, reset only frame sequencing for full resync. */
    fun prepareResync() {
        hasCommitted = false
        committedSequence = 0
        resetTransaction()
    }

    private fun resetTransaction() {
        working = null
        incomingSequence = -1
        expectedCount = 0
        receivedCount = 0
        received.fill(false)
    }

    private fun invalidate() {
        resetTransaction()
        resyncRequired = true
    }

    /** Returns a complete immutable ARGB image or null if still assembling. */
    fun accept(line: String): IntArray? {
        if (line.startsWith("B|")) {
            val p = line.split('|')
            if (p.size != 4) { invalidate(); return null }
            val seq = p[1].toIntOrNull() ?: run { invalidate(); return null }
            val count = p[2].toIntOrNull() ?: run { invalidate(); return null }
            val base = p[3].toIntOrNull() ?: run { invalidate(); return null }
            if (seq !in 0..65535 || base !in 0..65535 || count !in 1..COUNT ||
                working != null || (hasCommitted && base != committedSequence) ||
                (!hasCommitted && (base != 0 || count != COUNT))) {
                invalidate()
                return null
            }
            incomingSequence = seq
            working = committed.copyOf()
            expectedCount = count
            receivedCount = 0
            received.fill(false)
            return null
        }
        if (line.startsWith("T|")) {
            val target = working ?: return null
            val p = line.split('|', limit = 4)
            if (p.size != 4) { invalidate(); return null }
            val seq = p[1].toIntOrNull() ?: run { invalidate(); return null }
            val id = p[2].toIntOrNull() ?: run { invalidate(); return null }
            if (seq != incomingSequence || id !in 0 until COUNT || received[id]) {
                invalidate()
                return null
            }
            val x = id % COLS * TW
            val y = id / COLS * TH
            val w = minOf(TW, WIDTH - x)
            val h = minOf(TH, HEIGHT - y)
            val raw = try { Base64.getDecoder().decode(p[3]) }
                catch (_: IllegalArgumentException) { invalidate(); return null }
            if (raw.size != w * h * 2) { invalidate(); return null }
            var input = 0
            for (row in 0 until h) {
                val dst = ((y + row) * WIDTH + x) * 2
                System.arraycopy(raw, input, target, dst, w * 2)
                input += w * 2
            }
            received[id] = true
            receivedCount++
            return null
        }
        if (line.startsWith("E|")) {
            val target = working ?: return null
            val p = line.split('|')
            if (p.size != 3) { invalidate(); return null }
            val seq = p[1].toIntOrNull() ?: run { invalidate(); return null }
            val expected = p[2].toLongOrNull(16) ?: run { invalidate(); return null }
            if (seq != incomingSequence || receivedCount != expectedCount ||
                expected !in 0..0xffffffffL || checksum(target) != expected) {
                invalidate()
                return null
            }
            committed = target
            committedSequence = seq
            hasCommitted = true
            resetTransaction()
            return asArgb(committed) // a fresh array never mutated after publication
        }
        return null
    }

    private fun checksum(data: ByteArray): Long {
        var hash = 0x811c9dc5.toInt()
        for (b in data) {
            hash = (hash xor (b.toInt() and 255)) * 16777619
        }
        return hash.toLong() and 0xffffffffL
    }

    private fun asArgb(data: ByteArray): IntArray {
        val result = IntArray(WIDTH * HEIGHT)
        var offset = 0
        for (i in result.indices) {
            val rgb = (data[offset].toInt() and 255) or
                ((data[offset + 1].toInt() and 255) shl 8)
            offset += 2
            var r = (rgb shr 11) shl 3
            var g = ((rgb shr 5) and 63) shl 2
            var b = (rgb and 31) shl 3
            r = r or (r shr 5)
            g = g or (g shr 6)
            b = b or (b shr 5)
            result[i] = 0xff000000.toInt() or (r shl 16) or (g shl 8) or b
        }
        return result
    }
}
