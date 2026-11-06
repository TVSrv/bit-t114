package ru.bitpet.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder
import kotlin.math.roundToInt

// Погода с Open-Meteo (бесплатно, без ключа): город -> координаты -> текущая погода.
object Weather {
    data class Place(val name: String, val lat: Double, val lon: Double)
    data class Now(val kind: Int, val temp: Int)  // вид для платы: 1 ясно, 2 облачно, 3 дождь, 4 снег, 5 гроза, 6 туман

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

    suspend fun current(p: Place): Now? = runCatching {
        val cur = get("https://api.open-meteo.com/v1/forecast?latitude=${p.lat}&longitude=${p.lon}&current=temperature_2m,weather_code")
            .getJSONObject("current")
        Now(kindOf(cur.getInt("weather_code")), cur.getDouble("temperature_2m").roundToInt())
    }.getOrNull()

    // коды WMO -> вид погоды на плате
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
