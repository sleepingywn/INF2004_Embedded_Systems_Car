/*
 *----------------------------------------------------------------------
 *    Device Driver for μT-Kernel 3.00.05
 *
 *    Copyright (C) 2020-2021 by Ken Sakamura.
 *    This software is distributed under the T-License 2.2.
 *----------------------------------------------------------------------
 *
 *    Released by TRON Forum(http://www.tron.org) at 2021/11.
 *
 *----------------------------------------------------------------------
 */


/*
 *	config_device.h
 *	Device configuration definition
 */

#ifndef	__DEV_CONFIG_H__
#define	__DEV_CONFIG_H__

/* ------------------------------------------------------------------------ */
/* Device usage settings
 *	1: Use   0: Do not use
 */

#define DEVCNF_USE_SER		1		// Serial communication device 
#define DEVCNF_USE_ADC		0		// S3 owns ADC0-2 and ADC IRQ 22
		/* ir_acquisition.c provides 1 kHz acquisition for line/barcode.
		   Do not also enable the sample ADC device driver. */
#define DEVCNF_USE_IIC		1		// I2C communication device
		/* On for subsystem 4 (GY-511 on I2C0, GP16/GP17; TEAM.md).
		   Pins are set in device/i2c/sysdepend/rp2040/i2c_cnf_sysdep.h. */

#endif	/* __DEV_CONFIG_H__ */
