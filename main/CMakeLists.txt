idf_component_register(SRCS "main.c"
                            "admin.c" "admin_form.c" "dns_server.c"
                            "alert_log.c" "alert_log_buf.c" "alert_state.c"
                            "button.c" "button_gesture.c" "clock.c" "clock_calc.c"
                            "dfplayer.c" "font5x8.c" "led.c" "mqtt.c" "notify.c" "notify_rules.c"
                            "screen_fmt.c" "settings.c" "silence.c" "wifi.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash esp_wifi esp_http_server esp_netif
                                     esp_timer esp_driver_gpio esp_driver_uart)
