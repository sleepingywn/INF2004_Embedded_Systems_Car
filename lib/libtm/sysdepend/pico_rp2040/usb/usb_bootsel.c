/* RP2040 ROM USB-boot entry, without linking a second SDK runtime.
 * ROM ABI: function table halfword at 0x14, lookup halfword at 0x18,
 * little-endian code 'UB'. See Pico SDK bootrom.h / bootrom_constants.h.
 * The caller must stop application outputs before entering this function.
 */
#include <stdint.h>

typedef void *(*rom_lookup_t)(uint16_t *, uint32_t);
typedef void (*rom_usb_boot_t)(uint32_t, uint32_t);

int tm_usb_enter_bootsel(void)
{
    /* Volatile ROM reads avoid compiler null-page assumptions. */
    uintptr_t lookup_address=*(volatile uint16_t *)(uintptr_t)0x18u;
    uintptr_t table_address=*(volatile uint16_t *)(uintptr_t)0x14u;
    if(!lookup_address || !table_address)return -1;
    rom_lookup_t lookup=(rom_lookup_t)lookup_address;
    rom_usb_boot_t boot=(rom_usb_boot_t)lookup((uint16_t *)table_address,
                                             (uint32_t)'U' | ((uint32_t)'B'<<8));
    if(!boot)return -1;
    /* ROM schedules a watchdog reset into USB boot on both-core reset.
     * No activity GPIO, both mass-storage and picoboot interfaces enabled. */
    boot(0u,0u);
    return -1; /* ROM entry should never return */
}
