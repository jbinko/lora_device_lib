#ifndef RP2040_CHIP_H
#define RP2040_CHIP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "ldl_chip.h"

void chip_init(void);
void chip_set_mode(void *self, enum ldl_chip_mode mode);
bool chip_write(void *self, const void *opcode, size_t opcode_size, const void *data, size_t size);
bool chip_read(void *self, const void *opcode, size_t opcode_size, void *data, size_t size);
void chip_sleep_wakeup_after(uint32_t ticks);
uint32_t chip_system_ticks(void *app);
uint32_t chip_system_ticks_tps(void);
void chip_dio0_rising_edge_isr(void);
void chip_dio1_rising_edge_isr(void);

#ifdef __cplusplus
}
#endif

#endif
