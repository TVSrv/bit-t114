package ru.bitpet.app

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager

class BitApp : Application() {
    override fun onCreate() {
        super.onCreate()
        val nm = getSystemService(NotificationManager::class.java)
        nm.deleteNotificationChannel("link")
        nm.createNotificationChannel(NotificationChannel(CH_LINK, "Связь с платой", NotificationManager.IMPORTANCE_MIN).apply {
            description = "Постоянное уведомление, пока приложение держит связь по Bluetooth (без значка в строке состояния)"
            setShowBadge(false)
            setSound(null, null)
            enableVibration(false)
        })
        nm.createNotificationChannel(NotificationChannel(CH_PET, "Питомец зовёт", NotificationManager.IMPORTANCE_HIGH).apply {
            description = "Просьбы, рост, достижения, гости и будильник"
            setSound(null, null)  // звучит своя 8-битная мелодия
        })
        PetHub.init(this)
    }

    companion object {
        const val CH_LINK = "link2"
        const val CH_PET = "pet"
    }
}
