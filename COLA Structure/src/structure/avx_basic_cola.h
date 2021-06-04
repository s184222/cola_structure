#pragma once

#include <cstdint>

#include "./math_util.h"

class _AVXBasicCOLA_ConstIterator
{
public:
	using PointerType = const int32_t*;
	using ReferenceType = const int32_t&;

public:
	_AVXBasicCOLA_ConstIterator(const PointerType data, uint32_t size, uint32_t index) :
		m_Data(data),
		m_Size(size),
		m_Index(index) { }

	_AVXBasicCOLA_ConstIterator& operator++()
	{
		m_Index++;

		// Check if we are at the end of a layer
		if (isPO2(m_Index))
		{
			// Get index of the first element in the next layer
			// or all ones if there are no layers left.
			m_Index = leastZeroBits(m_Size & (~(m_Index - 1))) + 1;
		}

		return *this;
	}

	_AVXBasicCOLA_ConstIterator operator++(int)
	{
		_AVXBasicCOLA_ConstIterator itr = *this;
		++(*this);
		return itr;
	}

	_AVXBasicCOLA_ConstIterator& operator--()
	{
		// Check if we are at the beginning of a layer
		if (isPO2(m_Index))
		{
			// Get index after the last element in the previous layer
			m_Index = nextPO2MinusOne(m_Size & (m_Index - 1)) + 1;
		}

		m_Index--;

		return *this;
	}

	_AVXBasicCOLA_ConstIterator operator--(int)
	{
		_AVXBasicCOLA_ConstIterator itr = *this;
		--(*this);
		return itr;
	}

	PointerType operator->() const
	{
		return &m_Data[m_Index];
	}

	ReferenceType operator*() const
	{
		return m_Data[m_Index];
	}

	bool operator==(const _AVXBasicCOLA_ConstIterator& other) const
	{
		return (m_Data == other.m_Data && m_Index == other.m_Index);
	}

	bool operator!=(const _AVXBasicCOLA_ConstIterator& other) const
	{
		return !(*this == other);
	}

protected:
	const PointerType m_Data;
	const uint32_t m_Size;
	uint32_t m_Index;
};

class AVXBasicCOLA
{
public:
	using ConstIterator = _AVXBasicCOLA_ConstIterator;

public:
	AVXBasicCOLA() :
		AVXBasicCOLA::AVXBasicCOLA(16) { }

	AVXBasicCOLA(uint32_t initialCapacity);

	AVXBasicCOLA(const AVXBasicCOLA& other);

	~AVXBasicCOLA();

public:
	void add(int32_t value);

	bool contains(int32_t value) const;

	inline uint32_t size() const { return m_Size; }

	inline uint32_t capacity() const { return m_Capacity; }

	ConstIterator begin() const
	{
		return ConstIterator(m_Data, m_Size, leastZeroBits(m_Size) + 1);
	}

	ConstIterator end() const
	{
		return ConstIterator(m_Data, m_Size, 0);
	}

private:
	void allocateData(int32_t*& unalignedPtr, int32_t*& alignedPtr, uint32_t capacity) const;
	void reallocData(uint32_t capacity);

private:
	int32_t* m_DataUnaligned;
	int32_t* m_Data;
	uint32_t m_Capacity;
	uint32_t m_Size;
};
