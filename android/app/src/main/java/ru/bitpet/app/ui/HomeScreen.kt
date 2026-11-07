package ru.bitpet.app.ui

import android.graphics.Bitmap
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.Send
import androidx.compose.material.icons.rounded.Bluetooth
import androidx.compose.material.icons.rounded.BluetoothSearching
import androidx.compose.material.icons.rounded.CleaningServices
import androidx.compose.material.icons.rounded.Cookie
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Healing
import androidx.compose.material.icons.rounded.LocalCafe
import androidx.compose.material.icons.rounded.LocalPizza
import androidx.compose.material.icons.rounded.NightsStay
import androidx.compose.material.icons.rounded.RateReview
import androidx.compose.material.icons.rounded.Restore
import androidx.compose.material.icons.rounded.Save
import androidx.compose.material.icons.rounded.Schedule
import androidx.compose.material.icons.rounded.WbSunny
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import ru.bitpet.app.PetHub
import ru.bitpet.app.PetRenderer
import ru.bitpet.app.PetState
import ru.bitpet.app.Sounds
import ru.bitpet.app.Weather
import ru.bitpet.app.ble.BitLink
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HomeScreen(onNeedPermissions: () -> Unit) {
    val pet by PetHub.pet.collectAsState()
    val link by PetHub.link.state.collectAsState()
    val name by PetHub.link.deviceName.collectAsState()
    val saved by PetHub.savedAddress.collectAsState()
    val snack = remember { SnackbarHostState() }
    val scope = rememberCoroutineScope()
    val scroll = rememberLazyListState()
    var picker by remember { mutableStateOf(false) }
    var selectedTab by remember { mutableIntStateOf(0) }
    // Stable mode: render from the same native C++ game engine locally at 20 FPS.
// The experimental v2 BLE stream could starve state commands for several
// seconds and then freeze between heavy framebuffer transactions. Leave it
// disabled until a throughput-safe streaming protocol is proven on hardware.
LaunchedEffect(selectedTab) { PetHub.mirrorWanted = false }
    val say: (String) -> Unit = { msg -> scope.launch { snack.currentSnackbarData?.dismiss(); snack.showSnackbar(msg) } }

    Box(Modifier.fillMaxSize().background(Pal.bg).windowInsetsPadding(WindowInsets.safeDrawing).imePadding()) {
        Column(Modifier.fillMaxSize()) {
            Box(Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp)) {
                AppTabs(selectedTab) { tab ->
                    selectedTab = tab
                    scope.launch { scroll.scrollToItem(0) }
                }
            }
            LazyColumn(
                Modifier.fillMaxWidth().weight(1f),
                state = scroll,
                contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 16.dp, vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(14.dp),
            ) {
            item { Header(link, name, saved != null) { if (PetHub.hasPermissions()) picker = true else onNeedPermissions() } }
            pet?.let { p -> if (p.call != 0 && p.alive) item { CallBanner(p, say) } }
            if (Sounds.alarmRinging) item { AlarmStop() }
            when (selectedTab) {
                0 -> {
                    item { DeviceWindow(link) }
                    item { FirmwareHint() }
                    pet?.let { p ->
                        item { Vitals(p) }
                        item { Actions(p, link == BitLink.State.READY, say) }
                        item { Facts(p) }
                    }
                    if (pet == null) item { Text("Подключи Бита, чтобы увидеть его состояние и управлять им.",
                        color = Pal.muted, fontSize = 14.sp, modifier = Modifier.padding(8.dp)) }
                }
                1 -> {
                    pet?.let { p -> item { Evolution(p) } }
                    item { HistoryCard() }
                    pet?.let { p -> item { Album(p.formsSeen) } }
                    item { Backups(link == BitLink.State.READY, say) }
                }
                else -> {
                    item { FirmwareUpdate(saved != null) }
                    item { Settings(saved != null) }
                    item { Terminal(link == BitLink.State.READY) }
                    item { Journal() }
                }
            }
            item { Spacer(Modifier.height(24.dp)) }
            }
        }
        SnackbarHost(snack, Modifier.align(Alignment.BottomCenter).padding(16.dp))
    }

    if (picker) {
        LaunchedEffect(Unit) { PetHub.startScan() }
        ModalBottomSheet(onDismissRequest = { PetHub.stopScan(); picker = false }, containerColor = Pal.panel) {
            DevicePicker { PetHub.connect(it); picker = false }
        }
    }
}

// Основные разделы без длинной прокрутки через несвязанные функции.
@Composable
private fun AppTabs(selected: Int, onSelect: (Int) -> Unit) {
    val labels = listOf("Питомец", "Развитие", "Устройство")
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(7.dp)) {
        labels.forEachIndexed { index, label ->
            val active = index == selected
            Box(
                Modifier.weight(1f).clip(RoundedCornerShape(14.dp))
                    .background(if (active) Pal.accent.copy(alpha = 0.22f) else Pal.panel)
                    .border(1.dp, if (active) Pal.accent else Pal.line, RoundedCornerShape(14.dp))
                    .clickable { onSelect(index) }.padding(vertical = 13.dp),
                contentAlignment = Alignment.Center,
            ) {
                Text(label, color = if (active) Pal.text else Pal.muted, fontSize = 13.sp,
                    fontWeight = if (active) FontWeight.Bold else FontWeight.Medium)
            }
        }
    }
}

// --- шапка ---

@Composable
private fun Header(link: BitLink.State, name: String, known: Boolean, onFind: () -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            val nick by PetHub.nick.collectAsState()
            Text(nick ?: "Бит", color = Pal.text, fontSize = if (nick == null) 30.sp else 22.sp, fontWeight = FontWeight.Black,
                letterSpacing = (-0.3).sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
            val (dot, txt) = when (link) {
                BitLink.State.READY -> Pal.green to "на связи · $name"
                BitLink.State.CONNECTING -> Pal.yellow to "подключаюсь…"
                BitLink.State.IDLE -> Pal.muted to if (known) "нет связи — ищу плату" else "плата не выбрана"
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.size(8.dp).clip(CircleShape).background(dot))
                Spacer(Modifier.width(6.dp))
                Text(txt, color = Pal.muted, fontSize = 13.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
        }
        PillButton(if (known) Icons.Rounded.Bluetooth else Icons.Rounded.BluetoothSearching, if (known) "Сменить" else "Найти плату", onFind)
    }
}

@Composable
private fun PillButton(icon: ImageVector, label: String, onClick: () -> Unit) {
    Row(
        Modifier.heightIn(min = 48.dp).clip(RoundedCornerShape(50)).background(Pal.panel2)
            .border(1.dp, Pal.line, RoundedCornerShape(50)).clickable(onClick = onClick)
            .padding(horizontal = 14.dp, vertical = 9.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(icon, null, tint = Pal.accent, modifier = Modifier.size(18.dp))
        Spacer(Modifier.width(6.dp))
        Text(label, color = Pal.text, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
    }
}

// --- окно питомца: часы и погода показываются только на дисплее T114 ---

@Composable
private fun DeviceWindow(link: BitLink.State) {
    val firmwarePixels by PetHub.mirrorFrame.collectAsState()
    val latestFirmwarePixels by rememberUpdatedState(firmwarePixels)
    val bmp = remember { Bitmap.createBitmap(PetRenderer.W, PetRenderer.H, Bitmap.Config.ARGB_8888) }
    val px = remember { IntArray(PetRenderer.W * PetRenderer.H) }
    var frame by remember { mutableIntStateOf(0) }
    LaunchedEffect(Unit) {
        var last = 0L
        while (true) {
            // 20 FPS вместо непрерывной отрисовки под 60/120 Гц дисплей.
            // Эффект отменяется автоматически при переходе на другую вкладку.
            delay(50L)
            withFrameNanos { t ->
                val dt = if (last == 0L) 50 else ((t - last) / 1_000_000).toInt().coerceIn(1, 250)
                last = t
                val exact = latestFirmwarePixels
                if (exact != null) {
                    // Pixel-for-pixel firmware render; native C++ remains fallback
                    // while a full BLE snapshot is being received or offline.
                    bmp.setPixels(exact, 0, PetRenderer.W, 0, 0, PetRenderer.W, PetRenderer.H)
                } else {
                    PetRenderer.nativeFrame(dt, px)
                    bmp.setPixels(px, 0, PetRenderer.W, 0, 0, PetRenderer.W, PetRenderer.H)
                }
                frame++
            }
        }
    }
    val image = remember { bmp.asImageBitmap() }
    val dim by animateFloatAsState(if (link == BitLink.State.READY) 0f else 0.55f, tween(400), label = "dim")
    Column(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(22.dp))
            .background(Brush.verticalGradient(listOf(Pal.panel2, Pal.panel)))
            .border(1.dp, Pal.line, RoundedCornerShape(22.dp))
            .padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            val ready = link == BitLink.State.READY
            Box(Modifier.size(7.dp).clip(CircleShape).background(if (ready) Pal.green else Pal.muted))
            Spacer(Modifier.width(7.dp))
            Text("Питомец", color = Pal.text, fontSize = 13.sp, fontWeight = FontWeight.SemiBold,
                modifier = Modifier.weight(1f))
            Text("T114", color = Pal.muted, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
        }
        Box(Modifier.fillMaxWidth().clip(RoundedCornerShape(10.dp)).background(Pal.bezel).padding(7.dp)) {
            Canvas(Modifier.fillMaxWidth().aspectRatio(240f / 135f).clip(RoundedCornerShape(4.dp))) {
                frame  // перерисовка на каждом кадре
                drawImage(image, srcOffset = IntOffset.Zero, srcSize = IntSize(PetRenderer.W, PetRenderer.H),
                    dstSize = IntSize(size.width.toInt(), size.height.toInt()), filterQuality = FilterQuality.None)
                if (dim > 0f) drawRect(Color.Black.copy(alpha = dim))
            }
            if (link != BitLink.State.READY) {
                Text(if (link == BitLink.State.CONNECTING) "подключаюсь…" else "нет связи с платой",
                    color = Pal.text, fontSize = 15.sp, fontWeight = FontWeight.SemiBold, modifier = Modifier.align(Alignment.Center))
            }
        }
    }
}

@Composable
private fun FirmwareHint() {
    val old by PetHub.oldFirmware.collectAsState()
    if (!old) return
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(18.dp)).background(Pal.yellow.copy(alpha = 0.14f))
        .border(1.dp, Pal.yellow.copy(alpha = 0.5f), RoundedCornerShape(18.dp)).padding(14.dp)) {
        Text("Обнови прошивку платы", color = Pal.yellow, fontSize = 16.sp, fontWeight = FontWeight.Bold)
        Text("Приложению нужна прошивка 0.14 или новее: старая не присылает состояние питомца. " +
            "Файл bit-t114-0.14.uf2 — дважды RESET и скопировать на диск HT-n5262.", color = Pal.text, fontSize = 13.sp)
    }
}

// --- просьба питомца с кнопкой «сделать» ---

@Composable
private fun CallBanner(p: PetState, say: (String) -> Unit) {
    val (color, fix, cmd) = when (p.call) {
        1 -> Triple(Pal.orange, "Дать пиццу", "еда пицца")
        2 -> Triple(Pal.accent, "Поиграть на плате", null)
        3 -> Triple(Pal.yellow, "Выключить свет", "свет")
        4 -> Triple(Pal.red, "Отладчик", "лечить")
        5 -> Triple(Pal.purple, "Убрать баги", "уборка")
        else -> Triple(Pal.cyan, "Код-ревью", "ревью")
    }
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(18.dp)).background(color.copy(alpha = 0.16f))
            .border(1.dp, color.copy(alpha = 0.5f), RoundedCornerShape(18.dp)).padding(14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text("Зовёт", color = color, fontSize = 12.sp, fontWeight = FontWeight.Bold, letterSpacing = 1.sp)
            Text(p.callText.replaceFirstChar { it.uppercase() }, color = Pal.text, fontSize = 18.sp, fontWeight = FontWeight.SemiBold)
        }
        if (cmd != null) {
            Text(fix, color = Color(0xFF111318), fontSize = 14.sp, fontWeight = FontWeight.Bold,
                modifier = Modifier.clip(RoundedCornerShape(50)).background(color).clickable { PetHub.act(cmd) { say(it) } }
                    .padding(horizontal = 14.dp, vertical = 9.dp))
        } else {
            Text(fix, color = Pal.muted, fontSize = 13.sp)
        }
    }
}

// --- шкалы ---

@Composable
private fun Vitals(p: PetState) {
    Card(title = "Самочувствие") {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                VitalBar("Сытость", p.hunger, Pal.orange, Modifier.weight(1f))
                VitalBar("Настроение", p.happy, Pal.pink, Modifier.weight(1f))
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                VitalBar("Энергия", p.energy, Pal.yellow, Modifier.weight(1f))
                VitalBar("Дисциплина", p.discipline, Pal.cyan, Modifier.weight(1f))
            }
        }
    }
}

@Composable
private fun VitalBar(label: String, v: Int, color: Color, modifier: Modifier) {
    val value = v.coerceIn(0, 100)
    val k by animateFloatAsState(value / 100f, tween(600), label = label)
    val c = if (value < 25) Pal.red else color
    Column(modifier.clip(RoundedCornerShape(12.dp)).background(Pal.panel2).padding(10.dp)) {
        Text(label, color = Pal.muted, fontSize = 12.sp)
        Text("$value%", color = c, fontSize = 18.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
        Spacer(Modifier.height(5.dp))
        Canvas(Modifier.fillMaxWidth().height(5.dp)) {
            val r = androidx.compose.ui.geometry.CornerRadius(size.height / 2)
            drawRoundRect(Pal.bezel, cornerRadius = r)
            if (k > 0f) drawRoundRect(c, size = size.copy(width = size.width * k), cornerRadius = r)
        }
    }
}

// --- факты ---

@Composable
private fun Facts(p: PetState) {
    val chips = buildList {
        add(p.formName to Pal.text)
        add("возраст ${p.ageText}" to Pal.muted)
        add("вес ${p.weight} КБ" to Pal.muted)
        add(("часы ${p.clockText}" + if (!p.clockSure) " ?" else "") to if (p.clockSure) Pal.muted else Pal.yellow)
        add(("батарея ${p.battery}%" + if (p.charging) ", заряжается" else "") to Pal.muted)
        if (p.bugs > 0) add("багов ${p.bugs}" to Pal.purple)
        if (p.sick) add("болеет" to Pal.red)
        if (p.asleep) add("спит" to Pal.cyan)
        add("${PetState.TRAITS.getOrElse(p.trait) { "" }}, любит ${PetState.FOODS.getOrElse(p.favFood) { "" }}" to Pal.pink)
        add("монет ${p.coins}" to Pal.yellow)
        add("шагов сегодня ${p.stepsToday}" to Pal.green)
        add("достижений ${p.achievementsCount}" to Pal.yellow)
        add("поколение ${p.generation}" to Pal.muted)
        if (p.raw.size >= 54 && p.boardScene == 0) add((if (p.menuOpen) "меню: ${listOf("Еда", "Игры", "Свет", "Уборка", "Отладчик", "Статус", "Обучение", "Прогулка", "Магазин", "Настройки").getOrElse(p.menuCursor) { "открыто" }}" else "меню скрыто") to Pal.cyan)
    }
    FlowChips(chips)
}

@OptIn(androidx.compose.foundation.layout.ExperimentalLayoutApi::class)
@Composable
private fun FlowChips(chips: List<Pair<String, Color>>) {
    androidx.compose.foundation.layout.FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        chips.forEach { (t, c) ->
            Text(t, color = c, fontSize = 13.sp, modifier = Modifier.clip(RoundedCornerShape(50)).background(Pal.panel).padding(horizontal = 12.dp, vertical = 7.dp))
        }
    }
}

// --- действия ---

private data class Act(val icon: ImageVector, val label: String, val color: Color, val cmd: String)

@Composable
private fun Actions(p: PetState, ready: Boolean, say: (String) -> Unit) {
    val fw by PetHub.firmware.collectAsState()
    val minor = fw?.substringAfter('.', "")?.substringBefore('.')?.substringBefore('-')?.toIntOrNull() ?: 0
    val supportsTraining = minor >= 36
    val acts = listOf(
        Act(Icons.Rounded.LocalCafe, "Кофе", Pal.orange, "еда кофе"),
        Act(Icons.Rounded.LocalPizza, "Пицца", Pal.yellow, "еда пицца"),
        Act(Icons.Rounded.Cookie, "Печенька", Pal.pink, "еда печенька"),
        Act(if (p.lightOff) Icons.Rounded.WbSunny else Icons.Rounded.NightsStay, if (p.lightOff) "Свет" else "Спать", Pal.yellow, "свет"),
        Act(Icons.Rounded.CleaningServices, "Уборка", Pal.purple, "уборка"),
        Act(Icons.Rounded.Healing, "Отладчик", Pal.red, "лечить"),
        Act(Icons.Rounded.RateReview, if (supportsTraining) "Обучение" else "Ревью", Pal.accent, "ревью"),
        Act(Icons.Rounded.Schedule, "Время", Pal.green, ""),
    )
    Card(title = "Действия") {
        acts.chunked(4).forEach { row ->
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                row.forEach { a ->
                    Column(
                        Modifier.weight(1f).clip(RoundedCornerShape(16.dp)).background(Pal.panel2)
                            .clickable(enabled = ready) {
                                if (a.cmd.isEmpty()) {
                                    val now = SimpleDateFormat("dd.MM.yyyy HH:mm:ss", Locale.US).format(Date())
                                    PetHub.act("время $now") { say(it) }
                                } else PetHub.act(a.cmd) { reply ->
                                    say(if (a.cmd == "ревью" && supportsTraining && reply == "Готово")
                                        "Тренировка открыта на плате: коротко — 0, долго — 1" else reply)
                                }
                            }.padding(vertical = 12.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                    ) {
                        Box(Modifier.size(38.dp).clip(CircleShape).background(a.color.copy(alpha = if (ready) 0.18f else 0.06f)), contentAlignment = Alignment.Center) {
                            Icon(a.icon, null, tint = if (ready) a.color else Pal.muted, modifier = Modifier.size(22.dp))
                        }
                        Spacer(Modifier.height(6.dp))
                        Text(a.label, color = if (ready) Pal.text else Pal.muted, fontSize = 12.sp, maxLines = 1)
                    }
                }
            }
            Spacer(Modifier.height(10.dp))
        }
        Text(if (supportsTraining) "Обучение проводится кнопкой T114. Игры и прогулка — на самой плате."
             else "Игры, прогулка и настройки — на самой плате.",
             color = Pal.muted, fontSize = 12.sp)
    }
}

@Composable
private fun AlarmStop() {
    Text("Выключить звонок будильника", color = Color(0xFF111318), fontWeight = FontWeight.Bold, fontSize = 15.sp,
        modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp)).background(Pal.yellow).clickable { Sounds.stopAlarm() }.padding(16.dp))
}

// --- история шкал ---

// Длительности эволюции отражают активную длинную ветку 0.37+.
// Точный остаток времени недоступен без отдельной передачи stageMin по BLE.
@Composable
private fun Evolution(p: PetState) {
    val fw by PetHub.firmware.collectAsState()
    val minor = fw?.substringAfter('.', "")?.substringBefore('.')?.substringBefore('-')?.toIntOrNull() ?: 0
    val longEvolution = minor >= 37
    val (next, duration) = if (longEvolution) when (p.form) {
        0 -> "Малыш" to "10 минут"
        1 -> "Джун" to "12 часов"
        2 -> "Мидл или Фрилансер" to "7 дней"
        3, 4 -> "Взрослая форма" to "10 дней"
        6, 7 -> "Нейросеть (при соблюдении условий)" to "14 дней"
        else -> "Финальная форма" to "—"
    } else "Следующая стадия" to "Зависит от версии прошивки"
    Card(title = "Развитие питомца") {
        Text(p.formName, color = Pal.text, fontSize = 23.sp, fontWeight = FontWeight.Bold)
        Spacer(Modifier.height(5.dp))
        Text("Следующая стадия: $next", color = Pal.text, fontSize = 14.sp)
        Text("Продолжительность стадии: $duration", color = Pal.muted, fontSize = 13.sp)
        Spacer(Modifier.height(8.dp))
        Text("Дисциплина: ${p.discipline}/100 · ошибки ухода: ${p.mistakes}",
            color = if (p.discipline >= 75) Pal.green else Pal.cyan, fontSize = 13.sp)
        if (longEvolution) {
            Spacer(Modifier.height(8.dp))
            Text("Для Архитектора нужна дисциплина от 75, для Нейросети — 100. " +
                "Точный отсчёт до следующей формы плата пока не передаёт.",
                color = Pal.muted, fontSize = 12.sp)
        }
    }
}

@Composable
private fun HistoryCard() {
    val ctx = androidx.compose.ui.platform.LocalContext.current
    LaunchedEffect(Unit) { ru.bitpet.app.History.load(ctx) }
    val all by ru.bitpet.app.History.points.collectAsState()
    var week by remember { mutableStateOf(false) }
    val span = if (week) 7 * 24 * 3600_000L else 24 * 3600_000L
    val now = System.currentTimeMillis()
    val pts = all.filter { it.t > now - span }
    Card(title = "История") {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf(false to "Сутки", true to "Неделя").forEach { (w, l) ->
                Text(l, color = if (week == w) Color(0xFF111318) else Pal.muted, fontSize = 13.sp, fontWeight = FontWeight.SemiBold,
                    modifier = Modifier.clip(RoundedCornerShape(50)).background(if (week == w) Pal.accent else Pal.panel2)
                        .clickable { week = w }.padding(horizontal = 12.dp, vertical = 6.dp))
            }
        }
        Spacer(Modifier.height(10.dp))
        if (pts.size < 2) {
            Text("Пока мало данных: приложение записывает шкалы раз в 5 минут, пока есть связь с платой.", color = Pal.muted, fontSize = 13.sp)
        } else {
            val series = listOf(Pal.orange to { p: ru.bitpet.app.History.Point -> p.hunger }, Pal.pink to { p: ru.bitpet.app.History.Point -> p.happy },
                Pal.yellow to { p: ru.bitpet.app.History.Point -> p.energy }, Pal.cyan to { p: ru.bitpet.app.History.Point -> p.discipline })
            Canvas(Modifier.fillMaxWidth().height(140.dp)) {
                val t0 = now - span
                for (k in 0..4) {  // сетка 0/25/50/75/100
                    val y = size.height * (1 - k / 4f)
                    drawLine(Pal.line, Offset(0f, y), Offset(size.width, y), strokeWidth = 1f)
                }
                for ((col, f) in series) {
                    val path = androidx.compose.ui.graphics.Path()
                    pts.forEachIndexed { i, p ->
                        val x = ((p.t - t0).toFloat() / span) * size.width
                        val y = size.height * (1 - f(p) / 100f)
                        if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
                    }
                    drawPath(path, col, style = androidx.compose.ui.graphics.drawscope.Stroke(width = 4f))
                }
            }
            Spacer(Modifier.height(6.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                listOf(Pal.orange to "сытость", Pal.pink to "настроение", Pal.yellow to "энергия", Pal.cyan to "дисциплина").forEach { (c, l) ->
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Box(Modifier.size(8.dp).clip(CircleShape).background(c)); Spacer(Modifier.width(4.dp))
                        Text(l, color = Pal.muted, fontSize = 11.sp)
                    }
                }
            }
        }
    }
}

// --- альбом форм: портреты рисует то же ядро ---

@Composable
private fun Album(seen: Int) {
    val forms = listOf(1, 2, 3, 4, 5, 6, 7, 8, 9)
    val n = forms.count { (seen shr it) and 1 == 1 }
    Card(title = "Альбом форм · $n из 9") {
        forms.chunked(5).forEach { row ->
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                row.forEach { f ->
                    val got = (seen shr f) and 1 == 1
                    Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
                        Box(Modifier.fillMaxWidth().aspectRatio(1f).clip(RoundedCornerShape(12.dp)).background(Pal.panel2), contentAlignment = Alignment.Center) {
                            if (got) {
                                val img = remember(f) {
                                    val px = IntArray(64 * 64)
                                    PetRenderer.nativePortrait(f, 0x262B36, 64, px)
                                    Bitmap.createBitmap(px, 64, 64, Bitmap.Config.ARGB_8888).asImageBitmap()
                                }
                                Canvas(Modifier.fillMaxSize()) {
                                    drawImage(img, srcOffset = IntOffset.Zero, srcSize = IntSize(64, 64),
                                        dstSize = IntSize(size.width.toInt(), size.height.toInt()), filterQuality = FilterQuality.None)
                                }
                            } else Text("?", color = Pal.muted, fontSize = 22.sp, fontWeight = FontWeight.Bold)
                        }
                        Text(if (got) PetState.FORMS[f] else "—", color = if (got) Pal.text else Pal.muted, fontSize = 10.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    }
                }
                repeat(5 - row.size) { Spacer(Modifier.weight(1f)) }
            }
            Spacer(Modifier.height(8.dp))
        }
    }
}

// --- копии ---

@Composable
private fun Backups(ready: Boolean, say: (String) -> Unit) {
    val list by PetHub.backups.collectAsState()
    var confirm by remember { mutableStateOf<PetHub.Backup?>(null) }
    val fmt = remember { SimpleDateFormat("d MMM, HH:mm", Locale("ru")) }
    Card(title = "Копии питомца") {
        Text("Копия хранится в телефоне. Из неё можно вернуть питомца на плату, если он погиб или плату перепрошили.",
            color = Pal.muted, fontSize = 13.sp)
        Spacer(Modifier.height(10.dp))
        PillButton(Icons.Rounded.Save, "Сохранить копию сейчас") {
            if (!ready) say("Нет связи с платой") else PetHub.makeBackup { ok -> say(if (ok) "Копия сохранена" else "Не получилось сохранить") }
        }
        list.forEach { b ->
            Row(Modifier.fillMaxWidth().padding(top = 10.dp), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(b.form, color = Pal.text, fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
                    Text(fmt.format(Date(b.time)), color = Pal.muted, fontSize = 12.sp)
                }
                IconButton(onClick = { if (ready) confirm = b else say("Нет связи с платой") }) { Icon(Icons.Rounded.Restore, "Вернуть", tint = Pal.green) }
                IconButton(onClick = { PetHub.deleteBackup(b) }) { Icon(Icons.Rounded.Delete, "Удалить", tint = Pal.muted) }
            }
        }
    }
    confirm?.let { b ->
        AlertDialog(
            onDismissRequest = { confirm = null },
            containerColor = Pal.panel,
            title = { Text("Вернуть питомца из копии?") },
            text = { Text("Текущий питомец на плате заменится на «${b.form}» от ${fmt.format(Date(b.time))}.") },
            confirmButton = { TextButton({ PetHub.restore(b) { say(it) }; confirm = null }) { Text("Вернуть") } },
            dismissButton = { TextButton({ confirm = null }) { Text("Отмена") } },
        )
    }
}

// --- обновление прошивки по Bluetooth ---

@Composable
private fun FirmwareUpdate(known: Boolean) {
    val fw by PetHub.firmware.collectAsState()
    val dfu by PetHub.dfu.collectAsState()
    var picked by remember { mutableStateOf<android.net.Uri?>(null) }
    val pick = androidx.activity.compose.rememberLauncherForActivityResult(
        androidx.activity.result.contract.ActivityResultContracts.OpenDocument()) { uri -> if (uri != null) picked = uri }
    Card(title = "Прошивка платы") {
        Text(if (fw != null) "Сейчас стоит $fw" else "Версия неизвестна", color = Pal.text, fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
        Text("Обновление по воздуху: выбери файл bit-t114-*.zip. Плата перезапустится и примет его по Bluetooth — кабель не нужен.",
            color = Pal.muted, fontSize = 13.sp)
        Spacer(Modifier.height(10.dp))
        val d = dfu
        if (d == null || d.done || d.failed) {
            PillButton(Icons.Rounded.Save, "Обновить прошивку") {
                if (known) pick.launch(arrayOf("application/zip", "application/octet-stream", "application/x-zip-compressed"))
            }
        }
        if (d != null) {
            Spacer(Modifier.height(10.dp))
            Text(d.text, color = if (d.failed) Pal.red else if (d.done) Pal.green else Pal.text, fontSize = 14.sp)
            if (!d.done && !d.failed) {
                Spacer(Modifier.height(8.dp))
                val k by animateFloatAsState(d.percent / 100f, tween(300), label = "dfu")
                Canvas(Modifier.fillMaxWidth().height(10.dp)) {
                    val r = androidx.compose.ui.geometry.CornerRadius(size.height / 2)
                    drawRoundRect(Pal.bezel, cornerRadius = r)
                    if (k > 0.01f) drawRoundRect(Pal.green, size = size.copy(width = size.width * k), cornerRadius = r)
                }
                Text("Держи телефон рядом с платой и не закрывай приложение.", color = Pal.muted, fontSize = 12.sp, modifier = Modifier.padding(top = 6.dp))
            } else {
                Text("Скрыть", color = Pal.muted, fontSize = 13.sp, modifier = Modifier.padding(top = 6.dp).clickable { PetHub.clearDfu() })
            }
        }
    }
    picked?.let { uri ->
        AlertDialog(
            onDismissRequest = { picked = null },
            containerColor = Pal.panel,
            title = { Text("Обновить прошивку?") },
            text = { Text("Плата перезапустится в режим обновления и примет файл по Bluetooth. Это займёт 1–3 минуты. Питомец сохранится.") },
            confirmButton = { TextButton({ PetHub.startDfu(uri); picked = null }) { Text("Обновить") } },
            dismissButton = { TextButton({ picked = null }) { Text("Отмена") } },
        )
    }
}

// --- настройки ---

@Composable
private fun Settings(known: Boolean) {
    val sounds by PetHub.sounds.collectAsState()
    val sync by PetHub.syncTime.collectAsState()
    Card(title = "Настройки") {
        ToggleRow("Звуки вместо динамика", "писк, когда зовёт; будильник, фанфары", sounds) { PetHub.setSounds(it) }
        ToggleRow("Время с телефона", "ставить часы платы при каждом подключении", sync) { PetHub.setSyncTime(it) }
        val steps by PetHub.stepsOn.collectAsState()
        val askSteps = androidx.activity.compose.rememberLauncherForActivityResult(
            androidx.activity.result.contract.ActivityResultContracts.RequestPermission()) { ok -> PetHub.setSteps(ok) }
        if (PetHub.stepsAvailable) ToggleRow("Шаги с телефона", "шагомер телефона = прогулки питомца, без GPS", steps) { on ->
            val perm = PetHub.stepsPermission()
            if (on && perm != null) askSteps.launch(perm) else PetHub.setSteps(on)
        }
        WeatherRow()
        if (known) Text("Забыть плату", color = Pal.red, fontSize = 14.sp, modifier = Modifier.padding(top = 10.dp).clickable { PetHub.forget() })
    }
}

@Composable
private fun ToggleRow(title: String, hint: String, value: Boolean, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth().padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(title, color = Pal.text, fontSize = 15.sp)
            Text(hint, color = Pal.muted, fontSize = 12.sp)
        }
        Switch(value, onChange, colors = SwitchDefaults.colors(checkedTrackColor = Pal.accent, checkedThumbColor = Color.White))
    }
}

@Composable
private fun WeatherRow() {
    val place by PetHub.place.collectAsState()
    val now by PetHub.weatherNow.collectAsState()
    val syncStatus by PetHub.weatherSync.collectAsState()
    var edit by remember { mutableStateOf(false) }
    var text by remember { mutableStateOf("") }
    var busy by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }
    val scope = rememberCoroutineScope()
    Column(Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        Text("Погода за окном платы", color = Pal.text, fontSize = 15.sp)
        val p = place
        if (p != null && !edit) {
            val w = now
            Text(p.name + (w?.let { " · ${Weather.names[it.kind]}, ${if (it.temp > 0) "+" else ""}${it.temp}°" } ?: ""),
                color = Pal.muted, fontSize = 12.sp)
            Text(syncStatus, color = Pal.muted, fontSize = 12.sp, modifier = Modifier.padding(top = 4.dp))
            Row {
                Text("Обновить погоду", color = Pal.accent, fontSize = 13.sp, modifier = Modifier.padding(top = 6.dp, end = 16.dp).clickable { PetHub.updateWeather() })
                Text("Сменить город", color = Pal.accent, fontSize = 13.sp, modifier = Modifier.padding(top = 6.dp, end = 16.dp).clickable { edit = true; text = "" })
                Text("Выключить", color = Pal.muted, fontSize = 13.sp, modifier = Modifier.padding(top = 6.dp).clickable { PetHub.clearCity() })
            }
        } else {
            Text("Дождь, снег и солнце на экране по погоде в твоём городе", color = Pal.muted, fontSize = 12.sp)
            Row(verticalAlignment = Alignment.CenterVertically) {
                OutlinedTextField(text, { text = it; error = null }, Modifier.weight(1f).padding(top = 6.dp), singleLine = true,
                    placeholder = { Text("Город", color = Pal.muted) }, isError = error != null,
                    colors = OutlinedTextFieldDefaults.colors(focusedBorderColor = Pal.accent, unfocusedBorderColor = Pal.line, focusedTextColor = Pal.text, unfocusedTextColor = Pal.text),
                    shape = RoundedCornerShape(14.dp))
                TextButton(enabled = text.isNotBlank() && !busy, onClick = {
                    busy = true
                    scope.launch {
                        val ok = PetHub.setCity(text)
                        busy = false
                        if (ok) edit = false else error = "Город не найден или нет интернета"
                    }
                }) { Text(if (busy) "…" else "Найти") }
            }
            error?.let { Text(it, color = Pal.red, fontSize = 12.sp) }
        }
    }
}

// --- команда вручную ---

@Composable
private fun Terminal(ready: Boolean) {
    var text by remember { mutableStateOf("") }
    val send = { if (text.isNotBlank()) { PetHub.act(text.trim()); text = "" } }
    Card(title = "Команда") {
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(
                text, { text = it }, Modifier.weight(1f), singleLine = true, enabled = ready,
                placeholder = { Text("статус, помощь, время 21:40…", color = Pal.muted) },
                keyboardOptions = KeyboardOptions(imeAction = androidx.compose.ui.text.input.ImeAction.Send),
                keyboardActions = KeyboardActions(onSend = { send() }),
                colors = OutlinedTextFieldDefaults.colors(focusedBorderColor = Pal.accent, unfocusedBorderColor = Pal.line, focusedTextColor = Pal.text, unfocusedTextColor = Pal.text),
                shape = RoundedCornerShape(14.dp),
            )
            IconButton(onClick = { send() }, enabled = ready) { Icon(Icons.AutoMirrored.Rounded.Send, "Отправить", tint = if (ready) Pal.accent else Pal.muted) }
        }
    }
}

// --- журнал ---

@Composable
private fun Journal() {
    val log by PetHub.log.collectAsState()
    if (log.isEmpty()) return
    val fmt = remember { SimpleDateFormat("HH:mm", Locale.US) }
    Card(title = "Журнал") {
        log.take(25).forEach { l ->
            Row(Modifier.padding(vertical = 5.dp)) {
                Text(fmt.format(Date(l.time)), color = Pal.muted, fontSize = 12.sp, fontFamily = FontFamily.Monospace, modifier = Modifier.width(44.dp))
                Text(l.text, color = if (l.event) Pal.text else Pal.muted, fontSize = 13.sp,
                    fontFamily = if (l.event) FontFamily.Default else FontFamily.Monospace)
            }
        }
    }
}

// --- выбор платы ---

@Composable
private fun DevicePicker(onPick: (String) -> Unit) {
    val list by PetHub.found.collectAsState()
    val scanning by PetHub.scanning.collectAsState()
    Column(Modifier.fillMaxWidth().padding(horizontal = 20.dp).padding(bottom = 28.dp)) {
        Text("Платы рядом", color = Pal.text, fontSize = 20.sp, fontWeight = FontWeight.Bold)
        Text(if (scanning) "ищу… включи на плате Настройки → Bluetooth" else "поиск окончен", color = Pal.muted, fontSize = 13.sp)
        Spacer(Modifier.height(12.dp))
        if (list.isEmpty() && !scanning) PillButton(Icons.Rounded.BluetoothSearching, "Искать ещё раз") { PetHub.startScan() }
        list.forEach { f ->
            Row(
                Modifier.fillMaxWidth().padding(vertical = 4.dp).clip(RoundedCornerShape(14.dp)).background(Pal.panel2)
                    .clickable { onPick(f.address) }.padding(14.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Icon(Icons.Rounded.Bluetooth, null, tint = Pal.accent)
                Spacer(Modifier.width(10.dp))
                Column(Modifier.weight(1f)) {
                    Text(f.name, color = Pal.text, fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
                    Text(f.address, color = Pal.muted, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
                }
                Text("${f.rssi} дБм", color = Pal.muted, fontSize = 12.sp)
            }
        }
    }
}

// --- карточка ---

@Composable
private fun Card(title: String? = null, content: @Composable () -> Unit) {
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(Pal.panel)
        .border(1.dp, Pal.line, RoundedCornerShape(20.dp)).padding(16.dp)) {
        if (title != null) {
            Text(title.uppercase(), color = Pal.muted, fontSize = 12.sp, fontWeight = FontWeight.Bold, letterSpacing = 1.2.sp)
            Spacer(Modifier.height(10.dp))
        }
        content()
    }
}
