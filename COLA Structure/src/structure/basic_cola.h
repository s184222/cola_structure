#pragma once

#include <cstdint>

#include "./math_util.h"

#include <iostream>

class _BasicCOLA_ConstIterator
{
public:
	using PointerType = const int64_t*;
	using ReferenceType = const int64_t&;
public:
	_BasicCOLA_ConstIterator(const PointerType data, size_t size, size_t index) :
		m_Data(data), 
		m_Size(size),
		m_Index(index) { }

	_BasicCOLA_ConstIterator& operator++()
	{
		m_Index++;

		// Check if we are at the end of a layer
		if (isPO2(m_Index + 1))
		{
			// Get index of the first element in the next layer
			// or all ones if there are no layers left.
			m_Index = leastZeroBits(m_Size & (~m_Index));
		}

		return *this;
	}

	_BasicCOLA_ConstIterator operator++(int)
	{
		_BasicCOLA_ConstIterator itr = *this;
		++(*this);
		return itr;
	}

	_BasicCOLA_ConstIterator& operator--()
	{
		// Check if we are at the beginning of a layer
		if (isPO2(m_Index + 1))
		{
			// Get index after the last element in the previous layer
			m_Index = nextPO2MinusOne(m_Size & m_Index);
		}

		m_Index--;
		
		return *this;
	}

	_BasicCOLA_ConstIterator operator--(int)
	{
		_BasicCOLA_ConstIterator itr = *this;
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

	bool operator==(const _BasicCOLA_ConstIterator& other) const
	{
		return (m_Data == other.m_Data && m_Index == other.m_Index);
	}

	bool operator!=(const _BasicCOLA_ConstIterator& other) const
	{
		return !(*this == other);
	}

protected:
	const PointerType m_Data;
	const size_t m_Size;
	size_t m_Index;
};

class BasicCOLA
{
public:
	using ConstIterator = _BasicCOLA_ConstIterator;
public:
	BasicCOLA() :
		BasicCOLA::BasicCOLA(15) { }
	
	BasicCOLA(size_t initialCapacity);

	BasicCOLA(const BasicCOLA& other);

public:
	void add(int64_t value);

	bool contains(int64_t value) const;

	inline size_t size() const { return m_Size; }

	inline size_t capacity() const { return m_Capacity; }
	
	ConstIterator begin() const
	{
		return ConstIterator(m_Data, m_Size, leastZeroBits(m_Size));
	}

	ConstIterator end() const
	{
		return ConstIterator(m_Data, m_Size, ~0);
	}

private:
	void reallocData(size_t capacity);

private:
	int64_t* m_Data;
	size_t m_Capacity;
	size_t m_Size;
};