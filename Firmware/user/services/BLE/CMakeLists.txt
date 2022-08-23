idf_component_register(SRCS "ble_dwm1001.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash bt print_log kalman_filter user_button ble_nvs buzzer)