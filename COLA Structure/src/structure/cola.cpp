#include "cola.h"

#include <memory>

static uint32_t ceilPO2(uint32_t x)
{
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >> 16);
	return x + 1;
}

static uint32_t getLSBIndex(unsigned int v)
{
	// See http://graphics.stanford.edu/~seander/bithacks.html
	static const uint32_t MultiplyDeBruijnBitPosition[32] =
	{
	  0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
	  31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	};

	// Find the number of trailing zeros in 32-bit v
	return MultiplyDeBruijnBitPosition[((uint32_t)((v & (1 + ~v)) * 0x077CB531U)) >> 27];
}

COLA::COLA(size_t initialCapacity) :
	m_Data(nullptr),
	m_Capacity(0),
	m_Size(0)
{
	// Capacity must be a power of two minus 1 (and greater than zero)
	m_Capacity = (initialCapacity > 0) ? (ceilPO2(initialCapacity) - 1) : 15;
	m_Data = new int64_t[m_Capacity];
}

void COLA::add(int64_t value)
{
	const size_t nSize = m_Size + 1;
	if (nSize >= m_Capacity)
	{
		// Allocate a new layer
		reallocData((m_Capacity << 1) + 1);
	}

	// Find first position of empty array (merge layer)
	const uint32_t mIndex = (1 << getLSBIndex(nSize)) - 1;
	const uint32_t mEnd = (mIndex << 1) + 1;

	uint32_t count = 1;
	m_Data[mEnd - count] = value;

	// Iteratively merge arrays
	uint32_t i = 0;
	while (i != mIndex)
	{
		// Index after last element in current layer
		uint32_t lEnd = i << 1;
		
		// Index of first element in merging layer and new index
		uint32_t j = mEnd - count;
		uint32_t k = mEnd - (count << 1);

		// Simple merge sort (ascending order)
		while (i != lEnd && j != mEnd)
		{
			if (m_Data[i] < m_Data[j])
				m_Data[k++] = m_Data[i++];
			else
				m_Data[k++] = m_Data[j++];
		}

		// Copy remaining elements in current layer
		while (i < lEnd)
			m_Data[j++] = m_Data[i++];

		// Copy remaining elements in merging layer
		while (j < mEnd)
			m_Data[j++] = m_Data[i++];

		count = count << 1;
	}

	m_Size = nSize;
}

void COLA::remove(int64_t value)
{

}

bool COLA::contains(int64_t value) const
{
	return false;
}

void COLA::reallocData(size_t capacity)
{
	// Allocate and copy memory to new block
	int64_t* newBlock = new int64_t[capacity];
	memcpy(newBlock, m_Data, m_Size * sizeof(int64_t));

	// Delete and set old block
	delete[] m_Data;
	m_Data = newBlock;
	m_Capacity = capacity;
}
