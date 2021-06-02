#include "./lookahead_cola.h"

#include <memory>
#include <algorithm>

LookaheadCOLA::LookaheadCOLA(size_t initialCapacity) :
	m_Data(nullptr),
	m_Capacity(0),
	m_Size(0)
{
	// Capacity must be a power of two minus one (and greater than zero).
	m_Capacity = std::max(static_cast<size_t>(15), nextPO2MinusOne(initialCapacity));
	m_Data = new Entry[m_Capacity];

	// Store fake element with offset zero to ensure adding and
	// searching works correctly when the cola is empty.
	m_Data[0].m_Pointer = 0 | FAKE_ELEMENT_FLAG;
}

LookaheadCOLA::LookaheadCOLA(const LookaheadCOLA& other) :
	m_Data(new Entry[other.m_Capacity]),
	m_Capacity(other.m_Capacity),
	m_Size(other.m_Size)
{
	// Copy instead of pointing to the same memory.
	memcpy(m_Data, other.m_Data, other.m_Capacity * sizeof(Entry));
}

void LookaheadCOLA::add(int64_t value)
{
	const size_t nSize = m_Size + 1;
	// Note: at most half of the elements of a full cola are fake, so
	//       2N + 1 is the upper bound on the actual amount of elements.
	if ((nSize << 1) + 1 > m_Capacity)
	{
		// Allocate a new layer
		reallocData((m_Capacity << 1) + 1);
	}

	// Find first position of empty array (merge-layer)
	const size_t m = leastZeroBits(nSize << 1);
	const size_t mEnd = (m << 1) + 1;

	// Store the start of the current merge. The starting position
	// of lookahead pointers is stored as the pointer of the first
	// element in the layer.
	size_t s = isPO2(nSize) ? mEnd : (m + m_Data[m].m_Pointer);

	// In case we do not have any fake elements to the left.
	m_Data[s - 1].m_Pointer = 0;
	// Make space for and insert value into correct position.
	size_t j = s;
	for (; j < mEnd && m_Data[j].m_Value < value; j++)
		m_Data[j - 1] = m_Data[j];
	m_Data[j - 1].m_Value = value;
	m_Data[j - 1].m_Pointer &= REAL_POINTER_MASK;

	// We have inserted one element.
	s--;

	// Iteratively merge arrays
	size_t i = 0;
	while (i != m)
	{
		// Index after last element in current layer
		const size_t iEnd = (i << 1) + 1;

		// Index of first element in merging layer and new index
		j = s;
		size_t k = s - ((i + 1) >> 1);
		s = k;

		// Set i to the first element in layer
		if ((m_Data[i].m_Pointer & FAKE_ELEMENT_FLAG) == 0)
			i += m_Data[i].m_Pointer;

		// Keep track of closest fake lookahead pointer to the left.
		size_t p = 0;

		// Simple merge sort (ascending order)
		while (i != iEnd && j != mEnd)
		{
			if (m_Data[i].m_Pointer & FAKE_ELEMENT_FLAG)
				i++;
			else
			{
				if (m_Data[i].m_Value <= m_Data[j].m_Value)
					m_Data[k++] = { m_Data[i++].m_Value, p };
				else
				{
					// Set real lookahead pointer for next merged element
					p = m_Data[j].m_Pointer & REAL_POINTER_MASK;	
					m_Data[k++] = m_Data[j++];
				}
			}
		}

		// Copy remaining elements in current layer
		while (i != iEnd)
		{
			if (m_Data[i].m_Pointer & FAKE_ELEMENT_FLAG)
				i++;
			else
				m_Data[k++] = { m_Data[i++].m_Value, p };
		}
	}

	// Store relative pointer for the first element in layer.
	if (s != m || (m_Data[m].m_Pointer & FAKE_ELEMENT_FLAG) == 0)
		m_Data[m].m_Pointer = s - m;

	// Copy lookahead pointers to layers below
	for (i = m; i != 0; )
	{
		// Compute number of fake elements in the previous layer.
		const size_t c = ceilDiv((i << 1) + 1 - s, FAKE_ELEMENT_INTERVAL);
	
		for (j = i - c; j < i; j++)
		{
			m_Data[j].m_Value   = m_Data[s].m_Value;
			m_Data[j].m_Pointer = s | FAKE_ELEMENT_FLAG;
			s += FAKE_ELEMENT_INTERVAL;
		}

		s = i - c;
		i >>= 1;

		// Store relative pointer to first fake lookahead pointer.
		if (s != i)
			m_Data[i].m_Pointer = s - i;
	}

	m_Size = nSize;
}

bool LookaheadCOLA::contains(int64_t value) const
{
	// First element (fake or not) is always the smallest
	// element in the cola.
	if (m_Size > 0 && m_Data[0].m_Value >= value)
		return (value == m_Data[0].m_Value);

	size_t pointer = m_Data[0].m_Pointer & REAL_POINTER_MASK;

	while (pointer)
	{
		// Search for the best predecessor in current layer
		for (size_t i = 1; i < FAKE_ELEMENT_INTERVAL; i++)
		{
			// Check if next element is the first element in the next layer, or if the
			// next element can not be a predecessor. In either case, previous element
			// was the predecessor in the current layer.
			if (isPO2MinusOne(pointer + 1) || m_Data[pointer + 1].m_Value > value)
				break;

			pointer++;
		}

		// Check if we found the value
		if (m_Data[pointer].m_Value == value)
			return true;

		// Follow predecessor pointer to next layer
		pointer = m_Data[pointer].m_Pointer & REAL_POINTER_MASK;
	}

	return false;
}

bool LookaheadCOLA::predecessor(int64_t value, int64_t& result) const
{
	if (m_Size > 0 && m_Data[0].m_Value <= value)
	{
		int64_t best = m_Data[0].m_Value;
		size_t pointer = m_Data[0].m_Pointer & REAL_POINTER_MASK;

		while (pointer && value != best)
		{
			// Search for the best predecessor in current layer
			for (size_t i = 1; i < FAKE_ELEMENT_INTERVAL; i++)
			{
				if (isPO2MinusOne(pointer + 1) || m_Data[pointer + 1].m_Value > value)
					break;

				pointer++;
			}
			// Check if result is better than current best
			if (m_Data[pointer].m_Value > best)
				best = m_Data[pointer].m_Value;
			// Follow predecessor pointer to next layer
			pointer = m_Data[pointer].m_Pointer & REAL_POINTER_MASK;
		}

		result = best;
		return true;
	}

	return false;
}

void LookaheadCOLA::reallocData(size_t capacity)
{
	// Allocate and copy memory to new block
	Entry* newBlock = new Entry[capacity];
	const size_t c = (capacity > m_Capacity) ? m_Capacity : capacity;
	memcpy(newBlock, m_Data, c * sizeof(Entry));

	// Delete and set old block
	delete[] m_Data;
	m_Data = newBlock;
	m_Capacity = capacity;
}
