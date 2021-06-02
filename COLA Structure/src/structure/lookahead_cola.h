#pragma once

#include<cstdint>

#include "./math_util.h"

#define FAKE_ELEMENT_INTERVAL static_cast<size_t>(4)
#define REAL_POINTER_MASK (SIZE_MAX >> 1)
#define FAKE_ELEMENT_FLAG (~(SIZE_MAX >> 1))

struct _LookaheadCOLA_Entry {
	int64_t m_Value;
	size_t m_Pointer;
};

class _LookaheadCOLA_ConstIterator
{
public:
	using Entry = _LookaheadCOLA_Entry;
	using PointerType = const int64_t*;
	using ReferenceType = const int64_t&;

public:
	_LookaheadCOLA_ConstIterator(const Entry* data, size_t size, size_t index) :
		m_Data(data),
		m_Size(size),
		m_Index(index)
	{
		m_LayerStartIndex = isPO2MinusOne(m_Index) ? m_Index : (nextPO2MinusOne(m_Index) >> 1);
	}

	_LookaheadCOLA_ConstIterator& operator++()
	{
		do
		{
			m_Index++;

			// Check if we are at the end of a layer
			if (isPO2MinusOne(m_Index))
			{
				// Get index of the first element in the next layer
				// or all ones if there are no layers left.
				m_LayerStartIndex = m_Index = leastZeroBits((m_Size << 1) & (~m_Index));
				// Skip empty elements by following the first pointer.
				if (m_Index != ~0 && (m_Data[m_Index].m_Pointer & FAKE_ELEMENT_FLAG) == 0)
					m_Index += m_Data[m_Index].m_Pointer;
			}
			// Skip fake elements
		} while (m_Index != ~0 && (m_Data[m_Index].m_Pointer & FAKE_ELEMENT_FLAG));

		return *this;
	}

	_LookaheadCOLA_ConstIterator operator++(int)
	{
		_LookaheadCOLA_ConstIterator itr = *this;
		++(*this);
		return itr;
	}

	_LookaheadCOLA_ConstIterator& operator--()
	{
		do {
			if (m_LayerStartIndex != ~0)
			{
				// Make sure we do not iterate empty elements
				size_t firstIndex = (m_Data[m_LayerStartIndex].m_Pointer & FAKE_ELEMENT_FLAG) ?
					m_LayerStartIndex : m_LayerStartIndex + m_Data[m_LayerStartIndex].m_Pointer;	
				if (m_Index <= firstIndex)
					m_Index = m_LayerStartIndex;
			}

			// Check if we are at the beginning of a layer
			if (isPO2MinusOne(m_Index))
			{
				// Get index after the last element in the previous layer
				m_Index = nextPO2MinusOne((m_Size << 1) & m_Index);
				m_LayerStartIndex = m_Index >> 1;
			}

			m_Index--;
		} while (m_Index != 0 && (m_Data[m_Index].m_Pointer & FAKE_ELEMENT_FLAG));

		return *this;
	}

	_LookaheadCOLA_ConstIterator operator--(int)
	{
		_LookaheadCOLA_ConstIterator itr = *this;
		--(*this);
		return itr;
	}

	PointerType operator->() const
	{
		return &m_Data[m_Index].m_Value;
	}

	ReferenceType operator*() const
	{
		return m_Data[m_Index].m_Value;
	}

	bool operator==(const _LookaheadCOLA_ConstIterator& other) const
	{
		return (m_Data == other.m_Data && m_Index == other.m_Index);
	}

	bool operator!=(const _LookaheadCOLA_ConstIterator& other) const
	{
		return !(*this == other);
	}

protected:
	const Entry* m_Data;
	const size_t m_Size;
	size_t m_Index;
	size_t m_LayerStartIndex;
};

class LookaheadCOLA
{
private:
	using Entry = _LookaheadCOLA_Entry;
	using ConstIterator = _LookaheadCOLA_ConstIterator;

public:
	LookaheadCOLA() :
		LookaheadCOLA::LookaheadCOLA(15) { }

	LookaheadCOLA(size_t initialCapacity);

	LookaheadCOLA(const LookaheadCOLA& other);

public:
	void add(int64_t value);

	bool contains(int64_t value) const;

	bool predecessor(int64_t value, int64_t& result) const;

	inline size_t size() const { return m_Size; }

	inline size_t capacity() const { return m_Capacity; }

	ConstIterator begin() const
	{
		size_t index = leastZeroBits(m_Size << 1);
		// Skip empty elements
		if ((m_Data[index].m_Pointer & FAKE_ELEMENT_FLAG) == 0)
			index += m_Data[index].m_Pointer;
		// Skip fake elements
		while (m_Data[index].m_Pointer & FAKE_ELEMENT_FLAG)
			index++;
		return ConstIterator(m_Data, m_Size, index);
	}

	ConstIterator end() const
	{
		return ConstIterator(m_Data, m_Size, ~0);
	}
private:
	void reallocData(size_t capacity);

private:
	Entry* m_Data;
	size_t m_Capacity;
	size_t m_Size;
};