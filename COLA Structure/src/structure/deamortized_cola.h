#pragma once

#include "./math_util.h"

#include <cstdint>
#include <iostream>

class DeamortizedCOLA
{
public:
	struct Layer
	{
		int64_t* m_Data;

		size_t m_MergeLeftIndex;
		size_t m_MergeRightIndex;
		size_t m_MergeDstIndex;
	};

	class ConstIterator
	{

	public:
		using PointerType = const int64_t*;
		using ReferenceType = const int64_t&;
	public:
		ConstIterator(size_t leftFullFlags, size_t rightFullFlags, uint8_t layerCount,
			const Layer* layers, uint8_t layer, size_t index) :

			m_LeftFullFlags(leftFullFlags),
			m_RightFullFlags(rightFullFlags),

			m_LayerCount(layerCount),
			m_Layers(layers),

			m_Layer(layer),
			m_Index(index) { }

		ConstIterator& operator++()
		{
			const size_t prevIndex = m_Index++;
			const size_t mask = static_cast<size_t>(1) << m_Layer;

			// Check if we should go to next layer (end of left array and right empty, or end of right array)
			if (((prevIndex ^ m_Index) & mask) != 0 && (m_Index & m_RightFullFlags & mask) == 0)
			{
				m_Index = 0;

				// Go to next layer that contains elements
				const size_t nonEmptyFlags = m_LeftFullFlags | m_RightFullFlags;
				for (m_Layer++; m_Layer < m_LayerCount; m_Layer++)
				{
					// Check if current layer is non-empty
					if ((nonEmptyFlags >> m_Layer) & 0x1)
					{
						// Check if left array is empty and set index accordingly
						if (((m_LeftFullFlags >> m_Layer) & 0x1) == 0)
							m_Index = static_cast<size_t>(1) << m_Layer;
						break;
					}
				}
			}

			return *this;
		}

		ConstIterator operator++(int)
		{
			ConstIterator itr = *this;
			++(*this);
			return itr;
		}

		ConstIterator& operator--()
		{
			const size_t maskMinusOne = (static_cast<size_t>(1) << m_Layer) - 1;

			// Check if we should go to next layer (beginning of right array and left empty, or beginning of left array)
			if ((m_Index & maskMinusOne) == 0 && (m_Index & m_LeftFullFlags) == 0)
			{
				// Go to previous layer that contains elements
				const size_t nonEmptyFlags = m_LeftFullFlags | m_RightFullFlags;
				while (m_Layer--)
				{
					// Check if current layer is non-empty
					if ((nonEmptyFlags >> m_Layer) & 0x1)
					{
						// Check if right array is empty and set index accordingly
						if (((m_RightFullFlags >> m_Layer) & 0x1) == 0)
							m_Index = static_cast<size_t>(1) << m_Layer;
						else
							m_Index = static_cast<size_t>(2) << m_Layer;
						break;
					}
				}
			}
			
			m_Index--;
			
			return *this;
		}

		ConstIterator operator--(int)
		{
			ConstIterator itr = *this;
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

		bool operator==(const ConstIterator& other) const
		{
			return (other.m_Layers == m_Layers && other.m_Layer == m_Layer && other.m_Index == m_Index);
		}

		bool operator!=(const ConstIterator& other) const
		{
			return !(*this == other);
		}

	protected:
		const size_t m_LeftFullFlags;
		const size_t m_RightFullFlags;

		const uint8_t m_LayerCount;
		const DeamortizedCOLA::Layer* m_Layers;

		uint8_t m_Layer;
		size_t m_Index;
	};

public:
	DeamortizedCOLA() :
		DeamortizedCOLA::DeamortizedCOLA(15) { }
	
	DeamortizedCOLA(size_t initialCapacity);

	DeamortizedCOLA(const DeamortizedCOLA& other);

public:
	void add(int64_t value);

	bool contains(int64_t value) const;

	size_t size() const;

	size_t capacity() const;

	ConstIterator begin() const
	{
		const uint8_t layer = popcount(leastZeroBits(m_LeftFullFlags | m_RightFullFlags));
		const size_t index = ((m_LeftFullFlags >> layer) & 0x1) ? 0 : (static_cast<size_t>(1) << layer);
		return ConstIterator(m_LeftFullFlags, m_RightFullFlags, m_LayerCount, m_Layers, layer, index);
	}

	ConstIterator end() const
	{
		return ConstIterator(m_LeftFullFlags, m_RightFullFlags, m_LayerCount, m_Layers, m_LayerCount, 0);
	}

private:
	void prepareMerge(const uint8_t l);
	void mergeLayers(uint_fast16_t m);
	void reallocLayers(uint8_t layerCount);

private:
	size_t m_LeftFullFlags;
	size_t m_RightFullFlags;
	size_t m_MergeFlags;

	uint8_t m_LayerCount;
	Layer* m_Layers;
};