// gpio_blink.cpp — Blink the lowest bit of a Xilinx AXI GPIO at 1 Hz
//
// Device tree should expose the GPIO as a UIO device, e.g.:
//   gpio0: gpio@41200000 {
//       compatible = "trenz.biz,smartio-1.0";
//       reg = <0x41200000 0x1000>;
//   };

#include <cstdio>
#include <cstdint>
#include <csignal>
#include <unistd.h>

#include "smart/UioDevice.h"

// Xilinx AXI GPIO register indices (32-bit word addressing)
enum Reg : uint32_t {
    GPIO_DATA  = 0,  // Channel 1 Data
    GPIO_TRI   = 1,  // Channel 1 Tri-state (0 = output)
    GPIO2_DATA = 2,  // Channel 2 Data
    GPIO2_TRI  = 3,  // Channel 2 Tri-state
};

static volatile sig_atomic_t running = 1;

static void sighandler(int) { running = 0; }

int main(int argc, char* argv[])
{
    const char* dev = argc > 1 ? argv[1] : "gpio";

    smart::UioDevice uio(dev);
    smart::MappedFile* regs = uio.getRequiredMap(0);

    // Configure bit 0 as output
    uint32_t tri = regs->read32(GPIO_TRI);
    regs->write32(GPIO_TRI, tri & ~1u);

    std::fprintf(stderr, "gpio_blink: %s bit 0, 1 Hz (ctrl-C to quit)\n", dev);

    std::signal(SIGINT, sighandler);
    std::signal(SIGTERM, sighandler);

    uint32_t state = 0;
    while (running) {
        state ^= 1;
        regs->write32(GPIO_DATA, (regs->read32(GPIO_DATA) & ~1u) | state);
        usleep(500'000);  // 500 ms half-period → 1 Hz
    }

    // Turn off on exit
    regs->write32(GPIO_DATA, regs->read32(GPIO_DATA) & ~1u);
    std::fprintf(stderr, "\ngpio_blink: exiting\n");
    return 0;
}
