// uart_terminal.cpp — Simple serial terminal for a 16550 UART via UIO
//
// Device tree should expose the UART as a UIO device, e.g.:
//   uart0: serial@43c00000 {
//       compatible = "trenz.biz,smartio-1.0";
//       reg = <0x43c00000 0x1000>;
//       interrupts = <0 29 4>;
//   };

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <poll.h>
#include <termios.h>

#include "smart/UioDevice.h"

// 16550 register indices (32-bit word addressing)
enum Reg : uint32_t {
    RBR = 0,  // Receive Buffer (read)
    THR = 0,  // Transmit Holding (write)
    IER = 1,  // Interrupt Enable
    IIR = 2,  // Interrupt Identification (read)
    FCR = 2,  // FIFO Control (write)
    LCR = 3,  // Line Control
    MCR = 4,  // Modem Control
    LSR = 5,  // Line Status
    MSR = 6,  // Modem Status
    SCR = 7,  // Scratch
    DLL = 0,  // Divisor Latch Low  (LCR.DLAB=1)
    DLM = 1,  // Divisor Latch High (LCR.DLAB=1)
};

// LSR bits
constexpr uint32_t LSR_DR   = 0x01;  // Data Ready
constexpr uint32_t LSR_THRE = 0x20;  // TX Holding Register Empty

// IER bits
constexpr uint32_t IER_RDI  = 0x01;  // Receive Data Available

static void set_baud(smart::MappedFile* regs, uint32_t clk_hz, uint32_t baud)
{
    uint32_t divisor = clk_hz / (16 * baud);
    uint32_t lcr = regs->read32(LCR);
    regs->write32(LCR, lcr | 0x80);       // Set DLAB
    regs->write32(DLL, divisor & 0xFF);
    regs->write32(DLM, (divisor >> 8) & 0xFF);
    regs->write32(LCR, lcr & ~0x80u);     // Clear DLAB
}

static void uart_init(smart::MappedFile* regs, uint32_t clk_hz, uint32_t baud)
{
    regs->write32(IER, 0x00);             // Disable all interrupts
    regs->write32(FCR, 0x07);             // Enable & reset FIFOs
    regs->write32(LCR, 0x03);             // 8N1
    regs->write32(MCR, 0x00);             // No flow control
    set_baud(regs, clk_hz, baud);
    regs->write32(IER, IER_RDI);          // Enable RX interrupt
}

// Drain all available bytes from the UART RX FIFO
static void drain_rx(smart::MappedFile* regs)
{
    while (regs->read32(LSR) & LSR_DR) {
        char c = static_cast<char>(regs->read32(RBR) & 0xFF);
        write(STDOUT_FILENO, &c, 1);
    }
}

// Transmit one byte, busy-wait for THRE
static void tx_byte(smart::MappedFile* regs, uint8_t b)
{
    while (!(regs->read32(LSR) & LSR_THRE))
        ;
    regs->write32(THR, b);
}

int main(int argc, char* argv[])
{
    const char* dev  = argc > 1 ? argv[1] : "serial";
    uint32_t clk_hz  = 100'000'000;  // 100 MHz typical for PL UARTs
    uint32_t baud    = 115200;

    // Open the UIO device by name
    smart::UioDevice uio(dev);
    smart::MappedFile* regs = uio.getRequiredMap(0);

    uart_init(regs, clk_hz, baud);
    std::fprintf(stderr, "uart_terminal: %s @ %u baud (ctrl-C to quit)\n", dev, baud);

    // Put stdin in raw mode so keypresses are sent immediately
    struct termios orig, raw;
    tcgetattr(STDIN_FILENO, &orig);
    raw = orig;
    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    int uio_fd = uio.getFileHandle();

    struct pollfd fds[2] = {
        { .fd = STDIN_FILENO, .events = POLLIN, .revents = 0 },
        { .fd = uio_fd,       .events = POLLIN, .revents = 0 },
    };

    bool running = true;
    while (running) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        // Keyboard → UART TX
        if (fds[0].revents & POLLIN) {
            char buf[64];
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            for (ssize_t i = 0; i < n; ++i) {
                if (buf[i] == 0x03) { running = false; break; }  // Ctrl-C
                tx_byte(regs, static_cast<uint8_t>(buf[i]));
            }
        }

        // UART RX interrupt → screen
        if (fds[1].revents & POLLIN) {
            // Acknowledge the UIO interrupt (read the count)
            uint32_t irq_count;
            read(uio_fd, &irq_count, sizeof(irq_count));

            drain_rx(regs);

            // Re-enable the UIO interrupt
            uint32_t unmask = 1;
            write(uio_fd, &unmask, sizeof(unmask));
        }
    }

    // Restore terminal
    tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    std::fprintf(stderr, "\nuart_terminal: exiting\n");
    return 0;
}
