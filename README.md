# STM32 Blackpill Baremetal Application
This is a bare metal application for the STM32F411CE "Blackpill" dev board.

The intention for this software is to create a simple serial device that can
then be reverse engineered to run on a unicorn emulator.

## Zig
I have been trying to build the baremetal C application with zig's build system `build.zig`

## Linker
The Zig linker (LLD) and the GCC linker (LD) don't behave in the same way. I
had to explicitly specify the `.stack` and `.bss` sections as `NOLOAD` for LLD,
otherwise objcopy would include them in the binary (something gcc wouldn't do)

## RTOS
The RTOS implementation follows miro samek's
[modern embedded systems programming course](https://www.youtube.com/playlist?list=PLPW8O6W-1chwyTzI3BHwBLbGQoPFxPAPM)

### Links
- https://github.com/haydenridd/stm32-zig-porting-guide
- https://github.com/the-argus/zig-buildsystem-docs
- https://github.com/tralamazza/embedded_zig
- https://ziglang.org/learn/build-system
- https://github.com/haydenridd/gcc-arm-to-zig
