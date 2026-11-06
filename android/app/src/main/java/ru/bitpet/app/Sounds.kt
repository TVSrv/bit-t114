package ru.bitpet.app

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlin.math.PI
import kotlin.math.sin

// 8-битные звуки вместо динамика на плате: синтезируются здесь же, без звуковых файлов.
object Sounds {
    enum class Kind { CALL, ALARM, ACHIEVE, GROW, FRIEND, DEATH, QUEST, TAP }

    private const val RATE = 22050
    private val scope = CoroutineScope(Dispatchers.Default)
    private var alarmJob: Job? = null

    // ноты: частота, длительность мс; 0 Гц — пауза
    private fun melody(k: Kind): List<Pair<Double, Int>> = when (k) {
        Kind.CALL -> listOf(1568.0 to 70, 0.0 to 40, 1568.0 to 70, 0.0 to 40, 2093.0 to 110)
        Kind.ALARM -> listOf(2093.0 to 90, 1568.0 to 90, 2093.0 to 90, 1568.0 to 90, 2093.0 to 90, 0.0 to 450)
        Kind.ACHIEVE -> listOf(523.0 to 90, 659.0 to 90, 784.0 to 90, 1047.0 to 160, 0.0 to 40, 784.0 to 80, 1047.0 to 260)
        Kind.GROW -> listOf(392.0 to 80, 523.0 to 80, 659.0 to 80, 784.0 to 80, 1047.0 to 80, 1319.0 to 220)
        Kind.FRIEND -> listOf(880.0 to 110, 0.0 to 50, 1319.0 to 160)
        Kind.DEATH -> listOf(523.0 to 260, 494.0 to 260, 466.0 to 260, 440.0 to 520)
        Kind.QUEST -> listOf(1047.0 to 70, 1319.0 to 70, 1568.0 to 70, 2093.0 to 140)
        Kind.TAP -> listOf(1760.0 to 25)
    }

    private fun render(notes: List<Pair<Double, Int>>, volume: Double): ShortArray {
        val total = notes.sumOf { it.second } * RATE / 1000
        val out = ShortArray(total)
        var i = 0
        for ((f, ms) in notes) {
            val n = ms * RATE / 1000
            for (k in 0 until n) {
                if (i >= total) break
                if (f > 0) {
                    val t = k.toDouble() / RATE
                    // квадратная волна, смягчённая: как у старых тамагочи, но не режет слух
                    val sq = if (sin(2 * PI * f * t) >= 0) 1.0 else -1.0
                    val env = minOf(1.0, k / 60.0) * minOf(1.0, (n - k) / 200.0)
                    out[i] = (sq * env * volume * 9000).toInt().toShort()
                }
                i++
            }
        }
        return out
    }

    private fun play(k: Kind) {
        val pcm = render(melody(k), if (k == Kind.TAP) 0.25 else 0.8)
        val usage = if (k == Kind.ALARM) AudioAttributes.USAGE_ALARM else AudioAttributes.USAGE_NOTIFICATION_EVENT
        val track = AudioTrack.Builder()
            .setAudioAttributes(AudioAttributes.Builder().setUsage(usage).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build())
            .setAudioFormat(AudioFormat.Builder().setSampleRate(RATE).setEncoding(AudioFormat.ENCODING_PCM_16BIT).setChannelMask(AudioFormat.CHANNEL_OUT_MONO).build())
            .setTransferMode(AudioTrack.MODE_STATIC)
            .setBufferSizeInBytes(pcm.size * 2)
            .build()
        track.write(pcm, 0, pcm.size)
        track.play()
        scope.launch { delay(pcm.size * 1000L / RATE + 200); track.release() }
    }

    fun play(k: Kind, enabled: Boolean) {
        if (!enabled) return
        if (k == Kind.ALARM) { startAlarm(); return }
        scope.launch { runCatching { play(k) } }
    }

    // будильник звенит, пока его не выключат на плате или в приложении, но не дольше минуты
    private fun startAlarm() {
        alarmJob?.cancel()
        alarmJob = scope.launch {
            val end = System.currentTimeMillis() + 60_000
            while (isActive && System.currentTimeMillis() < end) {
                runCatching { play(Kind.ALARM) }
                delay(melody(Kind.ALARM).sumOf { it.second }.toLong())
            }
        }
    }

    fun stopAlarm() { alarmJob?.cancel(); alarmJob = null }
    val alarmRinging get() = alarmJob?.isActive == true
}
