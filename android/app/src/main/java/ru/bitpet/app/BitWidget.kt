package ru.bitpet.app

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.widget.RemoteViews

// Виджет: экран платы (рисует то же ядро) и что просит питомец.
class BitWidget : AppWidgetProvider() {
    override fun onUpdate(ctx: Context, mgr: AppWidgetManager, ids: IntArray) = update(ctx)

    companion object {
        private var lastUpdate = 0L
        private var lastText = ""

        fun update(ctx: Context, force: Boolean = true) {
            val mgr = AppWidgetManager.getInstance(ctx)
            val ids = mgr.getAppWidgetIds(ComponentName(ctx, BitWidget::class.java))
            if (ids.isEmpty()) return
            val p = PetHub.pet.value
            val text = when {
                p == null -> "${PetHub.nick.value ?: "Бит"} · нет связи"
                p.call != 0 && p.alive -> "${p.formName} зовёт: ${p.callText}"
                p.asleep -> "${p.formName} спит · ${p.clockText}"
                else -> "${p.formName} · сытость ${p.hunger}% · настроение ${p.happy}%"
            }
            val now = System.currentTimeMillis()
            if (!force && text == lastText && now - lastUpdate < 60_000) return  // не чаще раза в минуту
            lastUpdate = now
            lastText = text
            val px = IntArray(PetRenderer.W * PetRenderer.H)
            PetRenderer.nativeFrame(0, px)
            val small = Bitmap.createBitmap(px, PetRenderer.W, PetRenderer.H, Bitmap.Config.ARGB_8888)
            val big = Bitmap.createScaledBitmap(small, PetRenderer.W * 3, PetRenderer.H * 3, false)  // чёткие пиксели
            val open = PendingIntent.getActivity(ctx, 0, Intent(ctx, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE)
            val v = RemoteViews(ctx.packageName, R.layout.widget_bit).apply {
                setImageViewBitmap(R.id.widget_screen, big)
                setTextViewText(R.id.widget_text, text)
                setOnClickPendingIntent(R.id.widget_root, open)
            }
            mgr.updateAppWidget(ids, v)
        }
    }
}
