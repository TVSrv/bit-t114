// Проверка новых функций: перенос сохранения v1, наследство, будильник, достижения, игры.
#define private public
#include "../core/game.h"
#undef private
#include "../core/rooms.h"
#include "../core/artdata.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <cstdlib>
using namespace pet;
static gfx::Color fb[gfx::W * gfx::H];
static gfx::Canvas cv(fb);
static int fails = 0;
static void check(bool ok, const char* w) { printf("%s %s\n", ok ? "ок  " : "ОШИБКА", w); if (!ok) fails++; }
static void run(Game& g, int ms, int step = 16) { for (int t = 0; t < ms; t += step) { g.tick(step); g.render(cv); } }

int main() {
  // 1. перенос сохранения первой версии
  SaveV1 o; memset(&o, 0, sizeof o);
  o.magic = 0x42495431; o.version = 1; o.form = MIDDLE; o.alive = 1; o.hunger = 42; o.happy = 33;
  o.energy = 77; o.weight = 21; o.epoch = 86400 * 9000 + 3600 * 10; o.totalMeters = 1234; o.generation = 3;
  o.gamesWon = 7; o.seed = 99; o.tz = 3; o.brightness = 2; o.leds = 1;
  o.crc = Game::crc32(&o, offsetof(SaveV1, crc));
  Game g; memset(&g.s, 0, sizeof g.s);
  check(g.loadRaw((const uint8_t*)&o, sizeof o), "сохранение v1 загружается");
  check(g.s.version == 4 && g.s.form == MIDDLE && g.s.hunger == 42 && g.s.totalMeters == 1234 && g.s.generation == 3,
        "питомец, сытость, километраж и поколение перенесены");
  check((g.s.formsSeen >> BABY & 1) && (g.s.formsSeen >> JUNIOR & 1) && (g.s.formsSeen >> MIDDLE & 1), "пройденные формы отмечены в коллекции");
  check(g.s.totalWins == 7 && g.s.trait < TRAIT_COUNT && g.s.favFood < 3, "победы перенесены, характер задан");
  run(g, 1000);
  check(g.has(ACH_KM1), "1234 м прогулок дают «Первый километр»");
  Pose p = g.basePose();
  check(p.acc & ACC_SCARF, "и питомец в шарфе");
  // повреждённое сохранение не принимается
  SaveV1 bad = o; bad.hunger = 1;
  Game g2; memset(&g2.s, 0, sizeof g2.s);
  check(!g2.loadRaw((const uint8_t*)&bad, sizeof bad), "испорченное сохранение отвергается");

  // 2. полнота и старость
  g.s.weight = 40; p = g.basePose();
  check(p.fat > 0.5f, "лишний вес — питомец полнеет");
  g.s.form = SENIOR; g.s.weight = 30; g.s.ageMin = 36 * 24 * 60; p = g.basePose();
  check(p.gray > 0.5f && p.cane, "к концу жизни Сеньор седеет и берёт трость");

  // 3. наследство
  g.s.form = SENIOR; g.s.trait = T_SMART; g.s.discipline = 90; g.s.achievements |= 1u << ACH_WEEK;
  uint32_t ach = g.s.achievements;
  int same = 0;
  for (int i = 0; i < 40; i++) {
    Game h = g; h.init(1000 + i);
    if (h.s.trait == T_SMART) same++;
    if (i == 0) {
      check(h.s.parentForm == SENIOR && h.s.generation == g.s.generation + 1, "новый питомец помнит родителя");
      check(h.s.achievements == ach && h.s.totalMeters == g.s.totalMeters, "достижения и километраж общие");
      check(h.s.discipline == 20, "воспитанный родитель даёт стартовую дисциплину");
    }
  }
  printf("     черта родителя передалась в %d из 40\n", same);
  check(same >= 20 && same <= 38, "черта наследуется примерно в 70% случаев");

  // 4. будильник
  Game a; memset(&a.s, 0, sizeof a.s); a.init(5); a.demo(0); a.s.form = JUNIOR;
  a.s.alarmOn = 1; a.s.alarmH = (uint8_t)a.hourNow(); a.s.alarmM = (uint8_t)((a.minNow() + 2) % 60);
  a.justEvolved = false;
  a.speed = 60; run(a, 4000, 33);
  check(a.scene == S_ALARM && a.justEvolved, "будильник срабатывает и просит включить экран");
  check(a.ledColor() != 0, "светодиоды мигают радугой");
  a.speed = 1; a.button(B_SHORT);
  check(a.scene == S_HOME && a.s.alarmWakes == 1, "нажатие гасит будильник");

  // 5. игры: идеальная игра в каждую
  Game q; memset(&q.s, 0, sizeof q.s); q.init(8); q.demo(0); q.s.form = MIDDLE; q.s.discipline = 100;
  // Деплой: прыгаем, когда баг подъезжает
  q.startGame(G_DEPLOY);
  for (int t = 0; t < 120000 && q.gPhase != 3; t += 16) {
    for (float b : q.dBugs) if (b > 66 && b < 66 + q.dSpeed * 0.12f && q.dLift <= 0.01f) { q.button(B_SHORT); break; }
    q.tick(16); q.render(cv);
  }
  check(q.gPhase == 3 && q.gScore == 10 && !q.gResult, "Деплой: 10 багов перепрыгнуто без падения");
  q.button(B_SHORT);
  q.startGame(G_DEPLOY); run(q, 15000);
  check(q.gPhase == 3 && q.gResult == 1 && q.gScore == 0, "Деплой: без прыжков первый баг сбивает");
  q.button(B_SHORT);
  // Пинг
  q.startGame(G_PING);
  for (int t = 0; t < 60000 && q.gPhase != 3; t += 8) {
    if (q.gPhase == 0 && fabsf(q.pRing - 14) < 1.0f) q.button(B_SHORT);
    q.tick(8); q.render(cv);
  }
  check(q.gPhase == 3 && q.gScore == 10, "Пинг: 5 точных нажатий = 10 очков");
  q.button(B_SHORT);
  // Компиляция
  q.startGame(G_COMPILE);
  for (int t = 0; t < 60000 && q.gPhase != 3; t += 8) {
    if (q.gPhase == 0 && q.cProg > q.cZone + q.cZoneW * 0.5f) q.button(B_SHORT);
    q.tick(8); q.render(cv);
  }
  check(q.gPhase == 3 && q.gScore == 5, "Компиляция: 5 сборок в зелёной зоне");
  q.button(B_SHORT);
  q.startGame(G_COMPILE); run(q, 30000);
  check(q.gPhase == 3 && q.gScore == 0, "Компиляция: если не нажимать — перегрев и ноль");
  printf("     побед всего: %u, сыграно: %u\n", q.s.totalWins, q.s.gamesPlayed);

  // 6. любимая еда
  Game e; memset(&e.s, 0, sizeof e.s); e.init(4); e.demo(0); e.s.form = JUNIOR; e.s.discipline = 100; e.s.happy = 50; e.s.hunger = 20;
  e.startAction(A_EAT, e.s.favFood); run(e, 3000);
  check(e.s.favFed == 1 && e.s.happy >= 60, "любимая еда: +10 настроения и счётчик «Гурмана»");
  // 7. перенос сохранения второй версии
  {
    Game a2; memset(&a2.s, 0, sizeof a2.s); a2.init(77); a2.demo(0); a2.s.form = HACKER; a2.s.trait = T_SMART; a2.s.alarmH = 6;
    uint8_t blob[sizeof(Save)];
    memset(blob, 0xEE, sizeof blob);
    Save v = a2.s; v.version = 2;
    size_t body = offsetof(Save, questLat);
    memcpy(blob, &v, body);
    uint32_t crc = Game::crc32(blob, body);
    memcpy(blob + body, &crc, 4);
    Game b2; memset(&b2.s, 0, sizeof b2.s);
    check(b2.loadRaw(blob, body + 4), "сохранение v2 загружается");
    check(b2.s.version == 4 && b2.s.form == HACKER && b2.s.trait == T_SMART && b2.s.alarmH == 6 && b2.s.bleOn == 1,
          "форма, черта и будильник перенесены, Bluetooth включён");
  }

  // 8. GPS-квест: идём к цели по поддельным координатам
  {
    Game w; memset(&w.s, 0, sizeof w.s); w.init(21); w.demo(0); w.s.form = JUNIOR; w.s.happy = 40;
    w.activate(I_WALK);
    double lat = 55.75, lon = 37.61;
    auto feed = [&](double la, double lo) {
      GpsFix f; f.on = true; f.alive = true; f.posValid = true; f.sats = 9; f.inView = 14; f.hdop = 1.0f;
      f.speedKmh = 5.4f;  // идём 1,5 м/с
      f.lat = la; f.lon = lo; w.gpsFeed(f); w.tick(1000); w.render(cv);
    };
    feed(lat, lon);
    check(w.s.questKind != 0 && w.questDist >= 110 && w.questDist <= 310, "на прогулке появляется цель в 120–300 м");
    int steps = 0;
    while (w.s.questsDone == 0 && steps < 600) {  // шаг 1,5 м/с прямо к цели
      double tl = w.s.questLat / 1e6, tn = w.s.questLon / 1e6;
      double dy = (tl - lat) * 111195, dx = (tn - lon) * 111195 * cos(lat * 0.0174533);
      double d = sqrt(dx * dx + dy * dy);
      lat += dy / d * 1.5 / 111195; lon += dx / d * 1.5 / (111195 * cos(lat * 0.0174533));
      feed(lat, lon); steps++;
    }
    printf("     дошли за %d с\n", steps);
    check(w.s.questsDone == 1, "цель найдена, квест засчитан");
    float bearingErr = 0;
    { // стрелка: идём на север, цель на востоке — относительный угол около 90°
      w.s.questLat = (int32_t)(lat * 1e6); w.s.questLon = (int32_t)((lon + 200 / (111195 * cos(lat * 0.0174533))) * 1e6); w.s.questKind = 1;
      for (int i = 0; i < 4; i++) { lat += 5 / 111195.0; feed(lat, lon); }
      float rel = w.questBearing - w.walkCourse; if (rel < 0) rel += 360;
      bearingErr = fabsf(rel - 90);
    }
    check(bearingErr < 12, "стрелка по ходу движения показывает направо, когда цель справа");
    int before = w.s.placesTotal;
    for (int i = 0; i < 400; i++) { lat += 3 / 111195.0; feed(lat, lon); }  // 1,2 км на север
    printf("     новых мест за 1,2 км: %d\n", w.s.placesTotal - before);
    check(w.s.placesTotal - before >= 4, "новые места считаются");
  }
  // 9. светодиоды и «Реакция»
  {
    Game l; memset(&l.s, 0, sizeof l.s); l.init(31); l.demo(1);   // голод
    check(l.callColor() == 0xFF6000 && l.ledColor() != 0, "голод — оранжевый пульс");
    l.demo(2); run(l, 100);
    uint32_t a = l.ledColor(); run(l, 160); uint32_t b = l.ledColor();
    check(l.callColor() == 0xFF0010 && (a == 0) != (b == 0), "болезнь — частое красное мигание");
    l.demo(0); l.s.form = JUNIOR; l.s.energy = 50; l.activate(I_LIGHT); run(l, 1000);
    check(l.s.asleep && l.ledColor() == 0x482000, "выключили свет — ночник светит тёплым");
    l.speed = 1; run(l, 11 * 60000, 1000);
    check(l.ledColor() == 0, "через 11 минут ночник погас");
    Game r; memset(&r.s, 0, sizeof r.s); r.init(32); r.demo(0); r.s.form = MIDDLE;
    r.startGame(G_REACT);
    for (int t = 0; t < 60000 && r.gPhase != 3; t += 10) {
      if (r.gPhase == 0 && r.now >= r.rGoAt + 180) r.button(B_SHORT);
      r.tick(10); r.render(cv);
    }
    check(r.gPhase == 3 && r.gScore == 10 && r.s.bestReactCs == 18, "Реакция: 5 раз по 180 мс = 10 очков, рекорд 0,18 с");
    check(r.has(ACH_REFLEX), "достижение «Реакция» за быстрее 0,25 с");
    r.button(B_SHORT); r.startGame(G_REACT); run(r, 200); r.button(B_SHORT);
    check(r.gPhase == 1 && r.rTime == 0, "нажатие до зелёного — фальстарт");
  }
  // 10. команды по Bluetooth
  {
    Game b; memset(&b.s, 0, sizeof b.s); b.init(41); b.demo(0); b.s.form = JUNIOR; b.s.discipline = 100; b.s.hunger = 30;
    char out[600];
    b.command("Статус", out, sizeof out); printf("     > статус\n     %s\n", out);
    check(strstr(out, "Джун") && strstr(out, "Сытость 30%"), "«Статус» (с заглавной) отвечает состоянием");
    b.command("время 21:40", out, sizeof out);
    check(b.hourNow() == 21 && b.minNow() == 40 && b.s.clockSource == 3, "«время 21:40» ставит часы с телефона");
    b.command("ВРЕМЯ 03.11.2026 07:05:30", out, sizeof out);
    check(b.s.epoch == daysFrom2000(2026, 11, 3) * 86400u + 7 * 3600 + 5 * 60 + 30, "дата и время целиком, заглавными");
    b.command("time 2026-11-03 21:41", out, sizeof out);
    check(b.hourNow() == 21 && b.minNow() == 41, "формат 2026-11-03 21:41");
    b.command("время 25:00", out, sizeof out);
    check(strstr(out, "Неверное") != nullptr, "неверное время отклоняется");
    b.command("еда пицца", out, sizeof out); run(b, 3200);
    printf("     > еда пицца → %s; сытость %d\n", out, b.s.hunger);
    check(b.s.hunger >= 65, "«еда пицца» кормит");
    b.command("копия", out, sizeof out);
    printf("     > копия → %d символов\n", (int)strlen(out));
    std::string code = out;
    int gen = b.s.generation; b.s.hunger = 3; b.s.form = LEGACY;
    std::string cmd = "восстановить " + code;
    b.command(cmd.c_str(), out, sizeof out);
    check(b.s.form == JUNIOR && b.s.hunger >= 65 && b.s.generation == gen, "«восстановить» возвращает питомца из копии");
    std::string broken = cmd; broken[30] = broken[30] == 'A' ? 'B' : 'A';
    b.s.form = LEGACY;
    b.command(broken.c_str(), out, sizeof out);
    check(b.s.form == LEGACY && strstr(out, "повреждён"), "испорченный код не меняет питомца");
    b.command("время 12:00", out, sizeof out);
    uint16_t seq = b.eventSeq; b.s.form = JUNIOR; b.s.hunger = 0; b.speed = 60; run(b, 3000, 100);
    printf("     событие: %s\n", b.eventText);
    check(b.eventSeq != seq && strstr(b.eventText, "хочет есть"), "зов питомца становится событием для телефона");
    b.command("data", out, sizeof out);
    printf("     > data → %s\n", out);
    int fields = 0; for (char* p = out; *p; p++) if (*p == '|') fields++;
    check(strncmp(out, "D|3|", 4) == 0 && fields == 56, "«data» — 55 полей питомца и информера (старые индексы сохранены)");
    b.command("абракадабра", out, sizeof out);
    check(strstr(out, "Не знаю") != nullptr, "неизвестная команда — подсказка");
  }
  // 11. встреча двух плат по LoRa (эфир: пакет доходит, если второй слушает)
  {
    Game A, B; memset(&A.s, 0, sizeof A.s); memset(&B.s, 0, sizeof B.s);
    A.init(51); B.init(52); A.demo(0); B.demo(0);
    A.s.form = JUNIOR; B.s.form = HACKER; A.s.deviceId = 0xA1; B.s.deviceId = 0xB2;
    A.s.loraOn = B.s.loraOn = 1; A.s.happy = B.s.happy = 50; A.s.discipline = 100;
    uint8_t pkt[32]; int len;
    int metAt = -1;
    for (int t = 0; t < 30 * 60 * 1000 && metAt < 0; t += 100) {
      if (A.loraWantTx(pkt, len) && B.loraListen()) B.loraRx(pkt, len, -60);
      if (B.loraWantTx(pkt, len) && A.loraListen()) A.loraRx(pkt, len, -60);
      A.tick(100); B.tick(100);
      if (A.visiting() && B.visiting()) metAt = t;
    }
    printf("     встретились через %d с\n", metAt / 1000);
    check(metAt >= 0, "две платы находят друг друга за 30 минут");
    check(A.s.friendsMet == 1 && B.s.friendsMet == 1 && A.friendForm == HACKER && B.friendForm == JUNIOR, "оба видят друг друга, друг записан");
    check(A.has(ACH_FRIEND) && B.has(ACH_FRIEND), "достижение «Друг» у обоих");
    run(A, 300);
    // угощение
    int hb = B.s.happy;
    A.openList(0);
    check(A.listCount == 5, "в гостях в меню еды появляется «Угостить гостя»");
    A.listSel = 3; A.listChoose();
    bool sent = false;
    for (int t = 0; t < 20000 && !sent; t += 100) {
      if (A.loraWantTx(pkt, len)) { if (pkt[2] == 2) { sent = true; if (B.loraListen()) B.loraRx(pkt, len, -60); } else if (B.loraListen()) B.loraRx(pkt, len, -60); }
      A.tick(100); B.tick(100);
    }
    check(sent && B.s.happy >= hb + 15, "угощение доходит: у друга +15 настроения");
    // гость уходит через 3 минуты тишины
    for (int t = 0; t < 4 * 60000; t += 1000) { A.tick(1000); }
    check(!A.visiting(), "без сигнала гость уходит через 3 минуты");
    // чужие пакеты
    Game C; memset(&C.s, 0, sizeof C.s); C.init(53); C.demo(0); C.s.form = MIDDLE; C.s.loraOn = 1; C.s.deviceId = 0xC3;
    uint8_t junk[16] = {'X', 'Y', 1};
    C.loraRx(junk, 16, -50);
    check(!C.visiting(), "посторонние пакеты не считаются встречей");
    // при выключенных встречах радио не нужно
    C.s.loraOn = 0;
    check(!C.loraListen() && !C.loraWantTx(pkt, len), "встречи выключены — радио молчит");
    int listen = 0;
    C.s.loraOn = 1;
    for (int t = 0; t < 600000; t += 100) { C.loraWantTx(pkt, len); if (C.loraListen()) listen++; C.tick(100); }
    printf("     приёмник включён %d%% времени\n", listen * 100 / 6000);
    check(listen * 100 / 6000 <= 20, "приёмник слушает не больше 20% времени");
  }
  // 12. время с GPS: «время с включения» не принимается, настоящее — после 30 с привязки
  {
    Game k; memset(&k.s, 0, sizeof k.s); k.init(61); k.demo(0); k.s.form = JUNIOR; k.s.tz = 3;
    k.scene = S_GPS; k.gpsOnSince = k.now;
    uint32_t real = daysFrom2000(2026, 11, 3) * 86400u + 20 * 3600 + 8 * 60;  // 20:08 UTC
    uint32_t before = k.s.epoch;
    for (int i = 0; i < 200; i++) {  // модуль на 8-й минуте шлёт 00:08 с «привязкой»
      GpsFix f; f.on = f.alive = f.posValid = f.timeValid = true; f.sats = 6; f.hdop = 1.2f;
      f.moduleUp = 480 + i / 2; f.utc = daysFrom2000(2026, 11, 3) * 86400u + 480 + i / 2;
      k.gpsFeed(f); k.tick(500);
    }
    check(k.s.clockSource != 2 && k.gpsTimeNote == 2, "00:08 на 8-й минуте работы модуля — не принято");
    (void)before;
    for (int i = 0; i < 40; i++) {  // настоящее время, но привязка только 20 с
      GpsFix f; f.on = f.alive = f.posValid = f.timeValid = true; f.sats = 6; f.hdop = 1.2f;
      f.moduleUp = 600 + i / 2; f.utc = real + i / 2;
      k.gpsFeed(f); k.tick(500);
    }
    check(k.s.clockSource != 2, "привязка меньше 30 с — время ещё не принято");
    for (int i = 40; i < 80; i++) {
      GpsFix f; f.on = f.alive = f.posValid = f.timeValid = true; f.sats = 6; f.hdop = 1.2f;
      f.moduleUp = 600 + i / 2; f.utc = real + i / 2;
      k.gpsFeed(f); k.tick(500);
    }
    check(k.s.clockSource == 2 && k.hourNow() == 23, "после 30 с устойчивой привязки — 23:08 по Москве");
  }
  // 13. стоящая плата с прыгающими координатами не набирает метры; сброс прогулок
  {
    Game st; memset(&st.s, 0, sizeof st.s); st.init(71); st.demo(0); st.s.form = JUNIOR; st.activate(I_WALK);
    uint32_t m0 = st.s.totalMeters;
    for (int i = 0; i < 600; i++) {
      GpsFix f; f.on = f.alive = f.posValid = true; f.sats = 5; f.hdop = 2.5f; f.speedKmh = 0.3f;
      f.lat = 55.75 + ((i * 7919) % 100 - 50) * 1e-5; f.lon = 37.61 + ((i * 104729) % 100 - 50) * 1e-5;  // скачки до ~50 м
      st.gpsFeed(f); st.tick(500);
    }
    printf("     прыгающие координаты на месте: +%u м\n", st.s.totalMeters - m0);
    check(st.s.totalMeters == m0, "стоим на месте — метры не растут, даже если координаты скачут");
    st.s.totalMeters = 3929; st.s.questsDone = 3; st.s.placesTotal = 9; st.s.achievements |= (1u << ACH_KM1) | (1u << ACH_WEEK);
    char out[200];
    st.command("сброс прогулок", out, sizeof out);
    check(st.s.totalMeters == 0 && st.s.questsDone == 0 && st.s.placesTotal == 0 && !st.has(ACH_KM1) && st.has(ACH_WEEK),
          "«сброс прогулок» обнуляет метры, места, квесты и их награды, остальное не трогает");
  }
  // 14. монеты, магазин, шаги, праздники, перенос v3
  {
    Game m; memset(&m.s, 0, sizeof m.s); m.init(81); m.demo(0); m.s.form = JUNIOR; m.s.discipline = 100; m.s.coins = 0;
    char out[300];
    m.startGame(G_PING);
    for (int t = 0; t < 60000 && m.gPhase != 3; t += 8) { if (m.gPhase == 0 && fabsf(m.pRing - 14) < 1.0f) m.button(B_SHORT); m.tick(8); m.render(cv); }
    printf("     монеты за идеальный «Пинг»: %u\n", m.s.coins);
    check(m.s.coins >= 15, "за игру начисляются монеты");
    m.button(B_SHORT);
    m.s.coins = 45;
    m.openList(3); m.listSel = 3; m.listChoose();  // цилиндр 120 — не хватает
    check(!((m.s.owned >> IT_TOPHAT) & 1) && m.s.coins == 45, "не хватает монет — не покупается");
    m.openList(3); m.listSel = 1; m.listChoose();  // шапка с помпоном 40
    check(((m.s.owned >> IT_BEANIE) & 1) && m.s.hat == IT_BEANIE && m.s.coins >= 5 && m.has(ACH_SHOPPER), "куплена и надета шапка, достижение «Модник»");
    m.openList(3); m.listSel = 0; m.listChoose();
    check(m.s.hat == 0, "«Без шапки» снимает");
    m.openList(3); m.listSel = 1; m.listChoose();
    check(m.s.hat == IT_BEANIE, "купленную надевать бесплатно");
    Pose pp = m.basePose();
    check(pp.hat == IT_BEANIE, "питомец рисуется в шапке");
    // шаги
    uint32_t mm = m.s.totalMeters;
    m.command("шаги 1500", out, sizeof out);
    check(m.s.stepsToday == 1500 && m.s.totalMeters == mm + 1050, "1500 шагов = 1050 м прогулки");
    m.command("шаги 9000", out, sizeof out);
    check(m.has(ACH_STEPS), "10 000 шагов за день — достижение «Ходок»");
    m.command("шаги 999999", out, sizeof out);
    check(m.s.stepsToday == 10500, "нереальный пакет шагов отбрасывается");
    // праздники
    m.s.clockSource = 1;
    m.s.epoch = daysFrom2000(2026, 9, 13) * 86400u + 12 * 3600;
    check(m.holiday() == HD_PROGRAMMER, "13 сентября 2026 — День программиста (256-й день)");
    m.s.epoch = daysFrom2000(2028, 9, 12) * 86400u + 12 * 3600;
    check(m.holiday() == HD_PROGRAMMER, "в високосный 2028 — 12 сентября");
    m.s.epoch = daysFrom2000(2026, 12, 31) * 86400u + 12 * 3600;
    check(m.holiday() == HD_NEWYEAR && m.basePose().hat == IT_BEANIE, "Новый год; купленная шапка важнее новогодней");
    m.s.hat = 0;
    check(m.basePose().hat == 20, "без своей шапки в Новый год — шапка Деда Мороза");
    uint32_t c0 = m.s.coins;
    m.speed = 60; run(m, 3000, 100);
    check(m.s.coins >= c0 + 50, "праздничный бонус +50 монет");
    uint32_t c1 = m.s.coins; run(m, 3000, 100);
    check(m.s.coins < c1 + 50, "бонус — один раз в день");
    m.s.epoch = daysFrom2000(2026, 10, 31) * 86400u + 12 * 3600;
    check(m.holiday() == HD_HALLOWEEN, "31 октября — Хэллоуин");
    // перенос v3 -> v4
    Save v = m.s; v.version = 3;
    uint8_t blob[sizeof(Save)];
    size_t body = offsetof(Save, totalSteps);
    memcpy(blob, &v, body);
    uint32_t crc = Game::crc32(blob, body);
    memcpy(blob + body, &crc, 4);
    Game z; memset(&z.s, 0, sizeof z.s);
    check(z.loadRaw(blob, body + 4) && z.s.version == 4 && z.s.form == JUNIOR && z.s.coins == 0, "сохранение v3 переносится в v4");
  }
  // 15. дуэль по LoRa на двух платах; оценка батареи
  {
    Game A, B; memset(&A.s, 0, sizeof A.s); memset(&B.s, 0, sizeof B.s);
    A.init(91); B.init(92); A.demo(0); B.demo(0); A.s.form = JUNIOR; B.s.form = HACKER;
    A.s.deviceId = 0xA1; B.s.deviceId = 0xB2; A.s.loraOn = B.s.loraOn = 1; A.s.discipline = B.s.discipline = 100;
    A.s.coins = B.s.coins = 0;
    uint8_t pkt[32]; int len;
    auto air = [&](int ms) {
      for (int t = 0; t < ms; t += 50) {
        if (A.loraWantTx(pkt, len) && B.loraListen()) B.loraRx(pkt, len, -60);
        if (B.loraWantTx(pkt, len) && A.loraListen()) A.loraRx(pkt, len, -60);
        A.tick(50); B.tick(50); A.render(cv); B.render(cv);
      }
    };
    for (int i = 0; i < 40 && !(A.visiting() && B.visiting()); i++) air(5000);
    check(A.visiting() && B.visiting(), "платы встретились");
    A.s.coins = B.s.coins = 0;  // 25 монет за «Друга» не считаем
    A.openList(1);
    check(A.listCount == 8, "в гостях в «Играх» есть «Дуэль с гостем»");
    A.listSel = 6; A.listChoose();
    air(3000);
    check(A.duelState == 1 && B.duelState == 2 && B.scene == S_DUEL, "вызов дошёл, на второй плате вопрос");
    B.button(B_LONG); air(200);
    check(A.duelState == 3 && B.duelState == 3, "вызов принят, обе ждут зелёного");
    // A жмёт через 250 мс после своего зелёного, B через 400 мс
    for (int t = 0; t < 8000 && (A.duelState == 3 || B.duelState == 3); t += 10) {
      if (A.duelState == 3 && A.now >= A.duelGoAt + 250) A.button(B_SHORT);
      if (B.duelState == 3 && B.now >= B.duelGoAt + 400) B.button(B_SHORT);
      air(10);
    }
    air(4000);
    printf("     дуэль: A %u мс, B %u мс; монеты A %u, B %u\n", A.duelMy, B.duelMy, A.s.coins, B.s.coins);
    check(A.duelState == 5 && B.duelState == 5, "оба получили результат соперника");
    check(A.s.coins == 20 && B.s.coins == 5, "быстрый получил 20 монет, второй — 5");
    // батарея: 60% -> 54% за 6 замеров (час) -> ~9 ч
    Game bt; memset(&bt.s, 0, sizeof bt.s); bt.init(93); bt.demo(0);
    for (int i = 0; i < 7; i++) { bt.setBattery(60 - i, false); bt.tick(600000); }
    printf("     батарея: осталось ~%d ч\n", bt.batteryHoursLeft());
    check(bt.batteryHoursLeft() >= 8 && bt.batteryHoursLeft() <= 10, "оценка батареи: −6%% в час при 54%% ≈ 9 ч");
  }
  // Настройка «Время по GPS: выкл» обязана работать даже при открытом экране GPS.
  Game noGpsTime; memset(&noGpsTime.s, 0, sizeof noGpsTime.s); noGpsTime.init(222);
  noGpsTime.s.gpsAuto = 0;
  noGpsTime.s.clockSource = 3;
  noGpsTime.s.epoch = daysFrom2000(2026, 11, 5) * 86400u + 22u * 3600u + 54u * 60u;
  noGpsTime.clockSure = true;
  noGpsTime.scene = S_GPS;
  GpsFix gpsValue;
  gpsValue.timeValid = true;
  gpsValue.posValid = true;
  gpsValue.sats = 12;
  gpsValue.hdop = 1.0f;
  gpsValue.moduleUp = 330;
  gpsValue.utc = daysFrom2000(2026, 11, 5) * 86400u + 19u * 3600u + 54u * 60u;
  uint32_t oldClock = noGpsTime.s.epoch;
  for (int i = 0; i < 80; ++i) noGpsTime.gpsFeed(gpsValue);
  check(noGpsTime.s.epoch == oldClock && noGpsTime.s.clockSource == 3,
        "отключённое время GPS не меняет часы даже на экране проверки");
  check(noGpsTime.fix.posValid && noGpsTime.fix.sats == 12,
        "выключение синхронизации часов не отключает координаты и спутники");
  noGpsTime.s.gpsAuto = 1;
  noGpsTime.stableFix = 0;
  for (int i = 0; i < 80; ++i) noGpsTime.gpsFeed(gpsValue);
  check(noGpsTime.s.clockSource == 2 && noGpsTime.s.epoch == gpsValue.utc + 3u * 3600u,
        "синхронизация GPS снова доступна после включения");
  // 0.35 experimental: мягкий уход, самостоятельные занятия и возврат к классике.
  {
    Game calm; memset(&calm.s, 0, sizeof calm.s); calm.init(101); calm.demo(0);
    calm.s.form = MIDDLE; calm.s.hunger = 0; calm.s.sick = 1; calm.s.bugs = 3;
    check(calm.s.reserved4[0] == 0, "новый питомец начинает со спокойным уходом");
    for (int i = 0; i < 740; ++i) { calm.now += 60000; calm.minute(); }
    check(calm.s.alive && calm.s.sick == 0, "спокойный режим: нет гибели от отсутствия ухода, болезнь проходит");
    check(calm.s.reserved4[1] > 0, "Бит самостоятельно занимается делами");
    check(calm.s.starveMin == 0, "в спокойном режиме не накапливается таймер голодной смерти");
    // Классический режим сохраняет старые риски и ритм.
    Game classic; memset(&classic.s, 0, sizeof classic.s); classic.init(102); classic.demo(0);
    classic.s.form = MIDDLE; classic.s.reserved4[0] = 1;
    classic.s.hunger = 0; classic.s.starveMin = 719;
    classic.minute();
    check(!classic.s.alive && classic.s.causeOfDeath == D_STARVE,
          "классический уход: исходная механика голода сохранена");
    Game toggle; memset(&toggle.s, 0, sizeof toggle.s); toggle.init(103);
    toggle.openList(2); toggle.listSel = 8; toggle.listChoose();
    check(toggle.s.reserved4[0] == 1 && toggle.listCount == 15,
          "режим ухода переключается в настройках");
    toggle.listChoose();
    check(toggle.s.reserved4[0] == 0, "режим ухода можно вернуть на спокойный");
  }
  {
    Game memory; memset(&memory.s, 0, sizeof memory.s); memory.init(104);
    memory.demo(0); memory.s.form = JUNIOR;
    memory.startGame(G_BIT);
    for (int round = 0; round < 5; ++round) {
      memory.tick((uint32_t)(round + 1) * 850u + 250u);
      memory.render(cv);
      check(memory.gPhase == 1, "Память: последовательность показана, ждём ввод");
      for (int i = 0; i <= round; ++i)
        memory.button(memory.memoryPattern[i] ? B_LONG : B_SHORT);
      check(memory.gPhase == 2 && memory.gResult == 1, "Память: правильный ответ принят");
      memory.tick(1200); memory.render(cv);
    }
    check(memory.gPhase == 3 && memory.gScore == 5, "Память: 5 раундов и награда начислена");
    memory.startGame(G_BIT);
    memory.tick(1200); memory.render(cv);
    memory.button(memory.memoryPattern[0] ? B_SHORT : B_LONG);
    check(memory.gPhase == 2 && memory.gResult == 0, "Память: ошибочный ответ отклонён");
  }
  // Ночник — отдельная подсветка: независимо от эффектов и режима экрана.
  {
    Game n; memset(&n.s, 0, sizeof n.s); n.init(141); n.demo(0);
    n.s.form = JUNIOR; n.s.energy = 50;
    n.s.nightLight = 1; n.s.leds = 0;
    n.activate(I_LIGHT);
    check(n.s.asleep && n.nightLightColor() == 0x482000,
          "ночник светит при выключенных обычных светодиодах");
    check(n.ledColor() == 0x482000 && n.ledColorScreenOff() == 0x482000,
          "ночник одинаково работает с включённым и погашенным экраном");
    n.s.callKind = C_HUNGER;
    check(n.ledColorScreenOff() == 0x482000,
          "просьбы питомца не перекрывают ночник после отключения экрана");
    n.tick(610000);
    check(n.nightLightColor() != 0 && n.nightLightColor() < 0x482000,
          "после 10 минут ночник плавно затухает");
    n.tick(51000);
    check(n.nightLightColor() == 0 && n.ledColorScreenOff() == 0,
          "через 11 минут ночник гаснет, режим LED-эффектов выключен");
    n.openList(2); n.listSel = 5; n.listChoose();
    check(!n.s.nightLight && n.nightLightColor() == 0,
          "ночник можно полностью отключить в настройках");
    n.listChoose();
    check(n.s.nightLight && n.nightLightColor() == 0x482000,
          "включение ночника во время сна запускает новый 10-минутный интервал");
    Save snapshot = n.s;
    snapshot.crc = Game::crc32(&snapshot, offsetof(Save, crc));
    Game restored; memset(&restored.s, 0, sizeof restored.s);
    check(restored.load(snapshot) && restored.nightLightColor() == 0x482000,
          "ночник запускается после перезагрузки при сохранённом сне в темноте");
  }
  // 0.36: обучение дисциплине вместо бессмысленного ревью без каприза.
  {
    Game tr; memset(&tr.s, 0, sizeof tr.s); tr.init(717); tr.demo(0);
    tr.s.form = MIDDLE; tr.s.energy = 90; tr.s.discipline = 40;
    tr.activate(I_REVIEW);
    check(tr.scene == S_TRAIN, "Обучение запускается из значка код-ревью");
    for (int i = 0; i < 5; ++i) {
      bool right = i < 3;
      uint8_t pressed = right ? tr.trainPrompt : (tr.trainPrompt ^ 1u);
      tr.button(pressed ? B_LONG : B_SHORT);
      check(tr.trainPhase == 1, "Тренировка принимает один ответ за раунд");
      tr.tick(4500); tr.render(cv);
    }
    check(tr.scene == S_TRAIN && tr.trainPhase == 2 && tr.trainCorrect == 3,
          "Тренировка длится 22,5 секунды и завершается итогом");
    check(tr.s.discipline == 48 && tr.s.energy == 85,
          "Три верных ответа: дисциплина +8, энергия -5");
    tr.button(B_SHORT);
    check(tr.scene == S_HOME, "После итога можно выйти в комнату");
    tr.activate(I_REVIEW);
    check(tr.scene == S_HOME && tr.s.discipline == 48,
          "Повторное обучение сразу не запускается (30-минутная пауза)");
    tr.tick(30u * 60000u + 1u);
    tr.activate(I_REVIEW);
    check(tr.scene == S_TRAIN, "Обучение доступно после паузы");
    for (int i = 0; i < 5; ++i) {
      tr.button(tr.trainPrompt ? B_LONG : B_SHORT);
      tr.tick(4500); tr.render(cv);
    }
    check(tr.trainCorrect == 5 && tr.s.discipline == 58,
          "Пять правильных ответов: дисциплина +10");
    Game noIdle; memset(&noIdle.s, 0, sizeof noIdle.s); noIdle.init(718); noIdle.demo(0);
    noIdle.s.form = MIDDLE; noIdle.activate(I_REVIEW);
    noIdle.tick(5u * 4500u); noIdle.render(cv);
    check(noIdle.s.discipline == 0, "Бездействие не даёт дисциплину");
    Game cap; memset(&cap.s, 0, sizeof cap.s); cap.init(719); cap.demo(0);
    cap.s.form = MIDDLE; cap.s.discipline = 98; cap.activate(I_REVIEW);
    for (int i = 0; i < 5; ++i) {
      cap.button(cap.trainPrompt ? B_LONG : B_SHORT);
      cap.tick(4500); cap.render(cv);
    }
    check(cap.s.discipline == 100, "Дисциплина ограничена 100");
    Game classicReview; memset(&classicReview.s, 0, sizeof classicReview.s); classicReview.init(720);
    classicReview.demo(0); classicReview.s.form = MIDDLE; classicReview.s.reserved4[0] = 1;
    classicReview.setCall(C_FAKE);
    classicReview.activate(I_REVIEW);
    run(classicReview, 2400); // действие A_REVIEW длится 2300 мс
    check(classicReview.s.discipline == (classicReview.s.trait == T_SMART ? 35 : 25) && classicReview.s.callKind != C_FAKE,
          "Классический ответ на каприз остался без изменений");
  }
  {
    Game tidy; memset(&tidy.s, 0, sizeof tidy.s); tidy.init(721); tidy.demo(0);
    tidy.s.form = MIDDLE; tidy.s.discipline = 100; tidy.s.bugs = 4;
    for (int i = 0; i < 1000 && tidy.s.bugsCleaned == 0; ++i) {
      tidy.now += 60000;
      tidy.minute();
    }
    check(tidy.s.bugsCleaned > 0, "Дисциплинированный Бит самостоятельно убирает баги");
    Game beginner; memset(&beginner.s, 0, sizeof beginner.s); beginner.init(722);
    beginner.demo(0); beginner.s.form = MIDDLE; beginner.s.discipline = 10;
    beginner.s.bugs = 4;
    for (int i = 0; i < 200; ++i) { beginner.now += 60000; beginner.minute(); }
    check(beginner.s.bugsCleaned == 0, "Низкая дисциплина не включает автоуборку");
    Game calm; memset(&calm.s, 0, sizeof calm.s); calm.init(723); calm.demo(0);
    calm.s.form = MIDDLE; calm.s.discipline = 0;
    bool refusal = false;
    for (int i = 0; i < 100; ++i) refusal = calm.refuses() || refusal;
    check(!refusal, "В спокойном режиме Бит не отказывается от еды и игр");
  }
  // 0.37: долгий режим эволюции. Проверяем обе стороны каждой границы.
  {
    check(EVO_EGG_SECONDS == 600u && EVO_BABY_MINUTES == 720u &&
          EVO_JUNIOR_MINUTES == 10080u && EVO_MIDDLE_MINUTES == 14400u &&
          EVO_EXPERT_MINUTES == 20160u,
          "долгая эволюция: 10 минут / 12 часов / 7 / 10 / 14 дней");
    Game egg; memset(&egg.s, 0, sizeof egg.s); egg.init(831);
    egg.tick(599000u);
    check(egg.s.form == EGG && egg.s.stageMin == 599u,
          "яйцо не вылупляется до 10 минут");
    check(egg.screenAllowedOff(), "во время долгого ожидания яйца экран может спать");
    egg.tick(1000u);
    check(egg.s.form == BABY && egg.s.stageMin == 0,
          "яйцо вылупляется ровно через 10 минут");
    check(egg.justEvolved && !egg.screenAllowedOff(),
          "после вылупления экран сам просыпается для анимации");

    Game baby; memset(&baby.s, 0, sizeof baby.s); baby.init(832); baby.demo(0);
    baby.s.form = BABY; baby.s.stageMin = EVO_BABY_MINUTES - 1;
    baby.evolveCheck();
    check(baby.s.form == BABY, "малыш остаётся малышом до 12 часов");
    baby.s.stageMin = EVO_BABY_MINUTES;
    baby.evolveCheck();
    check(baby.s.form == JUNIOR && baby.s.stageMin == 0,
          "через 12 часов малыш превращается в Джуна");

    Game junior; memset(&junior.s, 0, sizeof junior.s); junior.init(833); junior.demo(0);
    junior.s.form = JUNIOR; junior.s.stageMin = EVO_JUNIOR_MINUTES - 1; junior.s.mistakes = 0;
    junior.evolveCheck();
    check(junior.s.form == JUNIOR, "Джун остаётся Джуном до 7 суток");
    junior.s.stageMin = EVO_JUNIOR_MINUTES;
    junior.evolveCheck();
    check(junior.s.form == MIDDLE && junior.s.stageMin == 0,
          "после 7 суток Джун взрослеет, время новой стадии обнуляется");

    Game middle; memset(&middle.s, 0, sizeof middle.s); middle.init(834); middle.demo(0);
    middle.s.form = MIDDLE; middle.s.discipline = 80; middle.s.mistakes = 0;
    middle.s.stageMin = EVO_MIDDLE_MINUTES - 1;
    middle.evolveCheck();
    check(middle.s.form == MIDDLE, "Мидл не взрослеет раньше 10 суток");
    middle.s.stageMin = EVO_MIDDLE_MINUTES;
    middle.evolveCheck();
    check(middle.s.form == ARCHITECT && middle.s.stageMin == 0,
          "Мидл с дисциплиной 80 становится Архитектором через 10 суток");

    Game freelancer; memset(&freelancer.s, 0, sizeof freelancer.s); freelancer.init(835); freelancer.demo(0);
    freelancer.s.form = FREELANCER; freelancer.s.stageMin = EVO_MIDDLE_MINUTES; freelancer.s.mistakes = 0;
    freelancer.evolveCheck();
    check(freelancer.s.form == HACKER, "Фрилансер также взрослеет после 10 суток");

    Game architect; memset(&architect.s, 0, sizeof architect.s); architect.init(836); architect.demo(0);
    architect.s.form = ARCHITECT; architect.s.discipline = 100; architect.s.mistakes = 0;
    architect.s.ageMin = 32u * 1440u;
    architect.s.stageMin = EVO_EXPERT_MINUTES - 1;
    architect.evolveCheck();
    check(architect.s.form == ARCHITECT && architect.s.alive,
          "Архитектор жив и не меняет форму до 14 суток этапа");
    architect.s.stageMin = EVO_EXPERT_MINUTES;
    architect.evolveCheck();
    check(architect.s.form == AI && architect.s.alive,
          "Архитектор становится Нейросетью через 14 суток");

    Game hacker; memset(&hacker.s, 0, sizeof hacker.s); hacker.init(837); hacker.demo(0);
    hacker.s.form = HACKER; hacker.s.discipline = 100; hacker.s.mistakes = 0;
    hacker.s.ageMin = 32u * 1440u; hacker.s.stageMin = EVO_EXPERT_MINUTES;
    hacker.evolveCheck();
    check(hacker.s.form == AI && hacker.s.alive,
          "Хакер тоже может стать Нейросетью и не умирает раньше эволюции");

    // Смерть от старости перенесена вслед за новыми сроками эволюции.
    Game old; memset(&old.s, 0, sizeof old.s); old.init(838); old.demo(0);
    old.s.form = HACKER; old.s.discipline = 50; old.s.stageMin = EVO_EXPERT_MINUTES;
    uint32_t end = (lifeDays(HACKER) + old.s.seed % 3u) * 1440u;
    old.s.ageMin = end - 1;
    old.evolveCheck();
    check(old.s.alive && old.lifeFrac() < 1.0f,
          "Хакер не погибает от старости до нового срока");
    old.s.ageMin = end;
    old.evolveCheck();
    check(!old.s.alive && old.s.causeOfDeath == D_OLD,
          "старость наступает по обновлённому возрастному пределу");
    check(lifeDays(AI) > lifeDays(ARCHITECT) &&
          lifeDays(HACKER) > 32 && lifeDays(LEGACY) > 17 &&
          lifeDays(SENIOR) > 17,
          "все взрослые формы живут дольше новых этапов взросления");
    // Формат Save v4 прежний: stageMin и ageMin сохраняются и при загрузке.
    Save saved = junior.s; saved.crc = Game::crc32(&saved, offsetof(Save, crc));
    Game resumed; memset(&resumed.s, 0, sizeof resumed.s);
    check(resumed.load(saved) && resumed.s.form == MIDDLE &&
          resumed.s.stageMin == 0 && resumed.s.ageMin == saved.ageMin,
          "новые сроки сохраняют совместимость с прежним Save v4");
  }
  // Экран информера: одно короткое нажатие переключает экран, долгое открывает меню.
  {
    Game m; memset(&m.s, 0, sizeof m.s); m.init(1001); m.demo(0);
    check(m.scene == S_HOME && !m.infoScreen && !m.menuOpen &&
          m.menuOffset() == gfx::H - 109,
          "домашний экран питомца по умолчанию, панель скрыта");
    m.render(cv);
    gfx::Color floorAtBottom = fb[(gfx::H - 1) * gfx::W + 100];
    check(floorAtBottom != pal::gutter, "пол комнаты заполняет нижние пиксели");

    // Имитация реальной кнопки платы: пока дисплей спит, событие пробуждения
    // потребляется целиком, а второе нажатие приходит в Game::button().
    m.idleMs = 31000;
    m.wakeToInfo();
    check(m.infoScreen && m.idleMs == 0 && !m.menuOpen &&
          m.scene == S_HOME && m.s.alive && m.s.hunger == 60,
          "первое нажатие при погашенном дисплее показывает часы, не меняя питомца");
    m.render(cv);
    m.button(B_SHORT);
    check(!m.infoScreen && !m.menuOpen && m.scene == S_HOME,
          "второе нажатие после пробуждения показывает питомца");

    m.button(B_SHORT);
    check(m.infoScreen && !m.menuOpen && m.scene == S_HOME,
          "когда экран уже включён, короткое показывает информер");
    m.render(cv);
    m.button(B_SHORT);
    check(!m.infoScreen && !m.menuOpen, "следующее короткое возвращает питомца");

    m.button(B_LONG);
    check(m.menuOpen && m.cursor == 0 && m.menuOffset() == gfx::H - 109,
          "долгое нажатие открывает меню с первого значка");
    m.tick(120);
    int halfway = m.menuOffset();
    check(halfway > 0 && halfway < gfx::H - 109, "меню плавно выезжает");
    m.tick(150);
    check(m.menuOffset() == 0, "меню открыто за 240 мс");
    m.button(B_SHORT);
    check(m.cursor == 1 && !m.infoScreen, "в открытом меню короткое листает");
    m.button(B_LONG);
    check(m.scene == S_LIST && !m.menuOpen && m.cursor == -1,
          "долгое подтверждает выбранный пункт");
    m.button(B_HOLD);
    check(m.scene == S_HOME && !m.infoScreen && !m.menuOpen,
          "удержание возвращает на экран питомца");

    m.button(B_SHORT);
    m.button(B_LONG);
    check(m.menuOpen && !m.infoScreen && m.cursor == 0,
          "с экрана часов долгое открывает меню, не оставляя информер поверх");
    m.button(B_HOLD);
    m.tick(250);
    check(!m.menuOpen && !m.infoScreen, "удержание закрывает меню");
    m.button(B_LONG); m.tick(250);
    m.tick(7000);
    check(!m.menuOpen && m.cursor == -1, "меню закрывается после 7 секунд");
    m.tick(250);
    check(m.menuOffset() == gfx::H - 109, "панель скрыта после бездействия");

    m.button(B_SHORT);
    check(m.infoScreen, "информер можно открыть после скрытия меню");
    Save snap = m.s; snap.crc = Game::crc32(&snap, offsetof(Save, crc));
    Game loaded; memset(&loaded.s, 0, sizeof loaded.s);
    check(loaded.load(snap) && !loaded.infoScreen && !loaded.menuOpen,
          "информер не изменяет Save v4 и сбрасывается при загрузке");
  }
  // 0.40: используются все 135 пикселей, фон, персонаж и мебель единым слоем.
  {
    constexpr int sceneTop = 22, shiftY = 19, floorY = 120;
    bool roomOk = true, sleepOk = true;
    static uint8_t raw[artdata::SCENE_W * artdata::SCENE_H];
    const int xs[] = {0, 17, 235, 239};
    for (int form = EGG; form <= GHOST; ++form) {
      Game roomGame; memset(&roomGame.s, 0, sizeof roomGame.s);
      roomGame.init(1500 + form); roomGame.demo(0);
      roomGame.s.form = form; roomGame.s.asleep = 0; roomGame.s.lightOff = 0;
      roomGame.s.epoch = (roomGame.s.epoch / 86400u) * 86400u + 12 * 3600u;
      roomGame.s.tz = 0;
      const rooms::Room& room = rooms::ROOM[form < GHOST ? form : JUNIOR];
      if (rooms::unpack(room.px, room.pxLen, raw, sizeof raw) != (int)sizeof raw) {
        roomOk = false; continue;
      }
      // Полуденная палитра, k=3: верхняя стена, исходная комната и пол.
      const uint16_t* pal = room.pal + 3 * 256;
      cv.clear(0);
      roomGame.drawBackground(cv, false);
      for (int x : xs) {
        const gfx::Color ceiling = pal[raw[x]];
        const gfx::Color ground = pal[raw[(artdata::SCENE_H - 1) * gfx::W + x]];
        const gfx::Color seam = pal[raw[79 * gfx::W + x]];
        if (fb[sceneTop * gfx::W + x] != ceiling ||
            fb[(sceneTop + shiftY - 1) * gfx::W + x] != ceiling ||
            fb[(sceneTop + shiftY) * gfx::W + x] != ceiling ||
            fb[floorY * gfx::W + x] != seam ||
            fb[(gfx::H - 1) * gfx::W + x] != ground) roomOk = false;
      }
      roomGame.s.asleep = 1;
      roomGame.s.lightOff = 1;
      roomGame.menuOpen = false;
      roomGame.menuTransitionAt = roomGame.now - Game::MENU_ANIM_MS;
      roomGame.render(cv);
      if (roomGame.basePose().y != floorY ||
          roomGame.menuOffset() != gfx::H - 109 ||
          fb[(gfx::H - 1) * gfx::W + 239] == pal::gutter) sleepOk = false;
    }
    check(roomOk, "все 11 комнат: высота 135, стена и пол без пустых полос");
    check(sleepOk, "все формы и сон: персонаж на полу y120, меню не заслоняет комнату");

    Game weather; memset(&weather.s, 0, sizeof weather.s); weather.init(1512);
    weather.demo(0); weather.s.form = JUNIOR; weather.s.tz = 0;
    weather.s.epoch = (weather.s.epoch / 86400u) * 86400u + 12 * 3600u;
    weather.weather = 3; weather.weatherUntil = weather.now + 60000;
    weather.drawBackground(cv, false);
    const rooms::Room& R = rooms::ROOM[JUNIOR];
    int wx = R.win[0] + 2, oldWy = sceneTop + R.win[1] + 2;
    gfx::Color oldBefore = fb[oldWy * gfx::W + wx];
    int movedWy = oldWy + shiftY;
    gfx::Color movedBefore = fb[movedWy * gfx::W + wx];
    weather.drawWeather(cv, false);
    check(fb[movedWy * gfx::W + wx] != movedBefore &&
          fb[oldWy * gfx::W + wx] == oldBefore,
          "погода остаётся внутри сдвинутого окна, не рисуется на старых координатах");
  }
  // Совместимый пакет D|3: поле 54 сообщает, открыт ли экран часов.
  {
    Game board; memset(&board.s, 0, sizeof board.s); board.init(4100); board.demo(0);
    Game phone; memset(&phone.s, 0, sizeof phone.s); phone.init(4101); phone.demo(0);
    board.s.form = JUNIOR; phone.s.form = JUNIOR;
    char packet[640];
    auto sync = [&]() {
      memset(packet, 0, sizeof packet);
      board.command("data", packet, sizeof packet);
      check(strncmp(packet, "D|3|", 4) == 0 && strlen(packet) < sizeof packet - 1,
            "данные помещаются в BLE-буфер");
      std::vector<int32_t> fields;
      const char* p = packet + 4;
      while (*p) {
        char* end = nullptr;
        long long v = strtoll(p, &end, 10);
        if (end == p) break;
        fields.push_back((int32_t)v);
        if (*end != '|') break;
        p = end + 1;
      }
      return fields;
    };
    auto idle = sync();
    check(idle.size() == 55 && idle[50] == 0 && idle[54] == 0,
          "старые 54 поля неизменны, информер передан 55-м");
    phone.mirror(idle.data(), (int)idle.size());
    check(!phone.menuOpen && !phone.infoScreen, "телефон повторяет экран питомца");

    board.button(B_SHORT);
    auto info = sync();
    check(info.size() == 55 && info[54] == 1 && info[50] == 0,
          "в BLE передаётся переключение на часы и погоду");
    phone.mirror(info.data(), (int)info.size());
    check(phone.infoScreen && !phone.menuOpen, "зеркало телефона показывает информер");
    board.button(B_SHORT);
    auto home = sync();
    phone.mirror(home.data(), (int)home.size());
    check(!phone.infoScreen, "зеркало возвращается на экран питомца");

    board.button(B_LONG); board.tick(90);
    auto opened = sync();
    check(opened.size() == 55 && opened[50] == 1 && opened[51] == 0 &&
          opened[52] == 90 && opened[53] == 90 && opened[54] == 0,
          "открытое меню передаёт положение и выбранный значок");
    phone.mirror(opened.data(), (int)opened.size());
    check(phone.menuOpen && phone.cursor == board.cursor &&
          phone.menuOffset() == board.menuOffset() && !phone.infoScreen,
          "зеркало повторяет открытие меню");
    board.button(B_SHORT); board.tick(180);
    auto selected = sync();
    phone.mirror(selected.data(), (int)selected.size());
    check(phone.menuOpen && phone.cursor == 1 && phone.menuOffset() == board.menuOffset(),
          "короткое в меню передаёт выбор следующего действия");
    board.button(B_HOLD); board.tick(100);
    auto closed = sync();
    phone.mirror(closed.data(), (int)closed.size());
    check(!phone.menuOpen && !phone.infoScreen && phone.menuOffset() == board.menuOffset(),
          "удержание закрывает меню синхронно");
    board.tick(180); phone.tick(180);
    check(board.menuOffset() == phone.menuOffset(), "анимация закрытия синхронна");

    board.button(B_LONG); board.tick(250);
    auto legacy = sync();
    phone.mirror(legacy.data(), 50);
    check(!phone.menuOpen && !phone.infoScreen && phone.cursor == -1,
          "старый Bluetooth-пакет из 50 полей поддерживается");
    phone.mirror(legacy.data(), 54);
    check(phone.menuOpen && !phone.infoScreen,
          "пакет 0.41 из 54 полей не показывает фиктивный информер");
    board.tick(7000); phone.tick(7000);
    check(!board.menuOpen && !phone.menuOpen, "автоскрытие работает на обоих экранах");
  }
  printf(fails ? "\nОшибок: %d\n" : "\nВсё верно\n", fails);
  return fails != 0;
}
