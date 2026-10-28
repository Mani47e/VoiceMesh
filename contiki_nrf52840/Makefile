CONTIKI_PROJECT = main

TARGET ?= nrf52840
BOARD  ?= dongle

# 1. Set the default Make target to the hex file
all: $(CONTIKI_PROJECT).hex

CONTIKI = /Users/mani/Scrap/contiki-ng

# 2. Include Contiki-NG core rules
include $(CONTIKI)/Makefile.include

# To generate the hex file from the compiled ELF
$(CONTIKI_PROJECT).hex: $(BUILD_DIR_BOARD)/$(CONTIKI_PROJECT).$(TARGET)
	arm-none-eabi-objcopy -O ihex $< $@


CLEAN += $(CONTIKI_PROJECT).hex