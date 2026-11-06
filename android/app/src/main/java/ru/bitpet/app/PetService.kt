package ru.bitpet.app

import android.app.Notification
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat

// Держит процесс живым, пока есть связь с платой: так уведомления и звуки приходят и в фоне.
class PetService : Service() {
    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) { stopSelf(); return START_NOT_STICKY }
        val n = notification(this)
        if (Build.VERSION.SDK_INT >= 29) startForeground(1, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
        else startForeground(1, n)
        return START_STICKY
    }

    companion object {
        const val ACTION_STOP = "stop"

        fun notification(s: Service): Notification {
            val open = PendingIntent.getActivity(s, 0, Intent(s, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE)
            return NotificationCompat.Builder(s, BitApp.CH_LINK)
                .setSmallIcon(R.drawable.ic_stat)
                .setContentTitle("Бит на связи")
                .setContentText("Уведомления и звуки от питомца")
                .setContentIntent(open)
                .setOngoing(true)
                .setSilent(true)
                .setShowWhen(false)
                .setPriority(NotificationCompat.PRIORITY_MIN)  // без значка в строке состояния
                .setForegroundServiceBehavior(NotificationCompat.FOREGROUND_SERVICE_DEFERRED)
                .build()
        }
    }
}
