package ru.bitpet.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder
import kotlin.math.roundToInt

// Реальная текущая погода и 6 трёхчасовых точек от Open-Meteo.
// Значение -1 означает, что показатель не был получен; ничего не подставляем.
object Weather {
    data class Place(val name: String, val lat: Double, val lon: Double)
    data class Hour(val hour: Int, val temp: Int, val kind: Int)
    data class Now(
        val kind: Int, val temp: Int,
        val humidity: Int = -1, val windMs: Int = -1, val feels: Int = -1,
        val pressureMm: Int = -1, val precipitation: Int = -1,
        val hourly: List<Hour> = emptyList()
    )

    private suspend fun get(url: String): JSONObject = withContext(Dispatchers.IO) {
        val c = URL(url).openConnection() as HttpURLConnection
        c.connectTimeout = 10_000; c.readTimeout = 10_000
        try { JSONObject(c.inputStream.bufferedReader().readText()) } finally { c.disconnect() }
    }

    suspend fun find(city: String): Place? = runCatching {
        val q = URLEncoder.encode(city.trim(), "UTF-8")
        val r = get("https://geocoding-api.open-meteo.com/v1/search?name=$q&count=1&language=ru").optJSONArray("results")
            ?: return null
        val o = r.getJSONObject(0)
        val region = o.optString("admin1").takeIf { it.isNotBlank() && it != o.getString("name") }
        Place(o.getString("name") + (region?.let { ", $it" } ?: ""), o.getDouble("latitude"), o.getDouble("longitude"))
    }.getOrNull()

    private fun number(o: JSONObject, key: String): Int = o.optDouble(key, Double.NaN)
        .takeIf { it.isFinite() }?.roundToInt() ?: -1

    suspend fun current(p: Place): Now? = runCatching {
        val root = get(
            "https://api.open-meteo.com/v1/forecast?latitude=${p.lat}&longitude=${p.lon}" +
                "&current=temperature_2m,weather_code,relative_humidity_2m,apparent_temperature,wind_speed_10m,pressure_msl" +
                "&hourly=temperature_2m,weather_code,precipitation_probability" +
                "&wind_speed_unit=ms&timezone=auto&forecast_days=2"
        )
        val cur = root.getJSONObject("current")
        val hourly = root.optJSONObject("hourly")
        val times = hourly?.optJSONArray("time")
        val temps = hourly?.optJSONArray("temperature_2m")
        val codes = hourly?.optJSONArray("weather_code")
        val pop = hourly?.optJSONArray("precipitation_probability")
        val key = cur.optString("time").take(13)  // ISO local time from same response
        val closest = if (times != null && key.length == 13) {
            (0 until times.length()).firstOrNull { times.optString(it).startsWith(key) } ?: -1
        } else -1
        val points = if (closest >= 0 && times != null && temps != null && codes != null) {
            (closest until minOf(times.length(), temps.length(), codes.length()))
                .filter { times.optString(it).drop(11).take(2).toIntOrNull()?.rem(3) == 0 }
                .take(6).mapNotNull { i ->
                    val hr = times.optString(i).drop(11).take(2).toIntOrNull()
                    val temperature = temps.optDouble(i, Double.NaN)
                    if (hr == null || !temperature.isFinite()) null
                    else Hour(hr, temperature.roundToInt().coerceIn(-60, 70), kindOf(codes.optInt(i)))
                }
        } else emptyList()
        val pressureHpa = cur.optDouble("pressure_msl", Double.NaN)
        Now(
            kind = kindOf(cur.getInt("weather_code")),
            temp = cur.getDouble("temperature_2m").roundToInt().coerceIn(-60, 70),
            humidity = number(cur, "relative_humidity_2m").takeIf { it in 0..100 } ?: -1,
            windMs = number(cur, "wind_speed_10m").takeIf { it in 0..99 } ?: -1,
            feels = number(cur, "apparent_temperature").takeIf { it in -60..70 } ?: -1,
            pressureMm = if (pressureHpa.isFinite()) (pressureHpa * 0.750061683).roundToInt().takeIf { it in 500..900 } ?: -1 else -1,
            precipitation = if (closest >= 0) pop?.optInt(closest, -1)?.takeIf { it in 0..100 } ?: -1 else -1,
            hourly = points
        )
    }.getOrNull()

    // Коды WMO -> вид погоды на плате.
    private fun kindOf(code: Int) = when (code) {
        0 -> 1
        1, 2, 3 -> 2
        45, 48 -> 6
        in 51..67, in 80..82 -> 3
        in 71..77, 85, 86 -> 4
        in 95..99 -> 5
        else -> 2
    }

    val names = listOf("", "ясно", "облачно", "дождь", "снег", "гроза", "туман")
}
