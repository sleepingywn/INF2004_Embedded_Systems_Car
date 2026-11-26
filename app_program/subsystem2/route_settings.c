#include "route_settings.h"
/* Two independently erasable sectors, excluded from the executable by linker.
 * Flash is touched ONLY during early boot: IRQs off, core 1 still in ROM,
 * before any DMA, kernel, USB or WiFi initialization. Runtime saves reboot. */
#define SLOT0 0x1fe000u
#define SLOT1 0x1ff000u
#define PAGE 256u
/* A=right, B=left, C=straight, D=U-turn; Z (word 2, slot 5)=left. */
static route_actions_t active_actions = {{1u | (2u<<3) | (3u<<6) | (4u<<9), 0u, 2u<<15}};

void route_mapping_apply(const route_actions_t *actions)
{
    for (unsigned i = 0; i < ROUTE_ACTION_WORDS; ++i)
        __atomic_store_n(&active_actions.words[i], actions->words[i], __ATOMIC_RELAXED);
}

uint8_t route_mapping_get(unsigned index)
{
    if (index >= ROUTE_MAP_COUNT) return 0u;
    uint32_t word = __atomic_load_n(&active_actions.words[index / ROUTE_ACTIONS_PER_WORD],
                                    __ATOMIC_RELAXED);
    return (uint8_t)((word >> route_action_shift(index)) & 7u);
}

void route_mapping_export(uint8_t map[ROUTE_MAP_COUNT])
{
    for (unsigned i = 0; i < ROUTE_MAP_COUNT; ++i) map[i] = route_mapping_get(i);
}
/* The console is the sole runtime writer and exports the table itself.
 * A concurrent motor decode reads exactly one letter and latches its action. */
uint32_t route_mapping_revision;
bool route_mapping_persisted;
bool route_settings_valid(const uint8_t *m) {
    for(unsigned i=0;i<26;++i)if(m[i]>4)return false;
    return true;
}
static __attribute__((unused)) uint32_t crc(const uint8_t *p,unsigned n) {
    uint32_t c=~0u;
    while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}
    return ~c;
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put32(uint8_t *p,uint32_t n) {
    for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));
}
static bool record_valid(const uint8_t *p) {
    return get32(p)==0x3150414du && route_settings_valid(p+8) && get32(p+36)==crc(p,36);
}
static __attribute__((unused)) int choose(const uint8_t *a,const uint8_t *b) {
    bool av=record_valid(a),bv=record_valid(b);
    if(!av)return bv ? 1 : -1;
    if(!bv)return 0;
    uint32_t d=get32(b+4)-get32(a+4);
    return d && d<0x80000000u ? 1 : 0;
}
static __attribute__((unused)) void make_record(uint8_t *p,const uint8_t *map,uint32_t revision) {
    for(unsigned i=0;i<PAGE;++i)p[i]=0xff;
    put32(p,0x3150414du);put32(p+4,revision);
    for(unsigned i=0;i<26;++i)p[8+i]=map[i];
    put32(p+36,crc(p,36));
}
/* MAP1 pages keep the deployed CRC and field offsets. Even a 10-byte table
 * requires one 256-byte flash program; retaining MAP1 also allows rollback
 * to firmware that already reads the robot's saved settings. */
static __attribute__((unused)) void pack(uint8_t *p,const uint8_t *map) {
    for(unsigned i=0;i<13;++i)p[i]=map[i*2]|(map[i*2+1]<<4);
    p[13]=0xa7;uint32_t c=crc(p,14);p[14]=(uint8_t)c;p[15]=(uint8_t)(c>>8);
}
static __attribute__((unused)) bool unpack(const uint8_t *p,uint8_t *map) {
    uint32_t c=crc(p,14);
    if(p[13]!=0xa7 || p[14]!=(uint8_t)c || p[15]!=(uint8_t)(c>>8))return false;
    for(unsigned i=0;i<13;++i){map[i*2]=p[i]&15;map[i*2+1]=p[i]>>4;}
    return route_settings_valid(map);
}
#ifndef ROUTE_SETTINGS_HOST
#define REG(a) (*(volatile uint32_t *)(a))
typedef void (*rom_void)(void);
typedef void (*rom_erase)(uint32_t,unsigned,uint32_t,uint8_t);
typedef void (*rom_program)(uint32_t,const uint8_t *,unsigned);
typedef void *(*rom_lookup)(uint16_t *,uint32_t);
static void *rom(unsigned code) {
    rom_lookup lookup=(rom_lookup)(uintptr_t)(*(volatile uint16_t *)0x18);
    return lookup((uint16_t *)(uintptr_t)(*(volatile uint16_t *)0x14),code);
}
/* All instructions, literals, page data and boot2 are in SRAM during XIP-off.
 * No library calls, interrupts or secondary-core work are permitted here. */
__attribute__((section(".time_critical.route_settings"),noinline))
static void commit(uint32_t offset,const uint8_t *page,rom_void connect,rom_void exit_xip,
                   rom_erase erase,rom_program program,rom_void flush,rom_void boot2) {
    connect();exit_xip();erase(offset,4096,65536,0xd8);program(offset,page,PAGE);flush();boot2();
}
void route_settings_boot(void) {
    const uint8_t *slots[2]={(const uint8_t *)(0x10000000u+SLOT0),(const uint8_t *)(0x10000000u+SLOT1)};
    uint8_t pending[16],map[26];
    for(unsigned i=0;i<4;++i)put32(pending+i*4,REG(0x4005800cu+i*4));
    bool requested=(REG(0x40058008u)&3u) && unpack(pending,map);
    REG(0x40058018u)=0; /* invalidate scratch marker before attempting flash */
    int active=choose(slots[0],slots[1]);
    if(requested) {
        bool changed=active<0;
        for(unsigned i=0;i<26 && active>=0;++i)if(slots[active][8+i]!=map[i])changed=true;
        if(changed) {
            uint32_t boot2[64];uint8_t page[PAGE];
            for(unsigned i=0;i<64;++i)boot2[i]=((const volatile uint32_t *)0x10000000u)[i];
            make_record(page,map,active<0 ? 1u : get32(slots[active]+4)+1u);
            rom_void connect=(rom_void)rom('I'|('F'<<8)),exit_xip=(rom_void)rom('E'|('X'<<8));
            rom_erase erase=(rom_erase)rom('R'|('E'<<8));
            rom_program program=(rom_program)rom('R'|('P'<<8));
            rom_void flush=(rom_void)rom('F'|('C'<<8));
            if(connect && exit_xip && erase && program && flush)
                commit(active==0 ? SLOT1 : SLOT0,page,connect,exit_xip,erase,program,flush,
                       (rom_void)((uintptr_t)boot2|1u));
            active=choose(slots[0],slots[1]);
        }
    }
    if(active>=0) {
        route_actions_t actions = {0};
        for (unsigned i = 0; i < ROUTE_MAP_COUNT; ++i)
            (void)route_action_set(&actions, i, slots[active][8+i]);
        route_mapping_apply(&actions);
        route_mapping_revision=get32(slots[active]+4);route_mapping_persisted=true;
    }
}
void route_settings_reboot(const uint8_t map[26]) {
    uint8_t pending[16];pack(pending,map);
    REG(0x40058000u)=0; /* disable watchdog before staging */
    for(unsigned i=0;i<4;++i)REG(0x4005800cu+i*4)=get32(pending+i*4);
    REG(0x4005801cu)=0; /* normal flash boot, not ROM BOOTSEL */
    REG(0x40010008u)=0x1fffcu; /* watchdog resets all except ROSC/XOSC */
    __asm volatile("dsb" ::: "memory");
    REG(0x40058000u)=0x80000000u;
    for(;;)__asm volatile("nop");
}
#endif
