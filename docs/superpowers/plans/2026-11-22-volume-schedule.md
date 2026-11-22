# Volume Schedule Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** гучність DFPlayer за розкладом (дні тижня + інтервали, редагується в адмінці без перезавантаження) і кнопка перевірки гучності.

**Architecture:** чиста логіка розкладу (вибір гучності, валідація, JSON) у `volume_calc.c`, тестується на хості. `volume.c` тримає розклад у RAM під mutex і в NVS. `notify.c` виставляє гучність перед кожним треком і раз на хвилину під час відтворення; новий тип команди для тесту. `admin.c` отримує `GET/POST /volume`, `POST /volume/test`, поле `volume` у `/status`.

**Tech Stack:** ESP-IDF v6.1.1, C11, cJSON, NVS, esp_http_server, host-тести `tests/host` (minitest).

**Spec:** `docs/superpowers/specs/2026-11-22-volume-schedule-design.md`

## Global Constraints

- Гучність 0..30; ліміт 64 записи; `from`/`to` хвилини 0..1439, обидві межі включно, `from <= to`; `days` маска біт 0 = пн … біт 6 = нд, не 0.
- Кілька збігів → найтихіший. Немає збігу або час не синхронізовано (`clock_now() == 0`) → `def`.
- Початкове `def` = `CONFIG_AA_DF_VOLUME`. Зберігання: NVS namespace `aa`, ключ `vsched` (тоді «Скинути налаштування»/BOOT 5 с стирає і розклад).
- Збереження розкладу діє одразу, без перезавантаження.
- Тест: 10 с, потім стоп і гучність за розкладом; відхиляється з 409, якщо щось грає; будь-який справжній звук перериває тест; BOOT зупиняє.
- `dfplayer.*` не змінювати. Один модуль = пара файлів, `TAG`, `esp_err_t`. Коментарі в одному файлі однією мовою (у наявних файлах англійською).
- `idf.py build` без помилок і без нових warning-ів у `main/`; `make -C tests/host test` зелений.

## Review Focus

1. Неділя: `tm_wday == 0` має відповідати біту 6, понеділок `tm_wday == 1` біту 0 — тест `sunday_maps_to_bit6` у Task 1.
2. Хвилина 23:59 у записі `00:00–23:59` і хвилина `to` взагалі входять в інтервал — тест `to_is_inclusive` у Task 1.
3. Неправильний POST (`from > to`, `"25:00"`, `vol: 31`, `days: 0`, 65 записів, не-JSON, відсутнє поле) → 400 і нічого не збережено — тести `rejects_*` у Task 1 (парсер не змінює `out` при помилці).
4. 64 записи з максимальною довжиною вміщаються в буфер тіла POST — тест `max_sched_json_fits` у Task 1 (довжина `< VOLUME_JSON_MAX`).
5. Пошкоджений blob у NVS (інша версія/розмір, `n > 64`, невалідний запис) → порожній розклад з `def = CONFIG_AA_DF_VOLUME`, `ESP_LOGW` — валідація через `volume_sched_valid()` (тест `sched_valid_*` у Task 1), перевірка вручну в Task 2.

---

### Task 1: Pure schedule logic (`volume_calc.c`)

**Files:**
- Create: `main/volume.h`, `main/volume_calc.c`
- Create: `tests/host/test_volume.c`
- Modify: `tests/host/Makefile` (додати `SRCS_test_volume := $(MAIN)/volume_calc.c $(CJSON)/cJSON.c`)

**Interfaces — Produces (у `main/volume.h`):**
```c
#define VOLUME_MAX        30
#define VOLUME_RULES_MAX  64
#define VOLUME_JSON_MAX   6144   /* max body of POST /volume */

typedef struct { uint8_t days; uint16_t from; uint16_t to; uint8_t vol; } volume_rule_t;
typedef struct { uint8_t def; uint8_t n; volume_rule_t r[VOLUME_RULES_MAX]; } volume_sched_t;

/* Pure, volume_calc.c */
bool volume_rule_valid(const volume_rule_t *r);
bool volume_sched_valid(const volume_sched_t *s);           /* def, n, every rule */
int  volume_pick(const volume_sched_t *s, int wday_mon0, int minute); /* wday_mon0 0=Mon..6=Sun */
int  volume_at(const volume_sched_t *s, time_t now);        /* now==0 -> s->def; localtime_r */
/* Parses {"default":N,"rules":[{"days":M,"from":"HH:MM","to":"HH:MM","vol":V}]}.
 * On failure returns false, writes a Ukrainian message to err, leaves *out untouched. */
bool volume_sched_from_json(const char *txt, volume_sched_t *out, char *err, size_t errlen);
/* {"default":N,"max":64,"rules":[...]} with "HH:MM" strings; malloc'ed, caller frees. */
char *volume_sched_to_json(const volume_sched_t *s);
```
(Task 2 додає в цей самий header декларації `volume.c`.)

- [ ] **Step 1: Write failing tests** у `tests/host/test_volume.c`:
  - `empty_gives_default`: `{def=20,n=0}` → `volume_pick(...,0,600) == 20`.
  - `match_inside`: правило пн–пт (`days=0x1F`) 13:00–15:00 vol 5 → пн 14:00 = 5, сб 14:00 = 20.
  - `to_is_inclusive`: правило `0..1439` vol 3 → хвилина 1439 = 3; правило 780..900 → хвилина 900 = 5, 901 = def.
  - `quietest_wins`: два правила, що перетинаються (vol 10 і 4) → 4.
  - `sunday_maps_to_bit6`: правило `days=0x40` vol 2; `volume_at` для `kyiv(2026,11,22,10,0,0)` (неділя) = 2, для `kyiv(2026,11,23,10,0,0)` (понеділок) = def. Хелпер `kyiv()` як у `test_clock.c`.
  - `unsynced_gives_default`: `volume_at(s, 0) == s->def`.
  - `rule_valid_*`: невалідні `days=0`, `days=0x80`, `from=900,to=780`, `to=1440`, `vol=31`.
  - `sched_valid_*`: `def=31` або `n=65` → false.
  - `json_roundtrip`: `to_json` → `from_json` дає ідентичну структуру (поле за полем); `"from":"09:05"` ↔ 545.
  - `rejects_*` (кожен перевіряє `false`, непорожній `err` і що `out` не змінився — заповнити `out` маркером `def=7` до виклику): `"from":"25:00"`, `"from":"9:5x"`, `from>to`, `"vol":31`, `"days":0`, 65 правил, `"not json"`, відсутнє `"default"`.
  - `max_sched_json_fits`: 64 правила `days=0x7F,from=0,to=1439,vol=30`, `def=30` → `strlen(to_json) < VOLUME_JSON_MAX`.

- [ ] **Step 2:** `make -C tests/host test` → FAIL (немає `volume.h`/`volume_calc.c`).

- [ ] **Step 3: Implement `main/volume_calc.c`** (без ESP-заголовків, як `clock_calc.c`). Час `HH:MM` парсити строго: рівно 5 символів, `HH` 00..23, `MM` 00..59. `volume_at`: `localtime_r`, `wday_mon0 = (tm_wday + 6) % 7`, `minute = tm_hour*60 + tm_min`. Повідомлення `err` українською, з номером рядка (1-based), напр. `"Рядок 3: «від» пізніше за «до»"`.

- [ ] **Step 4:** `make -C tests/host test` → усі PASS.

- [ ] **Step 5: Commit** `Add pure volume schedule logic with host tests`.

### Task 2: Schedule storage and volume during playback

**Files:**
- Create: `main/volume.c`
- Modify: `main/volume.h`, `main/CMakeLists.txt` (додати `"volume.c" "volume_calc.c"`), `main/main.c` (виклик `volume_init()` перед `notify_init()`), `main/notify.c`

**Interfaces:**
- Consumes: Task 1.
- Produces (у `main/volume.h`):
```c
esp_err_t volume_init(void);                /* load NVS "aa"/"vsched"; bad/missing -> {def=CONFIG_AA_DF_VOLUME, n=0} */
int       volume_now(void);                 /* volume_at(current, clock_now()) under mutex */
void      volume_get(volume_sched_t *out);  /* copy under mutex */
esp_err_t volume_set(const volume_sched_t *s); /* !volume_sched_valid -> ESP_ERR_INVALID_ARG; NVS then RAM */
```

- [ ] **Step 1: Implement `volume.c`.** Blob = `struct { uint8_t ver; volume_sched_t s; }`, `ver = 1`. При читанні: розмір ≠ `sizeof`, `ver ≠ 1` або `!volume_sched_valid` → дефолт + `ESP_LOGW(TAG, "stored schedule unusable, using default")`. Mutex створюється в `volume_init`. `volume_set` пише NVS (`nvs_set_blob` + `nvs_commit`), лише при успіху оновлює RAM, `ESP_LOGI` з кількістю правил і `def`.

- [ ] **Step 2: `notify.c`.** Замінити `dfplayer_set_volume(CONFIG_AA_DF_VOLUME)` на `volume_now()`. Додати `static int s_cur_vol = -1;` і хелпер `static void apply_volume(int v)` (шле `dfplayer_set_volume` лише якщо `v != s_cur_vol`, `ESP_LOGI` «volume N»). Перед кожним `dfplayer_play_track` у `play_body` → `apply_volume(volume_now())`. У циклі очікування: запам'ятати `clock_now() / 60`; коли змінилась → `apply_volume(volume_now())`.

- [ ] **Step 3:** `. ~/esp/esp-idf/export.sh && idf.py build` → без помилок і нових warning-ів у `main/`. `make -C tests/host test` → PASS.

- [ ] **Step 4: Commit** `Apply scheduled volume before and during playback`.

### Task 3: Volume test command

**Files:**
- Modify: `main/notify.h`, `main/notify.c`

**Interfaces — Produces:**
```c
/* Play `track` (1..3) at `vol` (0..30) for 10 s, then stop and restore the
 * scheduled volume. false if a sound is playing now (nothing queued). */
bool notify_test_volume(int track, int vol);
```

- [ ] **Step 1: Implement.** Новий `CMD_TEST` у `cmd_type_t`, поле `int vol` у `cmd_t`. `notify_test_volume`: перевірка діапазонів (false), `if (s_playing) return false;`, `post(...)`. `#define TEST_PLAY_MS 10000`. `play_body` отримує параметр `int test_vol` (`-1` = звичайне відтворення). У режимі тесту: `dfplayer_set_volume(test_vol)` і `s_cur_vol = test_vol` замість розкладу, щохвилинна перевірка не діє, ліміт `TEST_PLAY_MS` замість `TRACK_MAX_MS`, будь-який `next_track != 0` (з `CMD_TRANSITION`, `CMD_PLAY` чи іншого `CMD_TEST`) → `SOUND_INTERRUPT` без `notify_sound_decision`. Після виходу з тесту з будь-якої причини: `dfplayer_stop()` (якщо ще грає) і `apply_volume(volume_now())`. `play(track, repeats)` лишається обгорткою з `test_vol = -1`; цикл задачі обробляє `CMD_TEST` через `s_playing = true; play_body(c.track, 1, c.vol); s_playing = false;` (або розширена `play`). `CMD_TEST`, отриманий під час звичайного звуку в `play_body`, відкидається з `ESP_LOGI`.

- [ ] **Step 2:** `idf.py build` → без помилок/нових warning-ів.

- [ ] **Step 3: Commit** `Add timed volume test playback`.

### Task 4: Admin API

**Files:**
- Modify: `main/admin.c`

**Interfaces — Consumes:** `volume_get`, `volume_set`, `volume_now`, `volume_sched_to_json`, `volume_sched_from_json`, `VOLUME_JSON_MAX` (Tasks 1–2), `notify_test_volume` (Task 3).

- [ ] **Step 1: Implement handlers** (усі з `authorized()`):
  - `GET /volume` → `volume_sched_to_json` поточного, `application/json`.
  - `POST /volume`: `content_len` 0 або `>= VOLUME_JSON_MAX` → 400 `"bad body"`; тіло читати в `malloc(VOLUME_JSON_MAX)` (цикл `httpd_req_recv`, як у `h_save`); `from_json` помилка → 400 з текстом `err` (`text/plain; charset=utf-8`); `volume_set` ≠ OK → 500; успіх → `"ok"`. Без `restart_soon()`.
  - `POST /volume/test?track=T&vol=V`: T 1..3, V 0..30, інакше 400 `"track 1..3, vol 0..30"`; `notify_test_volume` false → статус `409 Conflict`, тіло `"Зараз грає звук"`; інакше `"ok"`.
  - `/status`: `cJSON_AddNumberToObject(o, "volume", volume_now())`.
  - Зареєструвати 3 маршрути (разом 14 обробників ≤ `max_uri_handlers = 16`). Шлях `/volume/test` реєструвати як окремий URI (точний збіг, без wildcard).

- [ ] **Step 2:** `idf.py build` → OK. Прошити основну плату (порт через `/dev/serial/by-id`, див. пам'ять: основна плата AC:A7:04:BD:FC:40) і перевірити:
  - `curl -u admin:PASS http://<ip>/volume` → `{"default":25,"max":64,"rules":[]}`.
  - `curl -u admin:PASS -X POST --data '{"default":20,"rules":[{"days":31,"from":"13:00","to":"15:00","vol":5}]}' http://<ip>/volume` → `ok`; повторний GET повертає те саме; після RESET теж те саме.
  - Той самий POST з `"to":"12:00"` → 400 з українським повідомленням; GET не змінився.
  - `curl ... -X POST 'http://<ip>/volume/test?track=2&vol=10'` → `ok`, звук 10 с; повторний запит під час звуку → 409.

- [ ] **Step 3: Commit** `Add volume schedule and volume test endpoints`.

### Task 5: Admin page section

**Files:**
- Modify: `main/admin_page.h`

- [ ] **Step 1: Implement** секцію `<h2>Гучність</h2>` перед «Перевірка звуку»:
  - «Гучність за замовчуванням (0–30)» `<input type=number min=0 max=30 id=vdef>`.
  - Таблиця в `.tw`: стовпці `Пн Вт Ср Чт Пт Сб Нд` (checkbox), `Від` і `До` (`<input type=time>`), `Гучність` (number 0–30), кнопка `✕`. Кнопки «Додати рядок» (новий рядок: пн–пт, 13:00–15:00, 5; блокується при 64 рядках) і «Зберегти розклад» → `POST /volume` JSON; показати відповідь сервера (текст помилки 400 як є) у `<small id=vr>`.
  - Підказка `<small>`: «Межі включно; 00:00–23:59 — весь день. Через північ — двома рядками. Якщо рядки перетинаються, діє найтихіший.»
  - Перевірка: number `id=tv` (0–30, стартове значення з `default`), `<select id=tt>` (1 Тривога, 2 Відбій, 3 09:00), кнопка «Перевірити гучність (10 с)» → `POST /volume/test?track=..&vol=..`; 409 → «Зараз грає звук».
  - У блоці стану додати `· Гучність: N`.
  - Завантаження `GET /volume` при відкритті сторінки.

- [ ] **Step 2:** `idf.py build`, прошити, відкрити сторінку в браузері: додати 2 рядки, зберегти, оновити сторінку — рядки на місці; рядок з «від» > «до» дає червоне повідомлення з номером рядка; тест на гучності 5 і 25 чутно відрізняється.

- [ ] **Step 3: Commit** `Add volume schedule section to admin page`.

### Task 6: Docs and on-device check

**Files:**
- Modify: `CLAUDE.md` (у розділі структури додати `volume.*`: розклад гучності, NVS `aa/vsched`, гучність виставляє `notify` перед треком і щохвилини), `README.md` (як налаштувати розклад в адмінці, якщо там описано адмінку).

- [ ] **Step 1:** Оновити документи.
- [ ] **Step 2:** Перевірка на платі: правило на поточну хвилину+1 з vol 3, запустити довгий трек через «Тест: 09:00» до межі; у моніторі після зміни хвилини з'являється `volume 3`, звук тихішає. Після «Скинути налаштування» розклад порожній, `default` = 25.
- [ ] **Step 3: Commit** `Document volume schedule`.
