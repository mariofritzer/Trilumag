all : build

TARGET := panel
TARGET_MCU ?= CH32V003
CH32FUN ?= ../../ch32fun/ch32fun
EXTRA_CFLAGS += -I$(CH32FUN)/../extralibs

include $(CH32FUN)/ch32fun.mk

flash : cv_flash
clean : cv_clean
