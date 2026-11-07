package ru.bitpet.app.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.content.Context
import android.os.Build
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import java.util.UUID

// «UART по Bluetooth» (Nordic UART Service) — как на плате (firmware/src/bt_link.h).
// Строки текста в обе стороны; длинные команды режутся по размеру пакета.
@SuppressLint("MissingPermission")  // разрешения проверяет PetHub перед вызовами
class BitLink(private val ctx: Context, private val scope: CoroutineScope) {
    enum class State { IDLE, CONNECTING, READY }

    val state = MutableStateFlow(State.IDLE)
    // Don't silently discard tiles/events when Android's main thread is busy.
    // Only PetHub collects this stream. Nordic UART notifications are
    // lossless in order, but the former 64-item SharedFlow dropped bursts.
    private val incoming = Channel<String>(Channel.UNLIMITED)
    val lines = incoming.receiveAsFlow()
    val deviceName = MutableStateFlow("")

    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private var mtu = 23
    private val inbox = java.io.ByteArrayOutputStream()  // байты до перевода строки: русская буква может прийти в двух пакетах
    private val outbox = Channel<ByteArray>(Channel.UNLIMITED)
    private var writeDone: CompletableDeferred<Boolean>? = null

    init {
        scope.launch {
            for (chunk in outbox) {
                val c = rx ?: continue
                val g = gatt ?: continue
                val done = CompletableDeferred<Boolean>()
                writeDone = done
                val ok = if (Build.VERSION.SDK_INT >= 33) {
                    g.writeCharacteristic(c, chunk, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == android.bluetooth.BluetoothStatusCodes.SUCCESS
                } else {
                    @Suppress("DEPRECATION")
                    run { c.value = chunk; c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT; g.writeCharacteristic(c) }
                }
                if (ok) withTimeoutOrNull(2000) { done.await() }
            }
        }
    }

    fun connect(device: BluetoothDevice) {
        close()
        state.value = State.CONNECTING
        deviceName.value = device.name ?: device.address
        gatt = device.connectGatt(ctx, false, callback, BluetoothDevice.TRANSPORT_LE)
    }

    fun close() {
        gatt?.let { it.disconnect(); it.close() }
        gatt = null
        rx = null
        state.value = State.IDLE
    }

    fun sendLine(text: String) {
        val bytes = (text + "\n").toByteArray(Charsets.UTF_8)
        val size = (mtu - 3).coerceAtLeast(20)
        var i = 0
        while (i < bytes.size) {
            outbox.trySend(bytes.copyOfRange(i, minOf(bytes.size, i + size)))
            i += size
        }
    }

    private fun onBytes(b: ByteArray) {
        synchronized(inbox) {
            for (x in b) {
                if (x == '\n'.code.toByte()) {
                    val line = String(inbox.toByteArray(), Charsets.UTF_8).trim('\r', ' ')
                    inbox.reset()
                    if (line.isNotEmpty()) incoming.trySend(line)
                } else if (inbox.size() < 4096) {
                    inbox.write(x.toInt())
                }
            }
        }
    }

    private fun refreshCache(g: BluetoothGatt): Boolean =
        runCatching { g.javaClass.getMethod("refresh").invoke(g) as Boolean }.getOrDefault(false)

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                // Android запоминает список служб устройства. После перепрошивки (в 0.17 добавилась служба
                // обновления) номера служб сдвинулись, а телефон писал по старым: команды не доходили.
                // Поэтому каждый раз сбрасываем запомненное и читаем службы заново.
                refreshCache(g)
                android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({ g.requestMtu(247) }, 600)
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                g.close()
                if (gatt == g) { gatt = null; rx = null; state.value = State.IDLE }
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, value: Int, status: Int) {
            mtu = value
            g.discoverServices()
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val svc = g.getService(SERVICE) ?: run { g.disconnect(); return }
            rx = svc.getCharacteristic(RX)
            val tx = svc.getCharacteristic(TX) ?: return
            g.setCharacteristicNotification(tx, true)
            val d = tx.getDescriptor(CCCD) ?: return
            if (Build.VERSION.SDK_INT >= 33) {
                g.writeDescriptor(d, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            } else {
                @Suppress("DEPRECATION")
                run { d.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE; g.writeDescriptor(d) }
            }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {
            state.value = State.READY
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
            writeDone?.complete(status == BluetoothGatt.GATT_SUCCESS)
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) {
            if (c.uuid == TX) onBytes(value)
        }

        @Deprecated("до Android 13")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT < 33 && c.uuid == TX) onBytes(c.value ?: return)
        }
    }

    companion object {
        val SERVICE: UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
        val RX: UUID = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")  // телефон -> плата
        val TX: UUID = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")  // плата -> телефон
        val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }
}
