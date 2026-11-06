package ru.bitpet.app

// Состояние питомца из ответа платы на «data»: «D|3|поле0|поле1|…» (порядок — core/remote.cpp).
data class PetState(
    val raw: IntArray,
    val form: Int, val alive: Boolean, val ageMin: Long,
    val hunger: Int, val happy: Int, val energy: Int, val discipline: Int,
    val weight: Int, val bugs: Int, val sick: Boolean, val asleep: Boolean, val lightOff: Boolean,
    val call: Int, val epoch: Long, val clockSource: Int, val clockSure: Boolean,
    val generation: Int, val trait: Int, val favFood: Int, val favGame: Int,
    val totalMeters: Long, val quests: Int, val friends: Int, val achievements: Long,
    val battery: Int, val charging: Boolean, val mistakes: Int, val lifePct: Int,
    val stepsToday: Long = 0, val coins: Long = 0, val formsSeen: Int = 0,
    val menuOpen: Boolean = false, val menuCursor: Int = -1, val boardScene: Int = 0,
) {
    val formName get() = FORMS.getOrElse(form) { "?" }
    val ageText get() = "${ageMin / 1440} дн ${ageMin / 60 % 24} ч"
    val clockText get() = "%02d:%02d".format((epoch / 3600) % 24, (epoch / 60) % 60)
    val callText get() = CALLS.getOrElse(call) { "" }
    val achievementsCount get() = java.lang.Long.bitCount(achievements)

    companion object {
        val FORMS = listOf("Компиляция", "Hello World", "Джун", "Мидл", "Фрилансер", "Сеньор",
            "Архитектор", "Хакер", "Легаси", "Нейросеть", "Призрак")
        val TRAITS = listOf("Обжора", "Непоседа", "Соня", "Умник", "Спокойный")
        val FOODS = listOf("кофе", "пиццу", "печеньки")  // после «любит»
        val GAMES = listOf("Отладка", "Память", "Деплой", "Пинг", "Компиляция", "Реакция")
        val CALLS = listOf("", "хочет есть", "скучает", "пора выключить свет", "болеет", "баги на полу", "", "капризничает")

        fun parse(line: String): PetState? {
            if (!line.startsWith("D|3|")) return null
            val v = line.removePrefix("D|3|").split('|').map { it.trim().toLongOrNull() ?: return null }
            if (v.size < 36) return null
            val raw = IntArray(v.size) { v[it].toInt() }
            return PetState(
                raw, v[0].toInt(), v[1] != 0L, v[2], v[3].toInt(), v[4].toInt(), v[5].toInt(), v[6].toInt(),
                v[7].toInt(), v[8].toInt(), v[9] != 0L, v[11] != 0L, v[12] != 0L,
                v[13].toInt(), v[14], v[15].toInt(), v[16] != 0L,
                v[17].toInt(), v[18].toInt(), v[19].toInt(), v[20].toInt(),
                v[21], v[22].toInt(), v[23].toInt(), v[24],
                v[28].toInt(), v[29] != 0L, v[30].toInt(), v[31].toInt(),
                v.getOrElse(44) { 0L }, v.getOrElse(45) { 0L }, v[25].toInt(),
                v.getOrElse(50) { 0L } == 1L, v.getOrElse(51) { -1L }.toInt(),
                v.getOrElse(42) { 0L }.toInt(),
            )
        }
    }
}
