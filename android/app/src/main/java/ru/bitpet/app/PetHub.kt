package ru.bitpet.app

import android.Manifest
import android.annotation.SuppressLint
import android.app.NotificationManager
import android.app.PendingIntent
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.ParcelUuid
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeoutOrNull
import android.net.Uri
import no.nordicsemi.android.dfu.DfuProgressListenerAdapter
import no.nordicsemi.android.dfu.DfuServiceInitiator
import no.nordicsemi.android.dfu.DfuServiceListenerHelper
import ru.bitpet.app.ble.BitLink
import java.io.File
import java.util.zip.ZipInputStream
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

// Всё приложение вокруг связи с платой: подключение, опрос, события, звуки, копии.
@SuppressLint("MissingPermission")
object PetHub {
    data class Found(val name: String, val address: String, val rssi: Int)
    data class LogLine(val time: Long, val text: String, val event: Boolean)
    data class Backup(val time: Long, val form: String, val code: String)

    private lateinit var ctx: Context
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    lateinit var link: BitLink
        private set

    val pet = MutableStateFlow<PetState?>(null)
    val log = MutableStateFlow<List<LogLine>>(emptyList())
    val found = MutableStateFlow<List<Found>>(emptyList())
    val scanning = MutableStateFlow(false)
    val backups = MutableStateFlow<List<Backup>>(emptyList())
    val sounds = MutableStateFlow(true)
    val syncTime = MutableStateFlow(true)
    val stepsOn = MutableStateFlow(true)        // шагомер телефона → прогулки питомца
    val place = MutableStateFlow<Weather.Place?>(null)   // город для погоды
    val weatherNow = MutableStateFlow<Weather.Now?>(null)
    private lateinit var steps: Steps
    val savedAddress = MutableStateFlow<String?>(null)
    @Volatile var uiVisible = false
    val oldFirmware = MutableStateFlow(false)  // плата не знает «data» — прошивка старше 0.14
    val firmware = MutableStateFlow<String?>(null)   // версия прошивки платы
    val nick = MutableStateFlow<String?>(null)       // ник платы (по номеру чипа, не меняется)
    // обновление по воздуху: null — не идёт; иначе текст состояния и проценты
    data class Dfu(val text: String, val percent: Int, val done: Boolean = false, val failed: Boolean = false)
    val dfu = MutableStateFlow<Dfu?>(null)

    private val requestLock = Mutex()
    private var pending: CompletableDeferred<String>? = null
    private var reconnectJob: Job? = null

    fun init(app: Context) {
        ctx = app
        link = BitLink(app, scope)
        val p = prefs()
        sounds.value = p.getBoolean("sounds", true)
        syncTime.value = p.getBoolean("syncTime", true)
        stepsOn.value = p.getBoolean("steps", true)
        p.getString("place", null)?.split('\t')?.takeIf { it.size == 3 }?.let {
            place.value = Weather.Place(it[0], it[1].toDouble(), it[2].toDouble())
        }
        scope.launch { weatherLoop() }
        steps = Steps(app)
        if (stepsOn.value) startSteps()
        savedAddress.value = p.getString("address", null)
        loadBackups()
        scope.launch { link.lines.collect { onLine(it) } }
        scope.launch {
            link.state.collect { st ->
                if (st == BitLink.State.READY) onReady()
                if (st == BitLink.State.IDLE) scheduleReconnect()
            }
        }
        scope.launch { pollLoop() }
    }

    private fun prefs() = ctx.getSharedPreferences("bit", Context.MODE_PRIVATE)

    fun hasPermissions(): Boolean = requiredPermissions().all {
        ContextCompat.checkSelfPermission(ctx, it) == PackageManager.PERMISSION_GRANTED
    }

    fun stepsPermission(): String? = if (Build.VERSION.SDK_INT >= 29) Manifest.permission.ACTIVITY_RECOGNITION else null

    fun requiredPermissions(): Array<String> = when {
        Build.VERSION.SDK_INT >= 33 -> arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.POST_NOTIFICATIONS)
        Build.VERSION.SDK_INT >= 31 -> arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else -> arrayOf(Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.ACCESS_FINE_LOCATION)
    }

    private fun adapter() = ctx.getSystemService(BluetoothManager::class.java)?.adapter

    // --- поиск и подключение ---

    private val scanCb = object : ScanCallback() {
        override fun onScanResult(type: Int, r: ScanResult) {
            val name = r.scanRecord?.deviceName ?: r.device.name ?: return
            val f = Found(name, r.device.address, r.rssi)
            found.value = (found.value.filter { it.address != f.address } + f).sortedByDescending { it.rssi }
        }
    }

    fun startScan() {
        if (!hasPermissions()) return
        val scanner = adapter()?.bluetoothLeScanner ?: return
        found.value = emptyList()
        scanning.value = true
        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(BitLink.SERVICE)).build()
        scanner.startScan(listOf(filter), ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scanCb)
        scope.launch { delay(12_000); stopScan() }
    }

    fun cancelAlarmNote() = cancel(NOTE_ALARM)

    fun stopScan() {
        if (!scanning.value) return
        runCatching { adapter()?.bluetoothLeScanner?.stopScan(scanCb) }
        scanning.value = false
    }

    fun connect(address: String) {
        if (!hasPermissions()) return
        stopScan()
        prefs().edit().putString("address", address).apply()
        savedAddress.value = address
        val dev = adapter()?.getRemoteDevice(address) ?: return
        ContextCompat.startForegroundService(ctx, Intent(ctx, PetService::class.java))
        link.connect(dev)
    }

    fun forget() {
        prefs().edit().remove("address").apply()
        savedAddress.value = null
        link.close()
        pet.value = null
        ctx.startService(Intent(ctx, PetService::class.java).setAction(PetService.ACTION_STOP))
    }

    // идёт обновление прошивки: загрузчик принимает одно подключение, и оно нужно библиотеке DFU
    private val dfuBusy get() = dfu.value?.let { !it.done && !it.failed } == true

    fun autoConnect() {
        if (dfuBusy) return  // иначе при возврате в приложение оно занимало загрузчик (ошибка 147 — время подключения истекло)
        val a = savedAddress.value ?: return
        if (link.state.value == BitLink.State.IDLE) connect(a)
    }

    private fun scheduleReconnect() {
        if (dfuBusy) return  // идёт обновление — плата занята
        reconnectJob?.cancel()
        reconnectJob = scope.launch {
            delay(4000)
            if (link.state.value == BitLink.State.IDLE && savedAddress.value != null) autoConnect()
        }
    }

    private fun onReady() {
        scope.launch {
            if (syncTime.value) {
                val now = SimpleDateFormat("dd.MM.yyyy HH:mm:ss", Locale.US).format(Date())
                request("время $now")
            }
            request("версия")?.takeIf { it.startsWith("V|") }?.let { firmware.value = it.removePrefix("V|") }
            request("ник")?.takeIf { it.startsWith("N|") }?.let { nick.value = it.removePrefix("N|") }
            weatherNow.value?.let { request("погода ${it.kind} ${it.temp}") } ?: sendWeather()
            refresh()
        }
    }

    // --- команды ---

    suspend fun request(cmd: String, timeoutMs: Long = 4000): String? = requestLock.withLock {
        if (link.state.value != BitLink.State.READY) return null
        val d = CompletableDeferred<String>()
        pending = d
        link.sendLine(cmd)
        val r = withTimeoutOrNull(timeoutMs) { d.await() }
        pending = null
        if (cmd != "data") addLog(LogLine(System.currentTimeMillis(), "> $cmd\n${r ?: "нет ответа"}", false))
        r
    }

    fun act(cmd: String, onReply: (String) -> Unit = {}) {
        Sounds.play(Sounds.Kind.TAP, sounds.value)
        scope.launch {
            val r = request(cmd)
            if (r != null) onReply(r)
            delay(300)
            request("версия")?.takeIf { it.startsWith("V|") }?.let { firmware.value = it.removePrefix("V|") }
            request("ник")?.takeIf { it.startsWith("N|") }?.let { nick.value = it.removePrefix("N|") }
            weatherNow.value?.let { request("погода ${it.kind} ${it.temp}") } ?: sendWeather()
            refresh()
        }
    }

    private var silent = 0  // сколько запросов подряд плата не ответила
    private var lastRefresh = 0L

    suspend fun refresh() {
        lastRefresh = System.currentTimeMillis()
        val r = request("data")
        if (r == null) {
            // плата не отвечает, хотя связь есть — переподключиться (со сбросом списка служб)
            if (link.state.value == BitLink.State.READY && ++silent >= 3) { silent = 0; link.close() }
            return
        }
        silent = 0
        val st = PetState.parse(r)
        oldFirmware.value = st == null && ("Не знаю" in r || "команд" in r)
        st?.let {
            if (pet.value?.call != 0 && it.call == 0) cancel(NOTE_CALL)  // просьбу выполнили — уведомление не нужно
            if (it.bugs == 0) cancel(NOTE_BUGS)                          // баги убраны
            pet.value = it
            PetRenderer.nativeApply(it.raw)
            BitWidget.update(ctx, force = false)
            History.add(ctx, it)
        }
    }

    // шаги: раз в 30 с прибавку отправляем на плату
    private var stepsJob: Job? = null
    fun startSteps() {
        if (Build.VERSION.SDK_INT >= 29 &&
            ContextCompat.checkSelfPermission(ctx, Manifest.permission.ACTIVITY_RECOGNITION) != PackageManager.PERMISSION_GRANTED) return
        steps.start()
        stepsJob?.cancel()
        stepsJob = scope.launch {
            while (isActive) {
                delay(30_000)
                if (link.state.value != BitLink.State.READY) continue
                val n = steps.takeDelta()
                if (n <= 0) continue
                val r = request("шаги $n")
                if (r == null || !r.startsWith("S|")) steps.giveBack(n)
            }
        }
    }

    fun setSteps(on: Boolean) {
        stepsOn.value = on
        prefs().edit().putBoolean("steps", on).apply()
        if (on) startSteps() else { stepsJob?.cancel(); steps.stop() }
    }

    val stepsAvailable get() = steps.available

    // --- погода: раз в полчаса и при подключении отправляем на плату ---

    suspend fun setCity(city: String): Boolean {
        val p = Weather.find(city) ?: return false
        place.value = p
        prefs().edit().putString("place", "${p.name}\t${p.lat}\t${p.lon}").apply()
        sendWeather()
        return true
    }

    fun clearCity() { place.value = null; weatherNow.value = null; prefs().edit().remove("place").apply() }

    private suspend fun sendWeather() {
        val p = place.value ?: return
        val w = Weather.current(p) ?: return
        weatherNow.value = w
        if (link.state.value == BitLink.State.READY) request("погода ${w.kind} ${w.temp}")
    }

    private suspend fun weatherLoop() {
        while (scope.isActive) {
            sendWeather()
            delay(30 * 60_000L)
        }
    }

    private suspend fun pollLoop() {
        while (scope.isActive) {
            delay(1000)  // раз в секунду: питомец на телефоне идёт туда же и делает то же, что на плате
            if (link.state.value != BitLink.State.READY) continue
            // экран открыт — каждую секунду; в фоне — раз в минуту (виджет и история)
            if (uiVisible || System.currentTimeMillis() - lastRefresh > 60_000) refresh()
        }
    }

    private fun onLine(line: String) {
        if (line.startsWith("» ")) { onEvent(line.removePrefix("» ")); return }
        pending?.complete(line)
    }

    // --- события с платы: журнал, уведомление, звук ---

    private fun onEvent(text: String) {
        addLog(LogLine(System.currentTimeMillis(), text, true))
        val t = text.lowercase()
        val kind = when {
            "будильник выключен" in t -> { Sounds.stopAlarm(); null }
            "будильник" in t -> Sounds.Kind.ALARM
            "появился баг" in t -> Sounds.Kind.CALL
            "зовёт" in t -> Sounds.Kind.CALL
            "достижение" in t -> Sounds.Kind.ACHIEVE
            "вырос" in t -> Sounds.Kind.GROW
            "погиб" in t || "ушёл" in t -> Sounds.Kind.DEATH
            "гости" in t || "друг" in t || "угостил" in t -> Sounds.Kind.FRIEND
            "квест" in t -> Sounds.Kind.QUEST
            else -> null
        }
        if (kind != null) Sounds.play(kind, sounds.value)
        if ("будильник выключен" in t) cancel(NOTE_ALARM)
        else if ("появился баг" in t) notify(text, kind, NOTE_BUGS)
        else if ("на связи" !in t) notify(text, kind)
        scope.launch { refresh() }
    }

    // по одному уведомлению на вид: новое заменяет старое; просьба снимается, когда выполнена, остальные гаснут сами
    private const val NOTE_CALL = 100
    private const val NOTE_ALARM = 101
    private const val NOTE_BUGS = 108
    private fun noteId(kind: Sounds.Kind?) = when (kind) {
        Sounds.Kind.CALL -> NOTE_CALL
        Sounds.Kind.ALARM -> NOTE_ALARM
        null -> 109
        else -> 102 + kind.ordinal
    }

    private fun notify(text: String, kind: Sounds.Kind?, id: Int = noteId(kind)) {
        if (Build.VERSION.SDK_INT >= 33 &&
            ContextCompat.checkSelfPermission(ctx, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) return
        val alarm = kind == Sounds.Kind.ALARM
        val open = PendingIntent.getActivity(ctx, 0, Intent(ctx, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE)
        val n = NotificationCompat.Builder(ctx, BitApp.CH_PET)
            .setSmallIcon(R.drawable.ic_stat)
            .setContentTitle(if (alarm) "Будильник" else (nick.value ?: "Бит"))
            .setContentText(text)
            .setContentIntent(open)
            .setAutoCancel(true)
            .setTimeoutAfter(if (kind == Sounds.Kind.CALL) 60 * 60_000L else 20 * 60_000L)
            .setCategory(if (alarm) NotificationCompat.CATEGORY_ALARM else NotificationCompat.CATEGORY_EVENT)
            .build()
        ctx.getSystemService(NotificationManager::class.java).notify(id, n)
    }

    private fun cancel(id: Int) = ctx.getSystemService(NotificationManager::class.java).cancel(id)

    private fun addLog(l: LogLine) { log.value = (listOf(l) + log.value).take(60) }

    // --- обновление прошивки по Bluetooth (Nordic DFU через загрузчик Adafruit) ---

    private val dfuListener = object : DfuProgressListenerAdapter() {
        override fun onDeviceConnecting(a: String) { dfu.value = Dfu("Подключаюсь к плате…", 0) }
        override fun onEnablingDfuMode(a: String) { dfu.value = Dfu("Перезапускаю плату в режим обновления…", 0) }
        override fun onDfuProcessStarting(a: String) { dfu.value = Dfu("Начинаю передачу…", 0) }
        override fun onProgressChanged(a: String, percent: Int, speed: Float, avg: Float, part: Int, parts: Int) {
            dfu.value = Dfu("Передаю прошивку: $percent%", percent)
        }
        override fun onFirmwareValidating(a: String) { dfu.value = Dfu("Плата проверяет прошивку…", 100) }
        override fun onDfuCompleted(a: String) {
            dfu.value = Dfu("Готово! Плата перезагружается с новой прошивкой", 100, done = true)
            firmware.value = null
            // подключаться сразу и повторять, пока плата не поднимется с новой прошивкой
            scope.launch {
                delay(2500)
                repeat(20) {
                    if (link.state.value == BitLink.State.READY) return@launch
                    if (link.state.value == BitLink.State.IDLE) autoConnect()
                    delay(2000)
                }
            }
        }
        override fun onDfuAborted(a: String) { dfu.value = Dfu("Обновление отменено", 0, failed = true); scheduleReconnect() }
        override fun onError(a: String, error: Int, type: Int, message: String?) {
            dfu.value = Dfu("Ошибка: ${message ?: error}. Плата могла остаться в режиме обновления — попробуй ещё раз или прошей по USB.", 0, failed = true)
            scope.launch { delay(4000); scheduleReconnect() }
        }
    }

    private fun checkDfuPackage(uri: Uri): String? {
        return try {
            val entries = mutableSetOf<String>()
            val stream = ctx.contentResolver.openInputStream(uri)
                ?: return "Не удалось открыть файл прошивки."
            stream.use { input ->
                ZipInputStream(input).use { archive ->
                    while (true) {
                        val entry = archive.nextEntry ?: break
                        entries.add(entry.name.substringAfterLast('/'))
                        archive.closeEntry()
                    }
                }
            }
            when {
                "manifest.json" in entries && entries.any { it.endsWith(".bin") } &&
                    entries.any { it.endsWith(".dat") } -> null
                entries.any { it.endsWith(".zip") } ->
                    "Это архив GitHub. Распакуй его и выбери firmware-ota.zip внутри."
                "firmware.uf2" in entries ->
                    "UF2 предназначен для USB. Для Bluetooth выбери firmware-ota.zip."
                else -> "Неверный формат: нужен Nordic DFU ZIP с manifest.json, .bin и .dat."
            }
        } catch (e: Exception) {
            "Ошибка чтения файла: ${e.message ?: "неизвестная"}"
        }
    }

    fun startDfu(zip: Uri) {
        val address = savedAddress.value ?: run {
            dfu.value = Dfu("Сначала подключи плату по Bluetooth.", 0, failed = true)
            return
        }
        if (!hasPermissions()) {
            dfu.value = Dfu("Нет разрешения Bluetooth.", 0, failed = true)
            return
        }
        checkDfuPackage(zip)?.let {
            dfu.value = Dfu(it, 0, failed = true)
            return
        }
        if (Build.VERSION.SDK_INT >= 26) DfuServiceInitiator.createDfuNotificationChannel(ctx)
        DfuServiceListenerHelper.registerProgressListener(ctx, dfuListener)
        reconnectJob?.cancel()
        dfu.value = Dfu("Сохраняю питомца…", 0)
        scope.launch {
            request("сохранить")      // плата пишет питомца во флеш до перезапуска
            delay(1500)
            link.close()              // библиотеке DFU нужна свободная связь
            delay(800)
            DfuServiceInitiator(address)
                .setDeviceName(link.deviceName.value)
                .setKeepBond(false)
                .setForeground(true)
                .setPacketsReceiptNotificationsEnabled(true)
                .setPacketsReceiptNotificationsValue(8)  // с 8 обновление проходило; с 20 оборвалось
                .setZip(zip)
                .start(ctx, DfuService::class.java)
        }
    }

    fun clearDfu() { dfu.value = null }

    // --- настройки ---

    fun setSounds(on: Boolean) { sounds.value = on; prefs().edit().putBoolean("sounds", on).apply(); if (!on) Sounds.stopAlarm() }
    fun setSyncTime(on: Boolean) { syncTime.value = on; prefs().edit().putBoolean("syncTime", on).apply() }

    // --- резервные копии питомца (код из команды «копия») ---

    private fun backupFile() = File(ctx.filesDir, "backups.txt")

    private fun loadBackups() {
        val f = backupFile()
        backups.value = if (!f.exists()) emptyList() else f.readLines().mapNotNull { l ->
            val p = l.split('\t')
            if (p.size == 3) Backup(p[0].toLongOrNull() ?: 0, p[1], p[2]) else null
        }
    }

    fun makeBackup(onDone: (Boolean) -> Unit) {
        scope.launch {
            val r = request("копия")
            if (r == null || !r.startsWith("BIT3 ")) { onDone(false); return@launch }
            val b = Backup(System.currentTimeMillis(), pet.value?.formName ?: "?", r.removePrefix("BIT3 ").trim())
            backups.value = (listOf(b) + backups.value).take(20)
            backupFile().writeText(backups.value.joinToString("\n") { "${it.time}\t${it.form}\t${it.code}" })
            onDone(true)
        }
    }

    fun restore(b: Backup, onReply: (String) -> Unit) = act("восстановить ${b.code}", onReply)

    fun deleteBackup(b: Backup) {
        backups.value = backups.value - b
        backupFile().writeText(backups.value.joinToString("\n") { "${it.time}\t${it.form}\t${it.code}" })
    }
}
