package ru.bitpet.app

import org.junit.Assert.*
import org.junit.Test
import java.util.Base64

class MirrorTilesTest {
    private fun pattern(seed: Int) = ByteArray(MirrorTiles.WIDTH * MirrorTiles.HEIGHT * 2) { i ->
        (i * 37 + seed * 11 + (i / 480) * 19).toByte()
    }

    private fun crc(data: ByteArray): String {
        var hash = 0x811c9dc5.toInt()
        for (b in data) hash = (hash xor (b.toInt() and 255)) * 16777619
        return (hash.toLong() and 0xffffffffL).toString(16).uppercase().padStart(8, '0')
    }

    private fun tile(data: ByteArray, seq: Int, id: Int): String {
        val x = (id % 15) * 16
        val y = (id / 15) * 4
        val w = minOf(16, 240 - x)
        val h = minOf(4, 135 - y)
        val bytes = ByteArray(w * h * 2)
        var n = 0
        for (dy in 0 until h) {
            System.arraycopy(data, ((y + dy) * 240 + x) * 2, bytes, n, w * 2)
            n += w * 2
        }
        return "T|$seq|$id|" + Base64.getEncoder().encodeToString(bytes)
    }

    private fun initial(decoder: MirrorTiles, data: ByteArray, seq: Int = 1): IntArray {
        assertNull(decoder.accept("B|$seq|510|0"))
        for (id in 0 until MirrorTiles.COUNT) {
            assertNull("Partial initial frame must never be visible", decoder.accept(tile(data, seq, id)))
        }
        return decoder.accept("E|$seq|${crc(data)}") ?: error("full initial frame not committed")
    }

    @Test fun noFrameIsPublishedUntilAllTilesAndChecksumArrive() {
        val decoder = MirrorTiles()
        val data = pattern(4)
        val full = initial(decoder, data)
        assertEquals(32400, full.size)
        assertEquals(0xff000000.toInt() or convertPixel(data, 0), full[0])
        assertFalse(decoder.takeResyncRequest())
    }

    private fun convertPixel(data: ByteArray, px: Int): Int {
        val color = (data[px * 2].toInt() and 255) or ((data[px * 2 + 1].toInt() and 255) shl 8)
        var r = (color shr 11) shl 3
        var g = ((color shr 5) and 63) shl 2
        var b = (color and 31) shl 3
        r = r or (r shr 5)
        g = g or (g shr 6)
        b = b or (b shr 5)
        return (r shl 16) or (g shl 8) or b
    }

    @Test fun twoFrameUpdatesAreAtomicAndPreviousFrameNeverChanges() {
        val decoder = MirrorTiles()
        val original = pattern(10)
        val frame1 = initial(decoder, original)
        val altered = original.copyOf()
        // Simulate pet movement: change tiles on both sides of sprite.
        val first = 15 * 19 + 6
        val second = 15 * 20 + 11
        for (id in intArrayOf(first, second)) {
            val x = (id % 15) * 16
            val y = (id / 15) * 4
            for (dy in 0 until 4) for (dx in 0 until 16) {
                val p = ((y + dy) * 240 + x + dx) * 2
                altered[p] = 17
                altered[p + 1] = 100
            }
        }
        assertNull(decoder.accept("B|2|2|1"))
        assertNull(decoder.accept(tile(altered, 2, first)))
        assertNull("Do not publish one new tile and one old tile", decoder.accept(tile(altered, 2, second)))
        val frame2 = decoder.accept("E|2|${crc(altered)}")!!
        assertNotSame(frame1, frame2)
        assertNotEquals(frame1[(first / 15 * 4) * 240 + first % 15 * 16],
                        frame2[(first / 15 * 4) * 240 + first % 15 * 16])
        assertEquals("Published frame1 is immutable", 0xff000000.toInt() or convertPixel(original, 0), frame1[0])
        assertFalse(decoder.takeResyncRequest())
    }

    @Test fun lostTileAndWrongChecksumAreRejectedNotDisplayed() {
        val decoder = MirrorTiles()
        val data = pattern(7)
        initial(decoder, data)
        assertNull(decoder.accept("B|2|2|1"))
        assertNull(decoder.accept(tile(data, 2, 5)))
        assertNull("Incomplete frame must not be shown", decoder.accept("E|2|${crc(data)}"))
        assertTrue(decoder.takeResyncRequest())
        decoder.prepareResync()
        initial(decoder, data)
        assertNull(decoder.accept("B|2|1|1"))
        assertNull(decoder.accept(tile(data, 2, 3)))
        assertNull(decoder.accept("E|2|DEADBEEF"))
        assertTrue(decoder.takeResyncRequest())
    }

    @Test fun mismatchedSequenceAndDuplicatesCauseResync() {
        val decoder = MirrorTiles()
        val data = pattern(5)
        initial(decoder, data)
        assertNull(decoder.accept("B|2|1|0")) // invalid baseline sequence
        assertTrue(decoder.takeResyncRequest())
        decoder.prepareResync()
        initial(decoder, data)
        assertNull(decoder.accept("B|2|1|1"))
        assertNull(decoder.accept(tile(data, 2, 1)))
        assertNull(decoder.accept(tile(data, 2, 1)))
        assertTrue(decoder.takeResyncRequest())
    }
}
