package ru.bitpet.app

// Мост к C++: получает состояние питомца с T114, но не дублирует экран часов/погоды.
object PetRenderer {
    const val W = 240
    const val H = 135

    init { System.loadLibrary("bitcore") }

    external fun nativeApply(fields: IntArray)
    external fun nativeFrame(dtMs: Int, out: IntArray)
    external fun nativePortrait(form: Int, bg: Int, size: Int, out: IntArray)
}
