#include "deamortized_cola.h"

#include <memory>
#include <algorithm>

DeamortizedCOLA::DeamortizedCOLA(size_t initialCapacity) :
	m_LeftFullFlags(0),
	m_RightFullFlags(0),
	m_MergeFlags(0),

	m_LayerCount(0),
	m_Layers(nullptr)
{
	// Layers should be able to contain twice the capacity to allow for merging.
	m_LayerCount = std::max(4ui8, popcount(nextPO2MinusOne(initialCapacity)));
	m_Layers = new Layer[m_LayerCount];
	
	// Allocate layer data
	for (uint8_t l = 0; l < m_LayerCount; l++)
	{
		// Size of each array on layer l is 2^l. Hence, with two
		// arrays on each layer the size of layer l is 2^(l + 1).
		m_Layers[l].m_Data = new int64_t[static_cast<size_t>(2) << l];
	}
}

DeamortizedCOLA::DeamortizedCOLA(const DeamortizedCOLA& other) :
	m_LeftFullFlags(other.m_LeftFullFlags),
	m_RightFullFlags(other.m_RightFullFlags),
	m_MergeFlags(other.m_MergeFlags),

	m_LayerCount(other.m_LayerCount),
	m_Layers(new Layer[other.m_LayerCount])
{
	// Allocate and copy layers
	for (uint8_t l = 0; l < m_LayerCount; l++)
	{
		Layer& dstLayer = m_Layers[l];
		Layer& srcLayer = other.m_Layers[l];

		// Allocate layer data
		const size_t layerSize = static_cast<size_t>(2) << l;
		dstLayer.m_Data = new int64_t[layerSize];
		
		// Copy layer data
		memcpy(dstLayer.m_Data, srcLayer.m_Data, layerSize * sizeof(int64_t));
		dstLayer.m_MergeLeftIndex = srcLayer.m_MergeLeftIndex;
		dstLayer.m_MergeRightIndex = srcLayer.m_MergeRightIndex;
		dstLayer.m_MergeDstIndex = srcLayer.m_MergeDstIndex;
	}
}

void DeamortizedCOLA::add(int64_t value)
{
	const size_t nSize = size() + 1;
	if (nSize > capacity())
	{
		// Double the usable capacity
		reallocLayers(m_LayerCount + 1);
	}

	// Insert value into empty array in first layer
	if (m_LeftFullFlags & 0x1)
	{
		// Insert value in right array
		m_Layers[0].m_Data[1] = value;
		m_RightFullFlags |= 0x1;

		// Prepare merging into layer 1
		prepareMerge(0);
	}
	else
	{
		// Insert value in left array
		m_Layers[0].m_Data[0] = value;
		m_LeftFullFlags |= 0x1;
	}

	// Merge layers with m = 2 * k + 2 moves
	mergeLayers((m_LayerCount << 1) + 2);
}

void DeamortizedCOLA::prepareMerge(const uint8_t l)
{
	const size_t flag = static_cast<size_t>(1) << l;
	m_MergeFlags |= flag;

	Layer& layer = m_Layers[l];
	layer.m_MergeLeftIndex = 0;
	layer.m_MergeRightIndex = flag;
	layer.m_MergeDstIndex = m_LeftFullFlags & (flag << 1);
}

void DeamortizedCOLA::mergeLayers(uint_fast16_t m)
{
	uint8_t l = 0;

	while (m && (m_MergeFlags >> l))
	{
		// Check if we are merging current layer
		if ((m_MergeFlags >> l) & 0x1)
		{
			// Current and next layer
			Layer& srcLayer = m_Layers[l];
			Layer& dstLayer = m_Layers[l + 1];
			
			// Retrieve indices for merging
			size_t& i = srcLayer.m_MergeLeftIndex;
			size_t& j = srcLayer.m_MergeRightIndex;
			size_t& k = srcLayer.m_MergeDstIndex;

			// Find last indices for merging
			const size_t iEnd = static_cast<size_t>(1) << l;
			const size_t jEnd = static_cast<size_t>(2) << l;

			// Perform simple merge sort with moves (ascending order)
			while (m && i != iEnd && j != jEnd)
			{
				if (srcLayer.m_Data[i] <= srcLayer.m_Data[j])
					dstLayer.m_Data[k++] = srcLayer.m_Data[i++];
				else
					dstLayer.m_Data[k++] = srcLayer.m_Data[j++];
				m--;
			}

			// Copy remaining elements in left array
			while (m && i != iEnd)
			{
				dstLayer.m_Data[k++] = srcLayer.m_Data[i++];
				m--;
			}

			// Copy remaining elements in right array
			while (m && j != jEnd)
			{
				dstLayer.m_Data[k++] = srcLayer.m_Data[j++];
				m--;
			}

			// Check if we are done merging
			if (i == iEnd && j == jEnd)
			{
				// Remove full and merge flags
				m_LeftFullFlags &= ~(1 << l);
				m_RightFullFlags &= ~(1 << l);
				m_MergeFlags &= ~(1 << l);

				// Set full flags of next layer.
				if ((k >> l) == 0x2)
				{
					// We were merging into the left array
					m_LeftFullFlags |= static_cast<size_t>(2) << l;
					// Test if the other array is also full
					if ((m_RightFullFlags >> l) & 0x2)
						prepareMerge(l + 1);
				}
				else
				{
					m_RightFullFlags |= static_cast<size_t>(2) << l;
					if ((m_RightFullFlags >> l) & 0x2)
						prepareMerge(l + 1);
				}
			}
		}

		l++;
	}
}

bool DeamortizedCOLA::contains(int64_t value) const
{
	for (uint8_t l = 0; l < m_LayerCount; l++)
	{
		// Size of an array in the layer is half the layer size
		const size_t arraySize = static_cast<size_t>(1) << l;
		
		// Check if the left array has elements
		if (((m_LeftFullFlags >> l) & 0x1))
		{
			// Perform simple binary search
			if (binarySearch(value, m_Layers[l].m_Data, 0, arraySize))
				return true;
		}

		// Check if the left array has elements
		if ((m_RightFullFlags >> l) & 0x1)
		{
			// Perform simple binary search
			if (binarySearch(value, m_Layers[l].m_Data, arraySize, arraySize << 1))
				return true;
		}
	}

	return false;
}

size_t DeamortizedCOLA::size() const
{
	return m_LeftFullFlags + m_RightFullFlags;
}

size_t DeamortizedCOLA::capacity() const
{
	// The actual capacity of the layers can be calculated as:
	//   Layers    0   1   2   ...   l
	//   Capacity  2 + 4 + 8 + ... + 2^(l + 1) = 2^(l + 2) - 2
	// To allow for merging, the usable capacity should be halved.
	return (static_cast<size_t>(1) << m_LayerCount) - 1;
}

void DeamortizedCOLA::reallocLayers(uint8_t layerCount)
{
	// Allocate and copy layers to new block
	Layer* newLayers = new Layer[layerCount];

	// Copy and prepare layers in new block
	for (uint8_t l = 0; l < layerCount; l++)
	{
		if (l < m_LayerCount)
			newLayers[l] = m_Layers[l];
		else
			newLayers[l].m_Data = new int64_t[static_cast<size_t>(2) << l];
	}

	// Delete and set old block
	delete[] m_Layers;
	m_Layers = newLayers;
	m_LayerCount = layerCount;
}
