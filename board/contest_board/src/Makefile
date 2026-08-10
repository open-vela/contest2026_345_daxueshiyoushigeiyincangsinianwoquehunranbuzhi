############################################################################
# Contest 2026 team 345 ESP32-P4X Function EV Board
############################################################################

include $(TOPDIR)/Make.defs

CSRCS = board_boot.c

ifeq ($(CONFIG_DEV_GPIO),y)
ifneq ($(CONFIG_GPIO_LOWER_HALF),y)
CSRCS += board_gpio.c
endif
endif

include $(TOPDIR)/boards/Board.mk
