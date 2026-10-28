/*
 *	drv_gy511.c
 *	Subsystem 4 -- accelerometer driver (GY-511 and compatible boards).
 *
 *	Same chip detection and settings as the bench-tested Pico SDK version
 *	(testing/subsystem4/imu_bench/), but the bus calls go through the
 *	kernel's I2C driver ("iica") so they are safe under micro T-Kernel.
 *
 *	  Chip            Address        ID register         Data
 *	  LSM303DLHC/AGR  0x19 (0x18)    0x0F = 0x33 (AGR)   0x28.., LE, 12-bit left-justified
 *	  LSM303D         0x1D (0x1E)    0x0F = 0x49         0x28.., LE, 16-bit
 *	  ADXL345         0x53 (0x1D)    0x00 = 0xE5         0x32.., LE, 3.9 mg/count
 *	  MMA845x         0x1D (0x1C)    0x0D = 0x1A/2A/3A   0x01.., BE, 16-bit left-justified
 *
 *	Every bus call is bounded by the driver timeout DEVCNF_I2C0_TMO
 *	(device/i2c/sysdepend/rp2040/i2c_cnf_sysdep.h), so a dead sensor makes
 *	a call fail instead of blocking the task.
 */

#include "drv_gy511.h"
#include <tm/tmonitor.h>
#include "../../device/include/dev_i2c.h"	/* T_I2C_EXEC, TDN_I2C_EXEC */

#define GY511_DEVNM	"iica"		/* kernel I2C driver, unit a = I2C0 */

/* ---- per-chip register values ---- */
/* LSM303DLHC / LSM303AGR (accelerometer part) */
#define DLHC_CTRL1	0x20
#define DLHC_CTRL4	0x23
#define DLHC_OUT	0x28
#define DLHC_CTRL1_VAL	0x57		/* 100 Hz, normal, X/Y/Z on       */
#define DLHC_CTRL4_VAL	0x88		/* BDU, +-2 g, high resolution     */
/* LSM303D */
#define LSMD_CTRL1	0x20
#define LSMD_CTRL2	0x21
#define LSMD_OUT	0x28
#define LSMD_CTRL1_VAL	0x6F		/* 100 Hz, BDU, X/Y/Z on           */
#define LSMD_CTRL2_VAL	0x00		/* +-2 g                           */
/* ADXL345 */
#define ADXL_BW_RATE	0x2C
#define ADXL_POWER_CTL	0x2D
#define ADXL_DATA_FMT	0x31
#define ADXL_OUT	0x32
#define ADXL_BW_VAL	0x0A		/* 100 Hz                          */
#define ADXL_FMT_VAL	0x08		/* full resolution, +-2 g          */
#define ADXL_POWER_VAL	0x08		/* measure mode                    */
/* MMA8451 / 8452 / 8453 */
#define MMA_CTRL1	0x2A
#define MMA_XYZ_CFG	0x0E
#define MMA_OUT		0x01
#define MMA_CTRL1_VAL	0x19		/* 100 Hz, active                  */
#define MMA_CFG_VAL	0x00		/* +-2 g                           */

#define AUTO_INC_ST	0x80		/* ST chips: bit 7 = auto-increment */

LOCAL ID		dd = -1;	/* device descriptor of "iica" */
LOCAL UB		acc_addr;
LOCAL gy511_chip_t	chip;
LOCAL UW		err_count;

/* ---------------- bus layer: the only part that touches I2C ---------------- */

LOCAL bool bus_open(void)
{
	if (dd < E_OK) {
		dd = tk_opn_dev((UB*)GY511_DEVNM, TD_UPDATE);
	}
	return dd >= E_OK;
}

LOCAL bool write_reg(UB addr, UB reg, UB val)
{
	UB	buf[2] = { reg, val };
	SZ	asz;
	return tk_swri_dev(dd, addr, buf, sizeof buf, &asz) == E_OK;
}

/* Write the register address, then (repeated start) read len bytes. */
LOCAL bool read_regs(UB addr, UB reg, UB *dst, SZ len)
{
	T_I2C_EXEC	ex;
	SZ		asz;

	ex.sadr     = addr;
	ex.snd_size = 1;
	ex.snd_data = &reg;
	ex.rcv_size = len;
	ex.rcv_data = dst;
	return tk_swri_dev(dd, TDN_I2C_EXEC, &ex, sizeof ex, &asz) == E_OK;
}

LOCAL INT read_reg(UB addr, UB reg)		/* -1 on error */
{
	UB v;
	return read_regs(addr, reg, &v, 1) ? (INT)v : -1;
}

LOCAL bool device_answers(UB addr)
{
	UB	dummy;
	SZ	asz;
	return tk_srea_dev(dd, addr, &dummy, 1, &asz) == E_OK;
}

/* ---------------- chip handling (unchanged from the bench version) ---------------- */

LOCAL bool set_reg(UB addr, UB reg, UB val)
{
	return write_reg(addr, reg, val) && read_reg(addr, reg) == (INT)val;
}

LOCAL gy511_chip_t identify(UB addr)
{
	INT st_who, mma;

	if (!device_answers(addr)) return GY511_CHIP_NONE;

	st_who = read_reg(addr, 0x0F);
	if (addr == 0x19 || addr == 0x18) {
		/* DLHC has no WHO_AM_I; AGR (and LIS3DH) read 0x33. Same registers. */
		return (st_who == 0x33) ? GY511_CHIP_LSM303AGR : GY511_CHIP_LSM303DLHC;
	}
	if (st_who == 0x49) return GY511_CHIP_LSM303D;
	if (read_reg(addr, 0x00) == 0xE5) return GY511_CHIP_ADXL345;
	mma = read_reg(addr, 0x0D);
	if (mma == 0x1A || mma == 0x2A || mma == 0x3A) return GY511_CHIP_MMA845X;
	return GY511_CHIP_NONE;
}

LOCAL bool configure(void)
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
		return set_reg(acc_addr, ADXL_BW_RATE, ADXL_BW_VAL) &&
		       set_reg(acc_addr, ADXL_DATA_FMT, ADXL_FMT_VAL) &&
		       set_reg(acc_addr, ADXL_POWER_CTL, ADXL_POWER_VAL);
	case GY511_CHIP_MMA845X:
		/* settings can only be changed in standby */
		return set_reg(acc_addr, MMA_CTRL1, 0x00) &&
		       set_reg(acc_addr, MMA_XYZ_CFG, MMA_CFG_VAL) &&
		       set_reg(acc_addr, MMA_CTRL1, MMA_CTRL1_VAL);
	default:
		return false;
	}
}

bool gy511_init(void)
{
	static const UB candidates[] = { 0x19, 0x18, 0x1D, 0x1E, 0x53, 0x1C };
	UINT i;

	err_count = 0;
	acc_addr  = 0;
	chip      = GY511_CHIP_NONE;
	if (!bus_open()) return false;

	for (i = 0; i < sizeof candidates; i++) {
		gy511_chip_t c = identify(candidates[i]);
		if (c != GY511_CHIP_NONE) { chip = c; acc_addr = candidates[i]; break; }
	}
	if (chip == GY511_CHIP_NONE) return false;
	if (!configure()) { chip = GY511_CHIP_NONE; acc_addr = 0; return false; }

	tk_dly_tsk(20);		/* let the first samples arrive */
	return true;
}

LOCAL H le16(const UB *b) { return (H)((b[1] << 8) | b[0]); }
LOCAL H be16(const UB *b) { return (H)((b[0] << 8) | b[1]); }

bool gy511_read(gy511_accel_t *out)
{
	UB	b[6];
	bool	ok;

	switch (chip) {
	case GY511_CHIP_LSM303DLHC:
	case GY511_CHIP_LSM303AGR:
		ok = read_regs(acc_addr, DLHC_OUT | AUTO_INC_ST, b, 6);
		if (ok) {	/* 12-bit left-justified: shift 4, then 1 mg per count */
			out->x = (le16(&b[0]) >> 4) * 0.001f;
			out->y = (le16(&b[2]) >> 4) * 0.001f;
			out->z = (le16(&b[4]) >> 4) * 0.001f;
		}
		break;
	case GY511_CHIP_LSM303D:
		ok = read_regs(acc_addr, LSMD_OUT | AUTO_INC_ST, b, 6);
		if (ok) {	/* 16-bit, 0.061 mg per count at +-2 g */
			out->x = le16(&b[0]) * 0.000061f;
			out->y = le16(&b[2]) * 0.000061f;
			out->z = le16(&b[4]) * 0.000061f;
		}
		break;
	case GY511_CHIP_ADXL345:
		ok = read_regs(acc_addr, ADXL_OUT, b, 6);
		if (ok) {	/* full resolution: 3.9 mg per count */
			out->x = le16(&b[0]) * 0.0039f;
			out->y = le16(&b[2]) * 0.0039f;
			out->z = le16(&b[4]) * 0.0039f;
		}
		break;
	case GY511_CHIP_MMA845X:
		ok = read_regs(acc_addr, MMA_OUT, b, 6);
		if (ok) {	/* big endian, left-justified: 16384 counts per g */
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

gy511_chip_t	gy511_chip(void)	{ return chip; }
UB		gy511_address(void)	{ return acc_addr; }
UW		gy511_error_count(void)	{ return err_count; }

const char *gy511_chip_name(void)
{
	switch (chip) {
	case GY511_CHIP_LSM303DLHC:	return "LSM303DLHC";
	case GY511_CHIP_LSM303AGR:	return "LSM303AGR";
	case GY511_CHIP_LSM303D:	return "LSM303D";
	case GY511_CHIP_ADXL345:	return "ADXL345";
	case GY511_CHIP_MMA845X:	return "MMA845x";
	default:			return "none";
	}
}

void gy511_i2c_scan(void)
{
	UB	addr;
	INT	found = 0;

	if (!bus_open()) {
		tm_printf((UB*)"terrain: cannot open %s (is DEVCNF_USE_IIC = 1?)\n", GY511_DEVNM);
		return;
	}
	tm_printf((UB*)"terrain: I2C0 scan (SDA GP16, SCL GP17):\n");
	for (addr = 0x08; addr < 0x78; addr++) {
		if (!device_answers(addr)) continue;
		found++;
		tm_printf((UB*)"terrain:   found 0x%02x  [id 0x00=%02x 0x0D=%02x 0x0F=%02x]\n",
			  addr, read_reg(addr, 0x00) & 0xFF,
			  read_reg(addr, 0x0D) & 0xFF, read_reg(addr, 0x0F) & 0xFF);
	}
	if (!found) {
		tm_printf((UB*)"terrain:   nothing found: check VIN->3V3, GND, SDA->GP16, SCL->GP17\n");
	}
}
