#pragma once

#include <cstdint>

#include "./math_util.h"

#include <iostream>

class _COLA_ConstIterator
{
public:
	using PointerType = const int64_t*;
	using ReferenceType = const int64_t&;
public:
	_COLA_ConstIterator(PointerType data, size_t size, size_t index)
		: m_Data(data), 
		  m_Size(size),
		  m_Index(index) { }

	_COLA_ConstIterator& operator++()
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

	_COLA_ConstIterator operator++(int)
	{
		_COLA_ConstIterator itr = *this;
		++(*this);
		return itr;
	}

	_COLA_ConstIterator& operator--()
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

	_COLA_ConstIterator operator--(int)
	{
		_COLA_ConstIterator itr = *this;
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

	bool operator==(const _COLA_ConstIterator& other) const
	{
		return (m_Data == other.m_Data && m_Index == other.m_Index);
	}

	bool operator!=(const _COLA_ConstIterator& other) const
	{
		return !(*this == other);
	}

protected:
	const PointerType m_Data;
	const size_t m_Size;
	size_t m_Index;
};

class COLA
{
public:
	using ConstIterator = _COLA_ConstIterator;
public:
	COLA()
		: COLA::COLA(15) { }
	
	COLA(size_t initialCapacity);

	COLA(const COLA& other);

public:
	void add(int64_t value);

	void remove(int64_t value);

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