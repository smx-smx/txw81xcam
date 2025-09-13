#include "hal/gpio.h"
#include "osal/os_api.h"
#include "common/printk.h"

/*
 * Safe, bit-banged SCCB/I2C scanner for TXW81x
 * - Only pulls lines low (never drives high)
 * - Uses 100k internal pull-ups
 * - Very slow clocks => gentle scanning
 *
 * Works without changing PIN_IIC2_* macros because this is pure GPIO bit-bang.
 */

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

/* --- timing helpers (slow and safe) --- */
static void bb_delay_us(unsigned int us)
{
    /* OSAL doesn't always expose a microsecond delay; use a tiny busy-wait. */
    volatile unsigned int n = us * 40; /* ~roughly tuned for ~240MHz/RTOS; slow is fine */
    while (n--) { __asm__ volatile("nop"); }
}
static void bb_delay_ms(unsigned int ms)
{
    os_mdelay(ms);
}

/* --- line helpers: release=high (via pull-up), drive low = 0 --- */
static void line_release(uint32_t pin)
{
    /* Input, pull-up, 100k, minimal drive */
    gpio_driver_strength(pin, GPIO_DS_4MA);
    gpio_set_mode(pin, GPIO_PULL_UP, GPIO_PULL_LEVEL_100K);
    gpio_set_dir(pin, GPIO_DIR_INPUT);
}
static void line_low(uint32_t pin)
{
    /* Open-drain low */
    gpio_driver_strength(pin, GPIO_DS_4MA);
    gpio_set_mode(pin, GPIO_OPENDRAIN_DROP_NONE, GPIO_PULL_LEVEL_NONE);
    gpio_set_dir(pin, GPIO_DIR_OUTPUT);
    gpio_set_val(pin, 0);
}
static int line_read(uint32_t pin)
{
    gpio_set_dir(pin, GPIO_DIR_INPUT);
    return gpio_get_val(pin);
}

/* --- I2C bit-bang primitives --- */
static void bb_start(uint32_t scl, uint32_t sda)
{
    line_release(sda); line_release(scl); bb_delay_us(200);
    line_low(sda); bb_delay_us(200);
    line_low(scl); bb_delay_us(200);
}
static void bb_stop(uint32_t scl, uint32_t sda)
{
    line_low(sda); bb_delay_us(200);
    line_release(scl); bb_delay_us(200);
    line_release(sda); bb_delay_us(200);
}
static void bb_clk_high(uint32_t scl) { line_release(scl); bb_delay_us(200); }
static void bb_clk_low (uint32_t scl) { line_low(scl);   bb_delay_us(200); }

static int bb_write_bit(uint32_t scl, uint32_t sda, int bit)
{
    if (bit) line_release(sda); else line_low(sda);
    bb_clk_high(scl);
    /* No clock-stretching handling to keep it simple and gentle */
    bb_clk_low(scl);
    return 0;
}
static int bb_read_bit(uint32_t scl, uint32_t sda)
{
    line_release(sda);
    bb_clk_high(scl);
    int b = line_read(sda);
    bb_clk_low(scl);
    return b;
}

/* Returns 1 if ACK received, 0 if NACK */
static int bb_write_byte(uint32_t scl, uint32_t sda, unsigned char byte)
{
    for (int i = 7; i >= 0; --i)
        bb_write_bit(scl, sda, (byte >> i) & 1);

    /* Read ACK bit */
    int ack = (bb_read_bit(scl, sda) == 0);
    return ack;
}

/* If bus looks stuck low, clock it free (classic 9-pulse reset) */
static void bb_try_bus_reset(uint32_t scl, uint32_t sda)
{
    line_release(sda); line_release(scl); bb_delay_us(200);
    if (!line_read(sda) || !line_read(scl)) {
        for (int i=0; i<9; ++i) { bb_clk_high(scl); bb_clk_low(scl); }
        bb_stop(scl, sda);
    }
}

/* Gentle sanity check: both lines should idle high via pull-ups */
static int bus_idle_high(uint32_t scl, uint32_t sda)
{
    line_release(scl); line_release(sda);
    bb_delay_us(300);
    return line_read(scl) && line_read(sda);
}

/* Candidate pin pairs (SCL,SDA).
 * Keep to "safe" ports that appear across EVBs/docs and your configs.
 * Feel free to add/remove here.
 */
static const uint16_t kSafePairs[][2] = {
    { PC_2, PC_3 }, { PC_3, PC_2 },
    { PC_4, PC_3 }, { PC_3, PC_4 },
    { PC_4, PC_5 }, { PC_5, PC_4 },
    { PC_0, PC_1 }, { PC_1, PC_0 },
    { PA_11, PA_8 }, { PA_8, PA_11 },
    /* Add others you want to try, but avoid pins already used by UART/WiFi/LCD in your config. */
};

/* Conservative address list:
 * SCCB/I2C camera sensors often answer on 0x21, 0x23, 0x30..0x3F, 0x42>>1 (=0x21), 0x60>>1 (=0x30) etc.
 * We only do a START + address + STOP and look for ACK.
 */
static const unsigned char kProbeAddrs[] = {
    0x10,0x11,0x12,0x13,
    0x21,0x23, /* 0x42/0x46 write addrs >>1 */
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
    0x3C,0x3D,0x3E,0x3F,
    0x40,0x41,0x42,0x43, /* some sensors decode multiple */
    0x60,0x61,0x62,0x63  /* some SCCB flavors */
};

static void print_pin(uint16_t p)
{
    char bank = (p >= 0x20 && p < 0x40) ? 'B' : (p >= 0x40 ? 'C' : 'A');
    int idx = (p & 0x1F);
    printk("%c%d", bank, idx);
}

/* Public entry */
void txw81x_bb_sccb_scan_and_log(void)
{
    printk("[bbscan] Safe SCCB/I2C pin scan START\n");

    for (unsigned i=0; i<ARRAYSIZE(kSafePairs); ++i) {
        uint16_t scl = kSafePairs[i][0];
        uint16_t sda = kSafePairs[i][1];
        if (scl == 255 || sda == 255) continue; /* skip invalid */

        /* Leave lines in passive pull-up first */
        line_release(scl);
        line_release(sda);
        bb_delay_ms(1);

        if (!bus_idle_high(scl, sda)) {
            printk("[bbscan] Skip ");
            print_pin(scl); printk("/"); print_pin(sda);
            printk(": idle not high; trying gentle bus-reset...\n");
            bb_try_bus_reset(scl, sda);
            if (!bus_idle_high(scl, sda)) {
                printk("[bbscan]  -> still busy; skipped to avoid forcing lines.\n");
                continue;
            }
        }

        /* Probe a few common addresses; stop at the first ACK */
        int hit = 0;
        for (unsigned a=0; a<ARRAYSIZE(kProbeAddrs) && !hit; ++a) {
            unsigned char addr = kProbeAddrs[a] << 1; /* write phase */
            bb_start(scl, sda);
            int ack = bb_write_byte(scl, sda, addr);
            bb_stop(scl, sda);
            bb_delay_ms(1);

            if (ack) {
                printk("[bbscan] FOUND device ACK at 7b 0x%02X on ", kProbeAddrs[a]);
                print_pin(scl); printk("(SCL)/"); print_pin(sda); printk("(SDA)\n");
                hit = 1;
            }
        }

        if (!hit) {
            printk("[bbscan] No ACKs on ");
            print_pin(scl); printk("/"); print_pin(sda); printk("\n");
        }
    }
    printk("[bbscan] DONE\n");
}
