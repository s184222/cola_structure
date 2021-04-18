#pragma once

#include <cstdint>

static size_t nextPO2MinusOne(size_t x)
{
	// size_t must be at least 16-bit
	x = x | (x >>  1);
	x = x | (x >>  2);
	x = x | (x >>  4);
	x = x | (x >>  8);
#if SIZE_MAX >= UINT32_MAX
	// size_t is at least a 32-bit integer
	x = x | (x >> 16);
#endif
#if SIZE_MAX >= UINT64_MAX
	// size_t is at least a 64-bit integer
	x = x | (x >> 32);
#endif
	return x;
}

static size_t leastZeroBits(size_t x)
{
	// Return an integer where exactly all the
	// least significant zero bits are set.
	x = x | (x <<  1);
	x = x | (x <<  2);
	x = x | (x <<  4);
	x = x | (x <<  8);
#if SIZE_MAX >= UINT32_MAX
	x = x | (x << 16);
#endif
#if SIZE_MAX >= UINT64_MAX
	x = x | (x << 32);
#endif
	// Invert x to reveal zero bits
	return ~x;
}

static uint8_t popcount(size_t x)
{
	// Runs in log(x). Alternatively switch to
	// constant time algorithm.
	uint8_t count = 0;
	while (x)
	{
		count += x & 0x1;
		x >>= 1;
	}
	return count;
}

template <typename T>
static bool binarySearch(T value, T* data, size_t start, size_t end)
{
	// Perform basic binary search
	while (start < end)
	{
		const size_t m = (start + end) >> 1;

		if (value > data[m])
			start = m + 1;
		else if (value < data[m])
			end = m;
		else
			return true;
	}

	return false;
}

inline static bool isPO2(size_t x)
{
	return (x & (x - 1)) == 0;
}
