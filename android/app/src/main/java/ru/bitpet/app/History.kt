package ru.bitpet.app

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import java.io.File

// История шкал: раз в 5 минут, хранится неделя (файл в памяти приложения).
object History {
    data class Point(val t: Long, val hunger: Int, val happy: Int, val energy: Int, val discipline: Int)

    val points = MutableStateFlow<List<Point>>(emptyList())
    private var loaded = false
    private const val STEP = 5 * 60_000L
    private const val KEEP = 7 * 24 * 3600_000L

    private fun file(ctx: Context) = File(ctx.filesDir, "history.csv")

    fun load(ctx: Context) {
        if (loaded) return
        loaded = true
        val f = file(ctx)
        if (!f.exists()) return
        val cut = System.currentTimeMillis() - KEEP
        points.value = f.readLines().mapNotNull { l ->
            val v = l.split(',').mapNotNull { it.toLongOrNull() }
            if (v.size == 5 && v[0] > cut) Point(v[0], v[1].toInt(), v[2].toInt(), v[3].toInt(), v[4].toInt()) else null
        }
    }

    fun add(ctx: Context, p: PetState) {
        load(ctx)
        val now = System.currentTimeMillis()
        val last = points.value.lastOrNull()
        if (last != null && now - last.t < STEP) return
        val pt = Point(now, p.hunger, p.happy, p.energy, p.discipline)
        val cut = now - KEEP
        points.value = (points.value.filter { it.t > cut } + pt)
        val f = file(ctx)
        if (points.value.size % 50 == 0) f.writeText(points.value.joinToString("\n") { "${it.t},${it.hunger},${it.happy},${it.energy},${it.discipline}" } + "\n")
        else f.appendText("${pt.t},${pt.hunger},${pt.happy},${pt.energy},${pt.discipline}\n")
    }
}
