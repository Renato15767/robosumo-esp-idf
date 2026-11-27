idf_component_register(
    SRCS "main.c" "hardware.c" "controle.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_ledc esp_timer esp_rom
)
