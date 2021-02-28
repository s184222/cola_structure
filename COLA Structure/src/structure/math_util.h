#pragma once

#include <cstdint>

static uint32_t nextPO2(uint32_t x)
{
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >> 16);
	return x + 1;
}

inline static bool isPO2(uint32_t x)
{
	return (x & (x - 1)) == 0;
}

static uint32_t lsbIndex(uint32_t x)
{
	// Return the index of the least significant one bit.
	// See http://graphics.stanford.edu/~seander/bithacks.html
	static const uint32_t MultiplyDeBruijnBitPosition[32] =
	{
	  0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
	  31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	};

	// Find the number of trailing zeros in 32-bit v
	return MultiplyDeBruijnBitPosition[((uint32_t)((x & (1 + ~x)) * 0x077CB531U)) >> 27];
}
