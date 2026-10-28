/*
 *	drv_gy511.h
 *	Subsystem 4 -- accelerometer driver for the GY-511 board.
 *
 *	The board on our car carries an LSM303D (found at I2C address 0x1D on
 *	the bench), not the LSM303DLHC a GY-511 usually has, so the driver
 *	detects the chip and supports: LSM303DLHC, LSM303AGR, LSM303D,
 *	ADXL345 and MMA845x.  Only the accelerometer is used.
 *
 *	Bus: the kernel's I2C driver, device "iica" (= I2C0), pins set in
 *	device/i2c/sysdepend/rp2040/i2c_cnf_sysdep.h (GP16 SDA, GP17 SCL).
 *	Call these functions ONLY from terrain_task, the owner of I2C0
 *	(TEAM.md section 3).
 */

#ifndef DRV_GY511_H
#define DRV_GY511_H

#include <tk/tkernel.h>
#include <stdbool.h>

/* One reading on the CHIP's own X/Y/Z axes, in g. */
typedef struct {
	float	x, y, z;
} gy511_accel_t;

typedef enum {
	GY511_CHIP_NONE = 0,
	GY511_CHIP_LSM303DLHC,	/* the usual GY-511 chip: 0x19 + magnetometer 0x1E */
	GY511_CHIP_LSM303AGR,
	GY511_CHIP_LSM303D,	/* our board: accelerometer + magnetometer at 0x1D */
	GY511_CHIP_ADXL345,
	GY511_CHIP_MMA845X
} gy511_chip_t;

/* Open the I2C device, find a supported chip and configure it
   (about 100 Hz, +-2 g).  Returns false if none answers. */
bool		gy511_init(void);

/* Read one sample.  Returns false on an I2C error or timeout. */
bool		gy511_read(gy511_accel_t *out);

gy511_chip_t	gy511_chip(void);
UB		gy511_address(void);
const char	*gy511_chip_name(void);
UW		gy511_error_count(void);	/* failed reads since init */

/* Print every address that answers on I2C0 (wiring check). */
void		gy511_i2c_scan(void);

#endif /* DRV_GY511_H */
