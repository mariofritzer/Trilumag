all : build

TARGET := bootloader
TARGET_MCU ?= CH32V003
CH32FUN ?= ../../../ch32fun/ch32fun
LINKER_SCRIPT := $(CH32FUN)/ch32v003fun-bootloader.ld
WRITE_SECTION := bootloader

include $(CH32FUN)/ch32fun.mk

flash : cv_flash
clean : cv_clean
