#include <stdlib.h>
#include "ldl_radio.h"
#include "ldl_mac.h"
#include "ldl_sm.h"
#include "ldl_system.h"
#include "chip.h"

struct ldl_radio radio;
struct ldl_mac mac;
struct ldl_sm sm;

/* pointers to 16B root keys - MSB */
const uint8_t app_key_ptr[] = {
    /* TODO: !!! PROVIDE YOUR OWN APP KEY HERE !!! */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* pointers to 8B identifiers - MSB */
const uint8_t dev_eui_ptr[] = {
    /* TODO: !!! PROVIDE YOUR OWN DEV EUI HERE !!! */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* pointers to 8B identifiers - MSB */
const uint8_t join_eui_ptr[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* a pointer to be passed back to the application (anything you like) */
void *app_pointer = NULL; /* Not used in this example */

/* a pointer to be passed to the chip interface (anything you like) */
void *chip_interface_pointer = NULL; /* Not used in this example */

/* Called from within LDL_MAC_process() to pass events back to the application */
static void app_handler(void *app, enum ldl_mac_response_type type, const union ldl_mac_response_arg *arg);

static uint32_t system_rand(void *app)
{
    (void)app;

    return (uint32_t)rand();
}

static void enable_interrupts(void)
{
    /* Not used in this example - interrupts are enabled during init */
}

int main(void)
{
    /* initialise main chip itself */
    chip_init();

    /* initialise the default security module */
    LDL_SM_init(&sm, app_key_ptr);

    /* setup the radio */
    {
        struct ldl_sx127x_init_arg arg = {0};

        arg.xtal = LDL_RADIO_XTAL_CRYSTAL;
        arg.tx_gain = 200;  /* 2dBi */
        arg.pa = LDL_SX127X_PA_BOOST;
        arg.chip = chip_interface_pointer,

        arg.chip_read = chip_read,
        arg.chip_write = chip_write,
        arg.chip_set_mode = chip_set_mode,

        LDL_SX1276_init(&radio, &arg);
    }

    {
        struct ldl_mac_init_arg arg = {0};

        /* tell LDL the frequency of the clock source and compensate
         * for uncertainty */
        arg.ticks = chip_system_ticks;
        arg.tps = chip_system_ticks_tps();
        arg.a = 10;
        arg.b = 0;
        arg.advance = 0;

        /* connect LDL to a source of random numbers */
        arg.rand = system_rand;

        arg.radio = &radio;
        arg.radio_interface = LDL_Radio_getInterface(&radio);

        arg.sm = &sm;
        arg.sm_interface = LDL_SM_getInterface();

        arg.devEUI = dev_eui_ptr;
        arg.joinEUI = join_eui_ptr;

        arg.app = app_pointer;
        arg.handler = app_handler;
        arg.session = NULL; /* restore cached session state (or not, in this case) */
        arg.devNonce = 0;   /* restore devNonce */
        arg.joinNonce = 0;  /* restore joinNonce */

        LDL_MAC_init(&mac, LDL_EU_863_870, &arg);

        /* remember to connect the radio events back to the MAC */
        LDL_Radio_setEventCallback(&radio, &mac, LDL_MAC_radioEvent);
    }

    /* Optional:
     *
     * Ensure a maximum aggregated duty cycle of ~1%
     *
     * EU_863_870 already includes duty cycle limits. This is to safeguard
     * the example if the region is changed to US_902_928 or AU_915_928.
     *
     * Aggregated Duty Cycle Limit = 1 / (2 ^ setting)
     *
     * */
    LDL_MAC_setMaxDCycle(&mac, 7);

    enable_interrupts();

    for(;;){

        if(LDL_MAC_ready(&mac)){

            if(LDL_MAC_joined(&mac)){

                const char msg[] = "hello world";

                /* final argument is NULL since we don't have any specific invocation options */
                (void)LDL_MAC_unconfirmedData(&mac, 1, msg, sizeof(msg), NULL);
            }
            else{

                (void)LDL_MAC_otaa(&mac);
            }
        }

        LDL_MAC_process(&mac);

        /* a demonstration of how you might use sleep modes with LDL */
        {
            uint32_t ticks_until_next_event = LDL_MAC_ticksUntilNextEvent(&mac);

            if(ticks_until_next_event > 0U)
            {
                chip_sleep_wakeup_after(ticks_until_next_event);
            }
        }
    }
}

static void app_handler(void *app, enum ldl_mac_response_type type, const union ldl_mac_response_arg *arg)
{
    switch(type){

    /* This event is trigger by an earlier call to LDL_MAC_entropy()
     *
     * The value returned is random gathered by the radio driver
     * which can be used to seed stdlib.
     *
     * */
    case LDL_MAC_ENTROPY:
        srand(arg->entropy.value);
        break;

    /* this is data from confirmed/unconfirmed down frames */
    case LDL_MAC_RX:
        (void)arg->rx.port;
        (void)arg->rx.data;
        (void)arg->rx.size;
        break;

    /* an opportunity for application to cache session */
    case LDL_MAC_SESSION_UPDATED:
        (void)arg->session_updated.session;
        break;

    /* an opportunity for application to cache joinNonce */
    case LDL_MAC_JOIN_COMPLETE:
        (void)arg->join_complete.joinNonce;
        break;

    /* an opportunity for application to cache nextDevNonce */
    case LDL_MAC_DEV_NONCE_UPDATED:
        (void)arg->dev_nonce_updated.nextDevNonce;
        break;

    case LDL_MAC_CHANNEL_READY:
    case LDL_MAC_OP_ERROR:
    case LDL_MAC_OP_CANCELLED:
    case LDL_MAC_DATA_COMPLETE:
    case LDL_MAC_DATA_TIMEOUT:
    case LDL_MAC_LINK_STATUS:
    case LDL_MAC_DEVICE_TIME:
    case LDL_MAC_JOIN_EXHAUSTED:
    default:
        break;
    }
}

void chip_dio0_rising_edge_isr(void)
{
    LDL_Radio_handleInterrupt(&radio, 0);
}

void chip_dio1_rising_edge_isr(void)
{
    LDL_Radio_handleInterrupt(&radio, 1);
}
