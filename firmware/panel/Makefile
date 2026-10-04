all : build

TARGET := panel
TARGET_MCU ?= CH32V003
CH32FUN ?= ../../ch32fun/ch32fun
EXTRA_CFLAGS += -I$(CH32FUN)/../extralibs

include $(CH32FUN)/ch32fun.mk

flash : cv_flash
clean : cv_clean

# Erstes Programmieren eines neuen Panels mit dem WCH-LinkE:
# Bootloader in den Boot-Bereich, Option-Byte "beim Einschalten im Boot-Bereich starten", dann die Firmware.
# Danach kommen alle weiteren Updates über den Bus vom Hauptpanel.
flashall : build
	$(MAKE) -C bootloader build CH32FUN=$(abspath $(CH32FUN))
	$(MINICHLINK)/minichlink -a -U -w bootloader/bootloader.bin bootloader -B
	$(MINICHLINK)/minichlink -w +a55aef10 option
	$(MINICHLINK)/minichlink -w $(TARGET).bin flash -b
