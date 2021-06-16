#pragma once

#include "./math_util.h"

#include <cstdint>
#include <iostream>

struct _AVXDeamortizedCOLA_Layer
{
	int32_t* m_Data;

	uint32_t m_MergeLeftIndex;
	uint32_t m_MergeRightIndex;
	uint32_t m_MergeDstIndex;
};

class _AVXDeamortizedCOLA_ConstIterator
{
private:
	using Layer = _AVXDeamortizedCOLA_Layer;
public:
	using PointerType = const int32_t*;
	using ReferenceType = const int32_t&;

public:
	_AVXDeamortizedCOLA_ConstIterator(uint32_t leftFullFlags, uint32_t rightFullFlags, uint8_t layerCount,
		const Layer* layers, uint8_t layer, uint32_t index) :

		m_LeftFullFlags(leftFullFlags),
		m_RightFullFlags(rightFullFlags),

		m_LayerCount(layerCount),
		m_Layers(layers),

		m_Layer(layer),
		m_Index(index) { }

	_AVXDeamortizedCOLA_ConstIterator& operator++()
	{
		const uint32_t prevIndex = m_Index++;
		const uint32_t mask = static_cast<uint32_t>(1) << m_Layer;

		// Check if we should go to next layer (end of left array and right empty, or end of right array)
		if (((prevIndex ^ m_Index) & mask) != 0 && (m_Index & m_RightFullFlags & mask) == 0)
		{
			m_Index = 0;

			// Go to next layer that contains elements
			const uint32_t nonEmptyFlags = m_LeftFullFlags | m_RightFullFlags;
			for (m_Layer++; m_Layer < m_LayerCount; m_Layer++)
			{
				// Check if current layer is non-empty
				if ((nonEmptyFlags >> m_Layer) & 0x1)
				{
					// Check if left array is empty and set index accordingly
					if (((m_LeftFullFlags >> m_Layer) & 0x1) == 0)
						m_Index = static_cast<uint32_t>(1) << m_Layer;
					break;
				}
			}
		}

		return *this;
	}

	_AVXDeamortizedCOLA_ConstIterator operator++(int)
	{
		_AVXDeamortizedCOLA_ConstIterator itr = *this;
		++(*this);
		return itr;
	}

	_AVXDeamortizedCOLA_ConstIterator& operator--()
	{
		const uint32_t maskMinusOne = (static_cast<uint32_t>(1) << m_Layer) - 1;

		// Check if we should go to next layer (beginning of right array and left empty, or beginning of left array)
		if ((m_Index & maskMinusOne) == 0 && (m_Index & m_LeftFullFlags) == 0)
		{
			// Go to previous layer that contains elements
			const uint32_t nonEmptyFlags = m_LeftFullFlags | m_RightFullFlags;
			while (m_Layer--)
			{
				// Check if current layer is non-empty
				if ((nonEmptyFlags >> m_Layer) & 0x1)
				{
					// Check if right array is empty and set index accordingly
					if (((m_RightFullFlags >> m_Layer) & 0x1) == 0)
						m_Index = static_cast<uint32_t>(1) << m_Layer;
					else
						m_Index = static_cast<uint32_t>(2) << m_Layer;
					break;
				}
			}
		}

		m_Index--;

		return *this;
	}

	_AVXDeamortizedCOLA_ConstIterator operator--(int)
	{
		_AVXDeamortizedCOLA_ConstIterator itr = *this;
		--(*this);
		return itr;
	}

	PointerType operator->() const
	{
		return &m_Layers[m_Layer].m_Data[m_Index];
	}

	ReferenceType operator*() const
	{
		return m_Layers[m_Layer].m_Data[m_Index];
	}

	bool operator==(const _AVXDeamortizedCOLA_ConstIterator& other) const
	{
		return (other.m_Layers == m_Layers && other.m_Layer == m_Layer && other.m_Index == m_Index);
	}

	bool operator!=(const _AVXDeamortizedCOLA_ConstIterator& other) const
	{
		return !(*this == other);
	}

protected:
	const uint32_t m_LeftFullFlags;
	const uint32_t m_RightFullFlags;

	const uint8_t m_LayerCount;
	const Layer* m_Layers;

	uint8_t m_Layer;
	uint32_t m_Index;
};

class AVXDeamortizedCOLA
{
private:
	using Layer = _AVXDeamortizedCOLA_Layer;
public:
	using ConstIterator = _AVXDeamortizedCOLA_ConstIterator;

public:
	AVXDeamortizedCOLA() :
		AVXDeamortizedCOLA::AVXDeamortizedCOLA(15) { }

	AVXDeamortizedCOLA(uint32_t initialCapacity);

	AVXDeamortizedCOLA(const AVXDeamortizedCOLA& other);

	~AVXDeamortizedCOLA();

public:
	void add(int32_t value);

	bool contains(int32_t value) const;

	uint32_t size() const;

	uint32_t capacity() const;

	ConstIterator begin() const
	{
		const uint8_t layer = popcount(leastZeroBits(m_LeftFullFlags | m_RightFullFlags));
		const uint32_t index = ((m_LeftFullFlags >> layer) & 0x1) ? 0 : (static_cast<uint32_t>(1) << layer);
		return ConstIterator(m_LeftFullFlags, m_RightFullFlags, m_LayerCount, m_Layers, layer, index);
	}

	ConstIterator end() const
	{
		return ConstIterator(m_LeftFullFlags, m_RightFullFlags, m_LayerCount, m_Layers, m_LayerCount, 0);
	}

private:
	void prepareMerge(const uint8_t l);
	void mergeLayers(int_fast16_t m);
	void reallocLayers(uint8_t layerCount);

private:
	uint32_t m_LeftFullFlags;
	uint32_t m_RightFullFlags;

	uint8_t m_LayerCount;
	Layer* m_Layers;
};
