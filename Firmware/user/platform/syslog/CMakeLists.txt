idf_component_register(SRCS "syslog.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash spi_flash print_log)
