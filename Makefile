
# CFLAGS and LDFLAGS used in build.zig
CFLAGS  ?=  -W -Wall -Wextra -Werror -Wundef -Wshadow -Wdouble-promotion \
            -Wformat-truncation -fno-common -Wconversion \
            -g3 -Os -ffunction-sections -fdata-sections -I. \
            -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 $(EXTRA_CFLAGS)
LDFLAGS ?= -Tstm32f411xx.ld -nostartfiles -nostdlib --specs nano.specs -lc -lgcc -Wl,--gc-sections -Wl,-Map=$@.map

SRC := src
INC := inc
TARGET := zig-out/bin/firmware

ZIG_SOURCES := $(shell find $(SRC) -name '*.zig')
C_SOURCES := $(shell find $(SRC) -name '*.c')
C_HEADERS := $(shell find $(INC) -name '*.h')

.PHONY: build dump clean flash

build: $(TARGET).bin

$(TARGET).bin: $(TARGET).elf

$(TARGET).elf: $(ZIG_SOURCES) $(C_SOURCES) $(C_HEADERS)
	@zig build

flash: $(TARGET).bin
	st-flash --reset write $< 0x8000000

dump: $(TARGET).elf
	$(OBJDUMP) -dh $<

clean:
	rm -rf zig-out .zig-cache
