package ru.bitpet.app

import android.app.Activity
import no.nordicsemi.android.dfu.DfuBaseService

// Служба библиотеки Nordic DFU: передаёт прошивку на плату по Bluetooth.
class DfuService : DfuBaseService() {
    override fun getNotificationTarget(): Class<out Activity> = MainActivity::class.java
    override fun isDebug(): Boolean = false
}
