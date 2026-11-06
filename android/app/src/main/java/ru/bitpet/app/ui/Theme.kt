package ru.bitpet.app.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// Палитра как на экране платы: тёмно-синяя тема терминала.
object Pal {
    val bg = Color(0xFF0B1322)
    val panel = Color(0xFF101D30)
    val panel2 = Color(0xFF18283F)
    val line = Color(0xFF304560)
    val text = Color(0xFFEFF6FF)
    val muted = Color(0xFFA8B9D0)
    val accent = Color(0xFF78E5D2)
    val orange = Color(0xFFF5AD73)
    val pink = Color(0xFFFF8FAB)
    val yellow = Color(0xFFF5D482)
    val green = Color(0xFFA5DB86)
    val red = Color(0xFFFF7887)
    val purple = Color(0xFFC678DD)
    val cyan = Color(0xFF83BAFF)
    val bezel = Color(0xFF080D17)
}

@Composable
fun BitTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = Pal.accent, onPrimary = Color(0xFF0B1A2A),
            background = Pal.bg, surface = Pal.panel, onSurface = Pal.text, onBackground = Pal.text,
            surfaceVariant = Pal.panel2, onSurfaceVariant = Pal.muted, outline = Pal.line,
        ),
        content = content,
    )
}
