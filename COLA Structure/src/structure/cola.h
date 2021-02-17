#pragma once

#include <cstdint>

class COLA
{
public:
	COLA() :
		COLA::COLA(15)
	{
	}
	
	COLA(size_t initialCapacity);

public:
	void add(int64_t value);

	void remove(int64_t value);

	bool contains(int64_t value) const;

	inline size_t size() const { return m_Size; }

	inline size_t capacity() const { return m_Capacity; }

private:
	void reallocData(size_t capacity);

private:
	int64_t* m_Data;
	size_t m_Capacity;
	size_t m_Size;
};