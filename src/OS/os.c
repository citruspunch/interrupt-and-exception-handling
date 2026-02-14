#include "os.h"

#if defined(TARGET_QEMU)
// QEMU VersatilePB UART0 (PL011)
#define UART0_BASE       0x101F1000
#define UART_DR          (UART0_BASE + 0x00)
#define UART_FR          (UART0_BASE + 0x18)
#define UART_FR_TXFF     0x20
#define UART_FR_RXFE     0x10

// QEMU VersatilePB Timer0 (SP804)
#define SP804_TIMER0_BASE 0x101E2000
#define TIMER_LOAD        (SP804_TIMER0_BASE + 0x00)
#define TIMER_CTRL        (SP804_TIMER0_BASE + 0x08)
#define TIMER_INTCLR      (SP804_TIMER0_BASE + 0x0C)

// QEMU VersatilePB VIC (PL190)
#define VIC_BASE         0x10140000
#define VIC_INTENABLE    (VIC_BASE + 0x10)
#define VIC_VECTADDR     (VIC_BASE + 0x30)
#define TIMER_IRQ_BIT    4

#else
// BeagleBone Black UART0 base address
#define UART0_BASE     0x44E09000
#define UART_THR       (UART0_BASE + 0x00)  // Transmit Holding Register
#define UART_LSR       (UART0_BASE + 0x14)  // Line Status Register
#define UART_LSR_THRE  0x20                  // Transmit Holding Register Empty
#define UART_LSR_RXFE  0x10                  // Receive FIFO Empty

// BeagleBone Black DMTIMER2 base address
#define DMTIMER2_BASE    0x48040000
#define TCLR             (DMTIMER2_BASE + 0x38)  // Timer Control Register
#define TCRR             (DMTIMER2_BASE + 0x3C)  // Timer Counter Register
#define TISR             (DMTIMER2_BASE + 0x28)  // Timer Interrupt Status Register
#define TIER             (DMTIMER2_BASE + 0x2C)  // Timer Interrupt Enable Register
#define TLDR             (DMTIMER2_BASE + 0x40)  // Timer Load Register

// BeagleBone Black Interrupt Controller (INTCPS) base address
#define INTCPS_BASE      0x48200000
#define INTC_MIR_CLEAR2  (INTCPS_BASE + 0xC8)    // Interrupt Mask Clear Register 2
#define INTC_CONTROL     (INTCPS_BASE + 0x48)    // Interrupt Controller Control
#define INTC_ILR68       (INTCPS_BASE + 0x110)   // Interrupt Line Register 68

// Clock Manager base address
#define CM_PER_BASE      0x44E00000
#define CM_PER_TIMER2_CLKCTRL (CM_PER_BASE + 0x80)  // Timer2 Clock Control
#endif

// ============================================================================
// UART Functions
// ============================================================================

// Function to send a single character via UART
void uart_putc(char c) {
#if defined(TARGET_QEMU)
    while (GET32(UART_FR) & UART_FR_TXFF);
    PUT32(UART_DR, c);
#else
    while ((GET32(UART_LSR) & UART_LSR_THRE) == 0);
    PUT32(UART_THR, c);
#endif
}

// Function to receive a single character via UART
char uart_getc(void) {
#if defined(TARGET_QEMU)
    while (GET32(UART_FR) & UART_FR_RXFE);
    return (char)(GET32(UART_DR) & 0xFF);
#else
    while ((GET32(UART_LSR) & UART_LSR_RXFE) != 0);
    return (char)(GET32(UART_THR) & 0xFF);
#endif
}

// Function to send a string via UART
void os_write(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

// Function to receive a line of input via UART
void os_read(char *buffer, int max_length) {
    int i = 0;
    char c;
    while (i < max_length - 1) { // Leave space for null terminator
        c = uart_getc();
        if (c == '\n' || c == '\r') {
            uart_putc('\n'); // Echo newline
            break;
        }
        uart_putc(c); // Echo character
        buffer[i++] = c;
    }
    buffer[i] = '\0'; // Null terminate the string
}

// Helper function to print an unsigned integer
void uart_putnum(unsigned int num) {
    char buf[10];
    int i = 0;
    if (num == 0) {
        uart_putc('0');
        uart_putc('\n');
        return;
    }
    do {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    } while (num > 0 && i < 10);
    while (i > 0) {
        uart_putc(buf[--i]);
    }
    uart_putc('\n');
}

// ============================================================================
// Timer Functions
// ============================================================================

// This function should:
// 1. Enable the timer clock (CM_PER_TIMER2_CLKCTRL = 0x2)
// 2. Unmask IRQ 68 in the interrupt controller (INTC_MIR_CLEAR2)
// 3. Configure interrupt priority (INTC_ILR68 = 0x0)
// 4. Stop the timer (TCLR = 0)
// 5. Clear any pending interrupts (TISR = 0x7)
// 6. Set the load value for 2 seconds (TLDR = 0xFE91CA00)
// 7. Set the counter to the same value (TCRR = 0xFE91CA00)
// 8. Enable overflow interrupt (TIER = 0x2)
// 9. Start timer in auto-reload mode (TCLR = 0x3)
void timer_init(void) {
#if defined(TARGET_QEMU)
    PUT32(TIMER_CTRL, 0x0);
    PUT32(TIMER_INTCLR, 0x1);
    PUT32(VIC_INTENABLE, (1U << TIMER_IRQ_BIT));
    PUT32(TIMER_LOAD, 2000000);
    PUT32(TIMER_CTRL, 0xE2); // Enable | Periodic | Interrupt enable | 32-bit
#else
    PUT32(CM_PER_TIMER2_CLKCTRL, 0x2); // Enable timer clock
    PUT32(INTC_MIR_CLEAR2, 1 << (68 - 64)); // Unmask IRQ 68
    PUT32(INTC_ILR68, 0x0); // Set interrupt priority
    PUT32(TCLR, 0); // Stop timer
    PUT32(TISR, 0x7); // Clear pending interrupts
    PUT32(TLDR, 0xFE91CA00); // Load value for 2 seconds
    PUT32(TCRR, 0xFE91CA00); // Set counter to load value
    PUT32(TIER, 0x2); // Enable overflow interrupt
    PUT32(TCLR, 0x3); // Start timer in auto-reload mode
#endif
}

// This function should:
// 1. Clear the timer interrupt flag (TISR = 0x2)
// 2. Acknowledge the interrupt to the controller (INTC_CONTROL = 0x1)
// 3. Print "Tick\n" via UART
void timer_irq_handler(void) {
#if defined(TARGET_QEMU)
    PUT32(TIMER_INTCLR, 0x1);
    PUT32(VIC_VECTADDR, 0x0);
#else
    PUT32(TISR, 0x2); // Clear timer interrupt flag
    PUT32(INTC_CONTROL, 0x1); // Acknowledge interrupt to controller
#endif
    os_write("Tick\n"); // Print "Tick"
}

// ============================================================================
// Main Program
// ============================================================================

// Simple random number generator (Linear Congruential Generator)
static unsigned int seed = 12345;

unsigned int rand(void) {
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    return seed;
}

int main(void) {
    os_write("Starting OS...\n");
    os_write("Initializing timer...\n");
    timer_init();
    os_write("Enabling interrupts...\n");
    enable_irq();
    os_write("Interrupts enabled. Starting main loop...\n");

    // Main loop: continuously print random numbers
    while (1) {
        unsigned int random_num = rand() % 1000;
        uart_putnum(random_num);
        
        // Small delay to prevent overwhelming UART
#if defined(TARGET_QEMU)
        for (volatile int i = 0; i < 50000000; i++);
#else
        for (volatile int i = 0; i < 1000000; i++);
#endif
    }
    
    return 0;
}
