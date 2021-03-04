#include "cola.h"

#include <memory>

COLA::COLA(size_t initialCapacity) :
	m_Data(nullptr),
	m_Capacity(0),
	m_Size(0)
{
	// Capacity must be a power of two minus 1 (and greater than zero)
	m_Capacity = initialCapacity ? nextPO2MinusOne(initialCapacity) : 15;
	m_Data = new int64_t[m_Capacity];
}

COLA::COLA(const COLA& other) :
	m_Data(new int64_t[other.m_Capacity]),
	m_Capacity(other.m_Capacity),
	m_Size(other.m_Size)
{
	// Copy instead of pointing to the same memory.
	memcpy(m_Data, other.m_Data, other.m_Capacity * sizeof(int64_t));
}

void COLA::add(int64_t value)
{
	const size_t nSize = m_Size + 1;
	if (nSize > m_Capacity)
	{
		// Allocate a new layer
		reallocData((m_Capacity << 1) + 1);
	}

	// Find first position of empty array (merge-layer)
	const size_t m = leastZeroBits(nSize);
	const size_t mEnd = (m << 1) + 1;

	m_Data[mEnd - 1] = value;

	// Iteratively merge arrays
	size_t i = 0;
	while (i != m)
	{
		// Index after last element in current layer
		const size_t iEnd = (i << 1) + 1;
		
		// Index of first element in merging layer and new index
		size_t j = mEnd - i - 1;
		size_t k = mEnd - iEnd - 1;

		// Simple merge sort (ascending order)
		while (i != iEnd && j != mEnd)
		{
			if (m_Data[i] < m_Data[j])
				m_Data[k++] = m_Data[i++];
			else
				m_Data[k++] = m_Data[j++];
		}

		// Copy remaining elements in current layer
		while (i != iEnd)
			m_Data[k++] = m_Data[i++];
	}

	m_Size = nSize;
}

void COLA::remove(int64_t value)
{

}

bool COLA::contains(int64_t value) const
{
	// Find index after the last element in the last layer.
	size_t iEnd = nextPO2MinusOne(m_Size);

	while (iEnd)
	{
		const size_t iStart = iEnd >> 1;

		// Check if the current layer is non-empty (when
		// the top set bit of iEnd is also set in m_Size).
		// E.g. of success (iEnd = 1111):
		//   0000 1111 & xxxx 1xxx   >    0000 0111
		// E.g. of failure (iEnd = 1111):
		//   0000 1111 & xxxx 0xxx   <=   0000 0111
		if ((iEnd & m_Size) > iStart)
		{
			// Binary search range (inclusive)
			size_t i = iStart;
			size_t j = iEnd - 1;
		
			// Perform basic binary search
			while (i <= j)
			{
				const size_t m = (i + j) >> 1;
		
				if (value > m_Data[m])
					i = m + 1;
				else if (value < m_Data[m])
					j = m - 1;
				else
					return true;
			}
		}

		// Go to previous layer
		iEnd = iStart;
	}

	return false;
}

void COLA::reallocData(size_t capacity)
{
	// Allocate and copy memory to new block
	int64_t* newBlock = new int64_t[capacity];
	memcpy(newBlock, m_Data, m_Capacity * sizeof(int64_t));

	// Delete and set old block
	delete[] m_Data;
	m_Data = newBlock;
	m_Capacity = capacity;
}
