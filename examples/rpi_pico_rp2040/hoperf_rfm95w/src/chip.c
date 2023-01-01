#include "pico/stdlib.h"
#include "chip.h"
#include "ldl_debug.h"

#include "hardware/spi.h"
#include "hardware/gpio.h"

#define RST_GPIO  6                                     // PIN09
#define NSS_GPIO  7                                     // PIN10
#define DIO0_GPIO 8                                     // PIN11
#define DIO1_GPIO 9                                     // PIN12
#define SPI0_MISO_GPIO PICO_DEFAULT_SPI_RX_PIN  // 16   // PIN21
#define SPI0_SCK_GPIO  PICO_DEFAULT_SPI_SCK_PIN // 18   // PIN24
#define SPI0_MOSI_GPIO PICO_DEFAULT_SPI_TX_PIN  // 19   // PIN25

#define SPI_INST spi0

#define REG_LR_VERSION                                      0x42

/*
Returns ticks in ms - increases from power up
Will overflow after 49 days
*/
static uint32_t chip_time_ms_32(void)
{
    return (uint32_t) (time_us_64() / 1000ULL);
}

/*
Ticks are represented by ms from power up
*/
uint32_t chip_system_ticks(void *app)
{
    /* unused in this example */
    (void)app;

    return chip_time_ms_32();
}

/*
Ticks are represented by ms from power up
so, 1000 TPS
*/
uint32_t chip_system_ticks_tps(void)
{
    return 1000UL;
}

static void spi_chip_select(void)
{
    gpio_put(NSS_GPIO, 0);
}

static void spi_chip_release(void)
{
    gpio_put(NSS_GPIO, 1);
}

static void spi_write_burst(const uint8_t* data, size_t len)
{
    if (len > 0) {
        int written = spi_write_blocking(SPI_INST, data, len);
        if (written != len) {
            LDL_ERROR("spi_write_burst error. Written: %d.\n", written);
        }
    }
}

static void spi_read_burst(uint8_t* data, size_t len)
{
    if (len > 0) {
        int read = spi_read_blocking(SPI_INST, 0x00, data, len);
        if (read != len) {
            LDL_ERROR("spi_read_burst error. Read: %d.\n", read);
        }
    }
}

static uint8_t spi_read_register(uint8_t address)
{
    // SPI Interface - Section 4.3 Datasheet
    // Address => wnr bit, which is 0 for read access + 7 bits of address, MSB first

    uint8_t data = address & 0x7F;
    chip_read(NULL, &data, 1, &data, 1);

    return data;
}

bool chip_write(void *self, const void *opcode, size_t opcode_size, const void *data, size_t size)
{
    /* unused in this example */
    (void)self;

    spi_chip_select();
    spi_write_burst((const uint8_t *)opcode, opcode_size);
    spi_write_burst((const uint8_t *)data, size);
    spi_chip_release();

    /* this would return false if there was a busy timeout */
    return true;
}

bool chip_read(void *self, const void *opcode, size_t opcode_size, void *data, size_t size)
{
    /* unused in this example */
    (void)self;

    spi_chip_select();
    spi_write_burst((const uint8_t *)opcode, opcode_size);
    spi_read_burst((uint8_t *)data, size);
    spi_chip_release();

    /* this would return false if there was a busy timeout */
    return true;
}

static void chip_reset(void)
{
    // POR Reset - Section 7.2.1 Datasheet
    // Wait for 10 ms before commencing communications over the SPI bus
    // Manual Reset - Section 7.2.2 Datasheet
    // Pin Reset should be pulled low for a hundred microseconds, and then released.
    // The user should then wait for 5 ms before using the chip.
    LDL_INFO("chip_reset\n");

    gpio_put(RST_GPIO, 0);
    sleep_ms(10);
    gpio_put(RST_GPIO, 1);
    sleep_ms(10);
}

/* On more sophisticated hardware you may also need to switch:
 *
 * - on/off an oscillator
 * - rfi circuit
 * - rfo circuit
 * - boost circuit
 *
 * */
void chip_set_mode(void *self, enum ldl_chip_mode mode)
{
    /* unused in this example */
    (void)self;

    switch(mode) {
    case LDL_CHIP_MODE_RESET:
        chip_reset();
        break;
    case LDL_CHIP_MODE_SLEEP:
        break;
    case LDL_CHIP_MODE_STANDBY:
        break;
    case LDL_CHIP_MODE_RX:
        break;
    case LDL_CHIP_MODE_TX_BOOST:
        break;
    case LDL_CHIP_MODE_TX_RFO:
        break;
    }
}

/*
somehow set a timer event that will ensure a wakeup so many ticks in future
TODO: This should do more advanced sleep with timer (power reduction)
*/
void chip_sleep_wakeup_after(uint32_t count)
{
    /* TODO: This is NOT good implementation */
    const uint32_t MaxCount = 0xFFFFFFFF;
    if (count == MaxCount)
        return;

    /* We represent ticks as milliseconds so we can sleep for count ticks/milliseconds */
    sleep_ms(count);
}

/*
LDL must be connected to the rising edge of some interrupt lines.
On SX127X this is DIO0 and DIO1.
On SX126X this is DIO1.
*/
static void dio_rising_edge_callback(uint gpio, uint32_t event_mask)
{
    if (gpio == DIO0_GPIO)
    {
        if (event_mask & GPIO_IRQ_EDGE_RISE)
        {
            LDL_INFO("DIO0 IRQ\n");
            chip_dio0_rising_edge_isr();
            gpio_acknowledge_irq(DIO0_GPIO, event_mask);
        }
    }
    else if (gpio == DIO1_GPIO)
    {
        if (event_mask & GPIO_IRQ_EDGE_RISE)
        {
            LDL_INFO("DIO1 IRQ\n");
            chip_dio1_rising_edge_isr();
            gpio_acknowledge_irq(DIO1_GPIO, event_mask);
        }
    }
}

void chip_init(void)
{
    LDL_INFO("chip_init\n");

    gpio_init(NSS_GPIO);
    gpio_set_dir(NSS_GPIO, GPIO_OUT);

    gpio_init(RST_GPIO);
    gpio_set_dir(RST_GPIO, GPIO_OUT);

    gpio_set_function(SPI0_MISO_GPIO, GPIO_FUNC_SPI);
    gpio_set_function(SPI0_SCK_GPIO, GPIO_FUNC_SPI);
    gpio_set_function(SPI0_MOSI_GPIO, GPIO_FUNC_SPI);

    // Init SPI, set MCU is master
    const int spi_speed = 15625 * 1000;
    spi_init(SPI_INST, spi_speed);
    spi_set_slave(SPI_INST, false);
    // SPI_MODE0 (0|0) - CPOL = 0 and CPHA = 0 - Section 4.3 Datasheet
    spi_set_format(SPI_INST, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_irq_enabled_with_callback(DIO0_GPIO, GPIO_IRQ_EDGE_RISE, true, dio_rising_edge_callback);
    gpio_set_irq_enabled_with_callback(DIO1_GPIO, GPIO_IRQ_EDGE_RISE, true, dio_rising_edge_callback);

    chip_reset();

    const uint8_t expectedVersion = 0x12;
    uint8_t version = spi_read_register(REG_LR_VERSION);
    if (version != expectedVersion) {

        LDL_ERROR("Unexpected SX127x version number: 0x%x. Expected 0x%x.\n", version, expectedVersion);
        return;
    }

    LDL_INFO("SX127x version number: 0x%x.\n", version);
}
