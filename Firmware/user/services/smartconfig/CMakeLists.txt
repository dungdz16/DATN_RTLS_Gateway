idf_component_register(SRCS "smartconfig.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi wpa_supplicant nvs_store esp_http_server syslog print_log ws2812b)
