#pragma once

#include <cstdint>

struct _DeamortizedLookaheadCOLA_Entry {
	int64_t m_Value;
	size_t m_PrimaryPointer;
	size_t m_SecondaryPointer;
};

struct _DeamortizedLookaheadCOLA_Layer
{
	using Entry = _DeamortizedLookaheadCOLA_Entry;

	Entry* m_Data;

	size_t m_MergePrimaryIndex;
	size_t m_MergeSecondaryIndex;
	size_t m_MergeDstIndex;
};

class DeamortizedLookaheadCOLA
{
private:
	using Entry = _DeamortizedLookaheadCOLA_Entry;
	using Layer = _DeamortizedLookaheadCOLA_Layer;

public:
	DeamortizedLookaheadCOLA() :
		DeamortizedLookaheadCOLA::DeamortizedLookaheadCOLA(15) { }
	
	DeamortizedLookaheadCOLA(size_t initialCapacity);

	DeamortizedLookaheadCOLA(const DeamortizedLookaheadCOLA& other);

	~DeamortizedLookaheadCOLA();

public:
	void add(int64_t value);

	bool contains(int64_t value) const;

	inline size_t size() const;

	inline size_t capacity() const;

private:
	uint8_t m_LayerCount;
	Layer* m_Layers;

	size_t m_PrimaryVisibleFlags;
	size_t m_SecondaryVisibleFlags;
};
