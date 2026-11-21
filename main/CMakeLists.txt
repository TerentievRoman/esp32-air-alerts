idf_component_register(SRCS "notify.c" "admin_form.c" "dns_server.c" "main.c" "alert_state.c" "dfplayer.c" "settings.c" "led.c" "button.c" "wifi.c" "admin.c" "admin_form.c" "dns_server.c" "notify.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash esp_wifi esp_http_server esp_netif
                                     esp_timer esp_driver_gpio esp_driver_uart)
