idf_component_register(SRCS "main.c" "alert_state.c" "dfplayer.c" "settings.c" "led.c" "button.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash esp_wifi esp_http_server esp_netif
                                     esp_timer esp_driver_gpio esp_driver_uart)
