# Ретранслятор вебхука → MQTT (PHP, shared-хостинг)

Приймає вебхук Порталу даних Києва про початок/кінець повітряної тривоги
і публікує retained-повідомлення в MQTT-брокер, на який підписана плата.
Без composer, без демонів, працює на звичайному PHP-хостингу.

## Файли

| Файл | Призначення |
|---|---|
| `hook.php` | приймач вебхука (секрет → лог → мапінг → публікація) |
| `selftest.php` | перевірка, що хостинг випускає TLS і креденшали брокера вірні |
| `hook_lib.php` | логіка приймача (`hook_handle`), тестується без мережі |
| `map.php` | мапінг тіла вебхука на `RED` / `YELLOW` / `CLEAR` |
| `mqtt_publish.php` | мінімальний MQTT 3.1.1 publisher поверх TLS |
| `config.example.php` | зразок конфігу; робочий `config.php` не комітиться |
| `.htaccess` | закриває прямий доступ до конфігу, логів і тестів |
| `log/` | сирі запити вебхука, `raw-YYYYMM.log` |
| `tests/run.php` | тести: `php relay/tests/run.php` |

## Вимоги до хостингу

- PHP ≥ 7.4 з розширеннями `openssl` і `json`; `mbstring` бажано (без нього
  працює фолбек для кирилиці).
- Вихідні TLS-з'єднання на порт брокера (8883). Перевіряється `selftest.php`.
- Apache з `AllowOverride` хоча б `Limit`/`AuthConfig`, щоб `.htaccess`
  закривав конфіг і логи. Якщо `.htaccess` ігнорується, перенести `log/`
  і `config.php` поза `public_html` і поправити шляхи в `hook.php`.

## Розгортання

1. Брокер: EMQX Cloud Serverless (console: https://cloud-intl.emqx.com/),
   деплоймент `air-alert`, Access Control → Authentication → користувач
   `relay`. Записати host (`xxxxxxxx.ala.eu-central-1.emqxsl.com`), port
   8883, логін, пароль. Spend Limit тримати 0, щоб ніколи не було списань.
2. Залити теку `relay/` на хостинг (без `tests/`). Робоче розгортання:
   корінь піддомену `https://alerts.yomochka.pp.ua/`, тому далі в прикладах
   `https://<домен>/air_alert/...` читати як `https://alerts.yomochka.pp.ua/...`.
3. На хостингу скопіювати `config.example.php` → `config.php`, вписати
   брокер і секрет. Секрет згенерувати:
   ```bash
   php -r 'echo bin2hex(random_bytes(16)), PHP_EOL;'
   ```
4. Дати `log/` права на запис для PHP (зазвичай 750 або 755 достатньо,
   бо PHP працює від власника акаунта).
5. Чекліст після заливки (усі чотири команди, не лише перша):
   ```bash
   curl "https://<домен>/air_alert/selftest.php?key=<secret>"
   ```
   Очікувано `{"ok":true,"published":true,"log_writable":true,"map_ok":true,...}`.
   `published:false` з `ms` близько таймауту означає, що хостинг не випускає
   вихідні з'єднання на порт 8883: спитати підтримку або перейти на запасний
   варіант зі специфікації (HTTP-API того ж EMQX на порту 8443).
   `log_writable:false`: дати права на `log/`.
   ```bash
   curl -o /dev/null -w "%{http_code}\n" "https://<домен>/air_alert/hook.php"
   ```
   Очікувано `403` (не `500`: `500` означає, що `.htaccess` не сумісний з
   Apache хостингу).
   ```bash
   curl -o /dev/null -w "%{http_code}\n" "https://<домен>/air_alert/config.php"
   curl -o /dev/null -w "%{http_code}\n" "https://<домен>/air_alert/log/raw-$(date +%Y%m).log"
   ```
   Обидва `403`. Якщо `200`, конфіг і логи відкриті назовні: див. вимоги вище.
6. Зареєструвати на Порталі даних Києва URL вебхука:
   `https://<домен>/air_alert/hook.php?key=<secret>`
7. Після першої реальної тривоги подивитись `log/raw-YYYYMM.log` **двічі**:
   після початку і після відбою. Кожен рядок закінчується результатом
   (`-> RED published`, `-> CLEAR published`, `-> unmapped`,
   `-> publish failed`). Якщо відбій дав `unmapped`, у брокері назавжди
   лишиться RED: доповнити `map.php` під реальний формат (тести в
   `tests/map_tests.php`).
8. Секрет у URL потрапляє в access-логи хостингу. Якщо Портал дозволяє
   задати заголовок, краще передавати його як `X-Relay-Key` і реєструвати
   URL без `?key=`. Секрет міняти, якщо логи доступу комусь передавались.

## Відповіді hook.php

| Код | Коли |
|---|---|
| 200 | стан розпізнано й опубліковано, тіло `{"ok":true,"state":"RED"}` |
| 202 | тіло не розпізнано, записано в лог, у брокер нічого не пішло |
| 403 | немає або невірний секрет, у лог не пишеться |
| 502 | брокер не прийняв публікацію |
| 500 | `config.php` відсутній або без `secret`/`topic`/`mqtt` |

## Ручна публікація без вебхука

```bash
curl -X POST "https://<домен>/air_alert/hook.php?key=<secret>" \
  -H 'Content-Type: application/json' -d '{"status":"start"}'
```
