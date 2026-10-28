/*
 * drv_gy511.h  -  Buddy 4: accelerometer driver (GY-511 and compatible boards)
 *
 * Detects and supports LSM303DLHC, LSM303AGR, LSM303D, ADXL345 and MMA845x.
 * Only the accelerometer is used (any magnetometer is ignored).
 * Wiring: I2C0, SDA = GP16, SCL = GP17, VCC = 3V3, GND = GND.
 *
 * Every function returns false instead of hanging if the sensor does not
 * answer, so the caller can set the IMU fault bit and carry on.
 */
#ifndef DRV_GY511_H
#define DRV_GY511_H

#include <stdbool.h>
#include <stdint.h>

#define GY511_I2C_SDA_PIN   16
#define GY511_I2C_SCL_PIN   17
#define GY511_I2C_BAUD      400000      /* 400 kHz "fast mode" */

/* Raw chip-axis reading in g. These are the SENSOR's X/Y/Z, not the car's;
 * the caller maps them to forward / side / up. */
typedef struct {
    float x, y, z;
} gy511_accel_t;

typedef enum {
    GY511_CHIP_NONE = 0,
    GY511_CHIP_LSM303DLHC,    /* the usual GY-511 chip: 0x19 + magnetometer 0x1E */
    GY511_CHIP_LSM303AGR,
    GY511_CHIP_LSM303D,       /* accelerometer + magnetometer at one address, 0x1D */
    GY511_CHIP_ADXL345,
    GY511_CHIP_MMA845X
} gy511_chip_t;

/* Set up the I2C pins, find a supported accelerometer and configure it
 * (about 100 Hz, +-2 g). Returns false if none answers or setup fails. */
bool gy511_init(void);

/* Read one sample. Returns false on an I2C error or timeout. */
bool gy511_read(gy511_accel_t *out);

/* Which chip / address was found (valid after a successful gy511_init). */
gy511_chip_t gy511_chip(void);
uint8_t      gy511_address(void);
const char  *gy511_chip_name(void);

/* Number of failed reads since init (for the fault / evidence log). */
uint32_t     gy511_error_count(void);

/* Print every address that answers on I2C0 (wiring check). Needs stdio. */
void gy511_i2c_scan(void);

#endif
