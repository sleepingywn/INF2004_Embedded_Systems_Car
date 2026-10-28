/*
 *	imu_math.h
 *	Subsystem 4 -- the few float maths functions imu_logic.c needs.
 *
 *	The template build does not link the C maths library (libm), so
 *	sqrtf / atan2f / sinf are not available.  These replacements are
 *	self-contained, so subsystem 4 does not have to change the shared
 *	makefile.  Checked against libm on the laptop
 *	(testing/subsystem4/imu_sim/math_check.c):
 *	  imu_sqrtf   relative error < 1e-6
 *	  imu_atan2f  error < 1e-5 rad (0.001 deg)
 *	  imu_sinf    error < 1e-6
 */

#ifndef IMU_MATH_H
#define IMU_MATH_H

#include <stdint.h>

#define IMU_PI		3.14159265f
#define IMU_HALF_PI	1.57079633f

static inline float imu_fabsf(float x)
{
	return (x < 0.0f) ? -x : x;
}

/* Square root: bit-level first guess, then 3 Newton steps. */
static inline float imu_sqrtf(float x)
{
	union { float f; uint32_t u; } v;
	float y;

	if (x <= 0.0f) return 0.0f;
	v.f = x;
	v.u = 0x1FBD1DF5u + (v.u >> 1);	/* rough sqrt via the exponent */
	y = v.f;
	y = 0.5f * (y + x / y);
	y = 0.5f * (y + x / y);
	y = 0.5f * (y + x / y);
	return y;
}

/* atan(z) for |z| <= 1: minimax polynomial, error about 1e-5 rad. */
static inline float imu_atan_unit(float z)
{
	float z2 = z * z;
	return z * (0.99997726f + z2 * (-0.33262347f + z2 * (0.19354346f
		+ z2 * (-0.11643287f + z2 * (0.05265332f + z2 * -0.01172120f)))));
}

/* atan2(y, x) in radians, -pi..+pi, same conventions as the C library. */
static inline float imu_atan2f(float y, float x)
{
	float ax = imu_fabsf(x), ay = imu_fabsf(y), a;

	if (ax == 0.0f && ay == 0.0f) return 0.0f;
	if (ax >= ay) a = imu_atan_unit(ay / ax);		/* |angle| <= 45 deg */
	else          a = IMU_HALF_PI - imu_atan_unit(ax / ay);
	if (x < 0.0f) a = IMU_PI - a;
	return (y < 0.0f) ? -a : a;
}

/* sin(x): fold into -pi/2..pi/2, then a Taylor series to x^11. */
static inline float imu_sinf(float x)
{
	float x2;

	while (x >  IMU_PI) x -= 2.0f * IMU_PI;
	while (x < -IMU_PI) x += 2.0f * IMU_PI;
	if (x >  IMU_HALF_PI) x =  IMU_PI - x;
	if (x < -IMU_HALF_PI) x = -IMU_PI - x;
	x2 = x * x;
	return x * (1.0f + x2 * (-1.0f / 6 + x2 * (1.0f / 120 + x2 * (-1.0f / 5040
		+ x2 * (1.0f / 362880 + x2 * (-1.0f / 39916800))))));
}

#endif /* IMU_MATH_H */
