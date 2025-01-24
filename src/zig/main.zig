const c = @cImport({
    @cInclude("hal/systick.h");
    @cInclude("utils/debug.h");
});

const p = @import("hal/pinutils.zig");
const uart = @import("hal/uart.zig").Uart;
const gpio = @import("hal/gpio.zig");

export fn zig_main() void {
    // const led: u16 = comptime p.Bank.C.pin(13);
    // _ = comptime p.Bank.bankFromPin(led);
    // gpio.setMode(led, .output);

    // c.systick_init(c.CLOCK_FREQ / 1000);

    var timer: u32 = 0;
    const period: u32 = 250;
    // uart.init(.uart1, 9600);
    var on: bool = true;
    uart.write(.uart1, "Booting\r\n");

    while (true) {
        if (c.systick_timer_expired(&timer, period, c.systick_get_ticks())) {
            // gpio.write(led, on);
            on = !on;
            uart.write(.uart1, if (on) "zick\r\n" else "zock\r\n");
        }
    }

    while (true) {}
}
