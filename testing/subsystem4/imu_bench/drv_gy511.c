/*
 * drv_gy511.c  -  Buddy 4: accelerometer driver (GY-511 and compatible boards)
 *
 * Boards sold as "GY-511" or used in the lab can carry different chips, so the
 * driver detects which one answers and sets it up accordingly:
 *
 *   Chip            I2C address     ID register         Data
 *   LSM303DLHC/AGR  0x19 (0x18)     0x0F = 0x33 (AGR)   0x28.., LE, 12-bit left-justified
 *   LSM303D         0x1D (0x1E)     0x0F = 0x49         0x28.., LE, 16-bit
 *   ADXL345         0x53 (0x1D)     0x00 = 0xE5         0x32.., LE, 16-bit, 3.9 mg/count
 *   MMA845x         0x1D (0x1C)     0x0D = 0x1A/2A/3A   0x01.., BE, 16-bit left-justified
 *
 * All are set to about 100 Hz and +-2 g, and every reading is returned in g.
 */
#include "drv_gy511.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define GY511_I2C           i2c0
#define I2C_TIMEOUT_US      2000        /* never block longer than 2 ms */

/* ---------------- per-chip register values ---------------- */
/* LSM303DLHC / LSM303AGR (accelerometer part) */
#define DLHC_CTRL1          0x20
#define DLHC_CTRL4          0x23
#define DLHC_OUT            0x28
#define DLHC_CTRL1_VAL      0x57        /* 100 Hz, normal, X/Y/Z on           */
#define DLHC_CTRL4_VAL      0x88        /* BDU, +-2 g, high resolution         */
/* LSM303D */
#define LSMD_CTRL1          0x20
#define LSMD_CTRL2          0x21
#define LSMD_OUT            0x28
#define LSMD_CTRL1_VAL      0x6F        /* 100 Hz, BDU, X/Y/Z on               */
#define LSMD_CTRL2_VAL      0x00        /* +-2 g                               */
/* ADXL345 */
#define ADXL_BW_RATE        0x2C
#define ADXL_POWER_CTL      0x2D
#define ADXL_DATA_FORMAT    0x31
#define ADXL_OUT            0x32
#define ADXL_BW_RATE_VAL    0x0A        /* 100 Hz                              */
#define ADXL_FORMAT_VAL     0x08        /* full resolution, +-2 g              */
#define ADXL_POWER_VAL      0x08        /* measure mode                        */
/* MMA8451 / 8452 / 8453 */
#define MMA_CTRL_REG1       0x2A
#define MMA_XYZ_DATA_CFG    0x0E
#define MMA_OUT             0x01
#define MMA_CTRL1_VAL       0x19        /* 100 Hz, active                      */
#define MMA_CFG_VAL         0x00        /* +-2 g                               */

#define AUTO_INC_ST         0x80        /* ST chips: set bit 7 to auto-increment */

static bool         bus_ready;
static uint8_t      acc_addr;
static gy511_chip_t chip;
static uint32_t     err_count;

static void bus_init(void)
{
    if (bus_ready) return;
    i2c_init(GY511_I2C, GY511_I2C_BAUD);
    gpio_set_function(GY511_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(GY511_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(GY511_I2C_SDA_PIN);    /* weak backup for the board's pull-ups */
    gpio_pull_up(GY511_I2C_SCL_PIN);
    bus_ready = true;
}

static bool write_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_write_timeout_us(GY511_I2C, addr, buf, 2, false, I2C_TIMEOUT_US) == 2;
}

static bool read_regs(uint8_t addr, uint8_t reg, uint8_t *dst, size_t len)
{
    if (i2c_write_timeout_us(GY511_I2C, addr, &reg, 1, true, I2C_TIMEOUT_US) != 1)
        return false;
    return i2c_read_timeout_us(GY511_I2C, addr, dst, len, false, I2C_TIMEOUT_US) == (int)len;
}

static int read_reg(uint8_t addr, uint8_t reg)      /* -1 on error */
{
    uint8_t v;
    return read_regs(addr, reg, &v, 1) ? v : -1;
}

static bool device_answers(uint8_t addr)
{
    uint8_t dummy;
    return i2c_read_timeout_us(GY511_I2C, addr, &dummy, 1, false, I2C_TIMEOUT_US) == 1;
}

/* Write a register and read it back to prove the write landed. */
static bool set_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
    return write_reg(addr, reg, val) && read_reg(addr, reg) == val;
}

/* Work out which accelerometer (if any) answers at this address. */
static gy511_chip_t identify(uint8_t addr)
{
    if (!device_answers(addr)) return GY511_CHIP_NONE;

    int st_who = read_reg(addr, 0x0F);
    if (addr == 0x19 || addr == 0x18) {
        /* LSM303DLHC has no WHO_AM_I; AGR (and LIS3DH) read 0x33. Same registers. */
        return (st_who == 0x33) ? GY511_CHIP_LSM303AGR : GY511_CHIP_LSM303DLHC;
    }
    if (st_who == 0x49) return GY511_CHIP_LSM303D;
    if (read_reg(addr, 0x00) == 0xE5) return GY511_CHIP_ADXL345;
    int mma = read_reg(addr, 0x0D);
    if (mma == 0x1A || mma == 0x2A || mma == 0x3A) return GY511_CHIP_MMA845X;
    return GY511_CHIP_NONE;
}

static bool configure(void)
{
    switch (chip) {
    case GY511_CHIP_LSM303DLHC:
    case GY511_CHIP_LSM303AGR:
        return set_reg(acc_addr, DLHC_CTRL1, DLHC_CTRL1_VAL) &&
               set_reg(acc_addr, DLHC_CTRL4, DLHC_CTRL4_VAL);
    case GY511_CHIP_LSM303D:
        return set_reg(acc_addr, LSMD_CTRL1, LSMD_CTRL1_VAL) &&
               set_reg(acc_addr, LSMD_CTRL2, LSMD_CTRL2_VAL);
    case GY511_CHIP_ADXL345:
        return set_reg(acc_addr, ADXL_BW_RATE, ADXL_BW_RATE_VAL) &&
               set_reg(acc_addr, ADXL_DATA_FORMAT, ADXL_FORMAT_VAL) &&
               set_reg(acc_addr, ADXL_POWER_CTL, ADXL_POWER_VAL);
    case GY511_CHIP_MMA845X:
        /* settings can only be changed in standby */
        return set_reg(acc_addr, MMA_CTRL_REG1, 0x00) &&
               set_reg(acc_addr, MMA_XYZ_DATA_CFG, MMA_CFG_VAL) &&
               set_reg(acc_addr, MMA_CTRL_REG1, MMA_CTRL1_VAL);
    default:
        return false;
    }
}

bool gy511_init(void)
{
    static const uint8_t candidates[] = { 0x19, 0x18, 0x1D, 0x1E, 0x53, 0x1C };
    bus_init();
    err_count = 0;
    acc_addr  = 0;
    chip      = GY511_CHIP_NONE;

    for (unsigned i = 0; i < sizeof candidates; i++) {
        gy511_chip_t c = identify(candidates[i]);
        if (c != GY511_CHIP_NONE) { chip = c; acc_addr = candidates[i]; break; }
    }
    if (chip == GY511_CHIP_NONE) return false;
    if (!configure()) { chip = GY511_CHIP_NONE; acc_addr = 0; return false; }

    sleep_ms(20);   /* let the first samples arrive */
    return true;
}

static int16_t le16(const uint8_t *b) { return (int16_t)((b[1] << 8) | b[0]); }
static int16_t be16(const uint8_t *b) { return (int16_t)((b[0] << 8) | b[1]); }

bool gy511_read(gy511_accel_t *out)
{
    uint8_t b[6];
    bool ok;
    switch (chip) {
    case GY511_CHIP_LSM303DLHC:
    case GY511_CHIP_LSM303AGR:
        ok = read_regs(acc_addr, DLHC_OUT | AUTO_INC_ST, b, 6);
        if (ok) {   /* 12-bit left-justified: shift 4, then 1 mg per count */
            out->x = (le16(&b[0]) >> 4) * 0.001f;
            out->y = (le16(&b[2]) >> 4) * 0.001f;
            out->z = (le16(&b[4]) >> 4) * 0.001f;
        }
        break;
    case GY511_CHIP_LSM303D:
        ok = read_regs(acc_addr, LSMD_OUT | AUTO_INC_ST, b, 6);
        if (ok) {   /* 16-bit, 0.061 mg per count at +-2 g */
            out->x = le16(&b[0]) * 0.000061f;
            out->y = le16(&b[2]) * 0.000061f;
            out->z = le16(&b[4]) * 0.000061f;
        }
        break;
    case GY511_CHIP_ADXL345:
        ok = read_regs(acc_addr, ADXL_OUT, b, 6);
        if (ok) {   /* full resolution: 3.9 mg per count */
            out->x = le16(&b[0]) * 0.0039f;
            out->y = le16(&b[2]) * 0.0039f;
            out->z = le16(&b[4]) * 0.0039f;
        }
        break;
    case GY511_CHIP_MMA845X:
        ok = read_regs(acc_addr, MMA_OUT, b, 6);
        if (ok) {   /* big endian, left-justified: 16384 counts per g at +-2 g */
            out->x = be16(&b[0]) / 16384.0f;
            out->y = be16(&b[2]) / 16384.0f;
            out->z = be16(&b[4]) / 16384.0f;
        }
        break;
    default:
        ok = false;
        break;
    }
    if (!ok) err_count++;
    return ok;
}

gy511_chip_t gy511_chip(void)        { return chip; }
uint8_t      gy511_address(void)     { return acc_addr; }
uint32_t     gy511_error_count(void) { return err_count; }

const char *gy511_chip_name(void)
{
    switch (chip) {
    case GY511_CHIP_LSM303DLHC: return "LSM303DLHC";
    case GY511_CHIP_LSM303AGR:  return "LSM303AGR";
    case GY511_CHIP_LSM303D:    return "LSM303D";
    case GY511_CHIP_ADXL345:    return "ADXL345";
    case GY511_CHIP_MMA845X:    return "MMA845x";
    default:                    return "none";
    }
}

void gy511_i2c_scan(void)
{
    bus_init();
    printf("I2C0 scan (SDA GP%d, SCL GP%d):\n", GY511_I2C_SDA_PIN, GY511_I2C_SCL_PIN);
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (!device_answers(addr)) continue;
        found++;
        gy511_chip_t c = identify(addr);
        /* ID bytes help identify an unknown chip: send these to Claude if needed */
        printf("  found 0x%02X  [id regs 0x00=%02X 0x0D=%02X 0x0F=%02X]  %s\n", addr,
               read_reg(addr, 0x00) & 0xFF, read_reg(addr, 0x0D) & 0xFF,
               read_reg(addr, 0x0F) & 0xFF,
               c == GY511_CHIP_NONE ? "(not a supported accelerometer)" : "<- accelerometer");
    }
    if (!found)
        printf("  nothing found: check VCC->3V3, GND, SDA->GP16, SCL->GP17\n");
}
