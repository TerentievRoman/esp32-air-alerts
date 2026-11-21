# air_alert — сповіщувач повітряної тривоги (ESP32-C3 SuperMini)

Прошивка на ESP-IDF v6.1 для плати ESP32-C3 SuperMini.

## Збірка та прошивка

```bash
. ~/esp/esp-idf/export.sh      # або alias get_idf
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Плата підключається через native USB (USB Serial/JTAG), тому порт зазвичай `/dev/ttyACM0`.
Якщо порт не з'являється — затиснути BOOT, натиснути RESET, відпустити BOOT.

## Структура

- `main/main.c` — точка входу, ініціалізація NVS, головний цикл
- `main/alert_state.*` — стан тривоги (UNKNOWN / CLEAR / ACTIVE)
- `partitions.csv` — таблиця розділів (4 MB flash, SPIFFS для налаштувань/сертифікатів)
- `sdkconfig.defaults` — базові налаштування під SuperMini

## TODO

- Wi-Fi: підключення та збереження креденшалів
- Джерело даних: API тривог (регіон задається у конфігурації)
- Індикація: LED / зумер / дисплей
