# Contest 2026 team 345 ESP32-P4X Function EV Board
nuttx_add_library(board)
target_sources(board PRIVATE board_boot.c)

if(CONFIG_DEV_GPIO AND NOT CONFIG_GPIO_LOWER_HALF)
  target_sources(board PRIVATE board_gpio.c)
endif()
