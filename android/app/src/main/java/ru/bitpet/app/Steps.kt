package ru.bitpet.app

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager

// Шагомер телефона: встроенный счётчик шагов (копит с перезагрузки телефона).
// Отдаём только прирост с прошлой отправки; база хранится, чтобы не считать шаги дважды.
class Steps(private val ctx: Context) : SensorEventListener {
    private val prefs = ctx.getSharedPreferences("steps", Context.MODE_PRIVATE)
    @Volatile private var lastCounter = -1L

    val available: Boolean
        get() = ctx.getSystemService(SensorManager::class.java)?.getDefaultSensor(Sensor.TYPE_STEP_COUNTER) != null

    fun start() {
        val sm = ctx.getSystemService(SensorManager::class.java) ?: return
        val s = sm.getDefaultSensor(Sensor.TYPE_STEP_COUNTER) ?: return
        sm.registerListener(this, s, SensorManager.SENSOR_DELAY_NORMAL, 30_000_000)  // пачками до 30 с — экономно
    }

    fun stop() { ctx.getSystemService(SensorManager::class.java)?.unregisterListener(this) }

    override fun onSensorChanged(e: SensorEvent) { lastCounter = e.values[0].toLong() }
    override fun onAccuracyChanged(s: Sensor?, a: Int) {}

    // сколько шагов прибавилось с прошлой отправки (и запомнить новую базу)
    fun takeDelta(): Int {
        val now = lastCounter
        if (now < 0) return 0
        val base = prefs.getLong("base", -1)
        prefs.edit().putLong("base", now).apply()
        if (base < 0 || now < base) return 0  // первый запуск или телефон перезагружен — начинаем отсчёт заново
        return (now - base).coerceAtMost(20_000).toInt()
    }

    // вернуть невзятые шаги (плата не ответила) — отправим в следующий раз
    fun giveBack(n: Int) {
        val base = prefs.getLong("base", -1)
        if (base >= 0 && n > 0) prefs.edit().putLong("base", base - n).apply()
    }
}
