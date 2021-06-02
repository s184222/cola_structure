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
	static const uint8_t nibblePopcount[16] = {
		/*0b0000: */ 0, /*0b0001: */ 1, /*0b0010: */ 1, /*0b0011: */ 2,
		/*0b0100: */ 1, /*0b0101: */ 2, /*0b0110: */ 2, /*0b0111: */ 3,
		/*0b1000: */ 1, /*0b1001: */ 2, /*0b1010: */ 2, /*0b1011: */ 3,
		/*0b1100: */ 2, /*0b1101: */ 3, /*0b1110: */ 3, /*0b1111: */ 4
	};

	// Runs in log(x)/4. Alternatively switch to constant time algorithm.
	uint8_t count = 0;
	while (x)
	{
		count += nibblePopcount[x & 0x1111];
		x >>= 4;
	}
	return count;
}

template <typename T>
static bool binarySearch(T value, T* data, size_t start, size_t end)
{
	// Perform basic binary search
	while (start < end)
	{
		const size_t m = start + ((end - start) >> 1);

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

inline static bool isPO2MinusOne(size_t x)
{
	return (x & (x + 1)) == 0;
}

template <typename T>
inline static T ceilDiv(T a, T b)
{
	return (a + b - 1) / b;
}
