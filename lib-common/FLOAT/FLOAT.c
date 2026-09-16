#include "FLOAT.h"
#include <stdint.h>

FLOAT F_mul_F(FLOAT a, FLOAT b) {
	int64_t product = (int64_t)a * b;
	return product >> 16;
}

FLOAT F_div_F(FLOAT a, FLOAT b) {
	/* Dividing two 64-bit integers needs the support of another library
	 * `libgcc', other than newlib. It is a dirty work to port `libgcc'
	 * to NEMU. In fact, it is unnecessary to perform a "64/64" division
	 * here. A "64/32" division is enough.
	 *
	 * To perform a "64/32" division, you can use the x86 instruction
	 * `div' or `idiv' by inline assembly. We provide a template for you
	 * to prevent you from uncessary details.
	 *
	 *     asm volatile ("??? %2" : "=a"(???), "=d"(???) : "r"(???), "a"(???), "d"(???));
	 *
	 * If you want to use the template above, you should fill the "???"
	 * correctly. For more information, please read the i386 manual for
	 * division instructions, and search the Internet about "inline assembly".
	 * It is OK not to use the template above, but you should figure
	 * out another way to perform the division.
	 */

	int32_t quotient, remainder;
	uint32_t dividend_low = (uint32_t)a << 16;
	int32_t dividend_high = a >> 16;

	asm volatile ("idivl %2"
			: "=a"(quotient), "=d"(remainder)
			: "r"(b), "a"(dividend_low), "d"(dividend_high));

	return quotient;
}

FLOAT f2F(float a) {
	/* You should figure out how to convert `a' into FLOAT without
	 * introducing x87 floating point instructions. Else you can
	 * not run this code in NEMU before implementing x87 floating
	 * point instructions, which is contrary to our expectation.
	 *
	 * Hint: The bit representation of `a' is already on the
	 * stack. How do you retrieve it to another variable without
	 * performing arithmetic operations on it directly?
	 */

	union {
		float f;
		uint32_t u;
	} value = { .f = a };
	uint32_t sign = value.u >> 31;
	uint32_t exponent = (value.u >> 23) & 0xff;
	uint32_t fraction = (value.u & 0x7fffff) | 0x800000;
	uint32_t magnitude;
	int shift;

	/* A denormalized IEEE-754 float is too small to be represented in
	 * Q16.16, so it becomes zero after truncation. */
	if(exponent == 0) {
		return 0;
	}

	/* value * 2^16 = fraction * 2^(exponent - 134). */
	shift = 134 - exponent;
	if(shift >= 32) {
		magnitude = 0;
	}
	else if(shift >= 0) {
		magnitude = fraction >> shift;
	}
	else {
		magnitude = fraction << -shift;
	}

	return sign ? -(FLOAT)magnitude : (FLOAT)magnitude;
}

FLOAT Fabs(FLOAT a) {
	return a < 0 ? -a : a;
}

/* Functions below are already implemented */

FLOAT sqrt(FLOAT x) {
	FLOAT dt, t = int2F(2);

	do {
		dt = F_div_int((F_div_F(x, t) - t), 2);
		t += dt;
	} while(Fabs(dt) > f2F(1e-4));

	return t;
}

FLOAT pow(FLOAT x, FLOAT y) {
	/* we only compute x^0.333 */
	FLOAT t2, dt, t = int2F(2);

	do {
		t2 = F_mul_F(t, t);
		dt = (F_div_F(x, t2) - t) / 3;
		t += dt;
	} while(Fabs(dt) > f2F(1e-4));

	return t;
}

