#include "avx_deamortized_cola.h"

#include <memory>
#include <algorithm>
#include <immintrin.h>

#ifndef AVX_PARALLEL_MERGE
#define AVX_PARALLEL_MERGE 1
#endif // !AVX_PARALLEL_MERGE

#ifndef AVX_MERGE_UNSAFE_CAST
#define AVX_MERGE_UNSAFE_CAST 1
#endif // !AVX_MERGE_UNSAFE_CAST

AVXDeamortizedCOLA::AVXDeamortizedCOLA(uint32_t initialCapacity) :
	m_LeftFullFlags(0),
	m_RightFullFlags(0),

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
		m_Layers[l].m_Data = new int32_t[static_cast<uint32_t>(2) << l];
	}
}

AVXDeamortizedCOLA::AVXDeamortizedCOLA(const AVXDeamortizedCOLA& other) :
	m_LeftFullFlags(other.m_LeftFullFlags),
	m_RightFullFlags(other.m_RightFullFlags),

	m_LayerCount(other.m_LayerCount),
	m_Layers(new Layer[other.m_LayerCount])
{
	// Allocate and copy layers
	for (uint8_t l = 0; l < m_LayerCount; l++)
	{
		Layer& dstLayer = m_Layers[l];
		Layer& srcLayer = other.m_Layers[l];

		// Allocate layer data
		const uint32_t layerSize = static_cast<uint32_t>(2) << l;
		dstLayer.m_Data = new int32_t[layerSize];

		// Copy layer data
		memcpy(dstLayer.m_Data, srcLayer.m_Data, layerSize * sizeof(int32_t));
		dstLayer.m_MergeLeftIndex = srcLayer.m_MergeLeftIndex;
		dstLayer.m_MergeRightIndex = srcLayer.m_MergeRightIndex;
		dstLayer.m_MergeDstIndex = srcLayer.m_MergeDstIndex;
	}
}

AVXDeamortizedCOLA::~AVXDeamortizedCOLA()
{
	for (uint8_t l = 0; l < m_LayerCount; l++)
		delete[] m_Layers[l].m_Data;
	delete[] m_Layers;
}

#if AVX_PARALLEL_MERGE
/* Helpers for insert operation during merges */
static const __m256i _reverse_idx = _mm256_set_epi32(0, 1, 2, 3, 4, 5, 6, 7);
static const __m256i _swap128_idx = _mm256_set_epi32(3, 2, 1, 0, 7, 6, 5, 4);
#if !AVX_MERGE_UNSAFE_CAST
static const __m256i _loadMask = _mm256_set_epi32(-1, -1, -1, -1, -1, -1, -1, -1);
#endif

#define AVX_BITONIC_SORT_COUNT 8

static inline __m256i reverse(__m256i& _value)
{
	return _mm256_permutevar8x32_epi32(_value, _reverse_idx);
}

static inline __m256i swap128(__m256i& _value)
{
	return _mm256_permutevar8x32_epi32(_value, _swap128_idx);
}

static inline __m256i load8x32i(const int32_t* src)
{
#if AVX_MERGE_UNSAFE_CAST
	return *((__m256i*)src);
#else
	return _mm256_maskload_epi32(src, _loadMask);
#endif
}

static inline void store8x32i(int32_t* dst, __m256i src)
{
#if AVX_MERGE_UNSAFE_CAST
	* ((__m256i*)dst) = src;
#else
	_mm256_maskstore_epi32(dst, _loadMask, src);
#endif
}

static inline void minmax(__m256i& _a, __m256i& _b, __m256i& _mn, __m256i& _mx)
{
	_mn = _mm256_min_epi32(_a, _b);
	_mx = _mm256_max_epi32(_a, _b);
}

static inline void bitonicMerge8x8(__m256i& _a, __m256i& _b)
{
	// Perform a standard branchless bitonic merge on the two 8-vectors
	// using a simple merge network.
	// See https://xhad1234.github.io/Parallel-Sort-Merge-Join-in-Peloton/

	// Reverse the second 8-vector to obtain a single bitonic sequence
	_b = reverse(_b);

	__m256i _mna, _mxa, _mnb, _mxb, _atmp, _btmp;

	// Phase 1: Perform the first min-max to obtain two bitonic sequences
	_atmp = _a;
	_a = _mm256_min_epi32(_a, _b);
	_b = _mm256_max_epi32(_atmp, _b);

	// Phase 2
	_atmp = swap128(_a);
	_btmp = swap128(_b);

	minmax(_a, _atmp, _mna, _mxa);
	minmax(_b, _btmp, _mnb, _mxb);

	_a = _mm256_blend_epi32(_mna, _mxa, 0b11110000);
	_b = _mm256_blend_epi32(_mnb, _mxb, 0b11110000);

	// Phase 3
	_atmp = _mm256_shuffle_epi32(_a, 0b01001110);
	_btmp = _mm256_shuffle_epi32(_b, 0b01001110);

	minmax(_a, _atmp, _mna, _mxa);
	minmax(_b, _btmp, _mnb, _mxb);

	_a = _mm256_unpacklo_epi64(_mna, _mxa);
	_b = _mm256_unpacklo_epi64(_mnb, _mxb);

	// Phase 4
	_atmp = _mm256_shuffle_epi32(_a, 0b10110001);
	_btmp = _mm256_shuffle_epi32(_b, 0b10110001);

	minmax(_a, _atmp, _mna, _mxa);
	minmax(_b, _btmp, _mnb, _mxb);

	_a = _mm256_blend_epi32(_mna, _mxa, 0b10101010);
	_b = _mm256_blend_epi32(_mnb, _mxb, 0b10101010);
}
#endif

void AVXDeamortizedCOLA::add(int32_t value)
{
	const uint32_t nSize = size() + 1;
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

void AVXDeamortizedCOLA::prepareMerge(const uint8_t l)
{
	Layer& layer = m_Layers[l];
	layer.m_MergeLeftIndex = 0;
	layer.m_MergeRightIndex = static_cast<uint32_t>(1) << l;
	layer.m_MergeDstIndex = m_LeftFullFlags & (layer.m_MergeRightIndex << 1);
}

void AVXDeamortizedCOLA::mergeLayers(int_fast16_t m)
{
	uint8_t l = 0;

	while (m > 0 && (m_RightFullFlags >> l))
	{
		// Check if we are merging current layer
		if ((m_RightFullFlags >> l) & 0x1)
		{
			// Current and next layer
			Layer& srcLayer = m_Layers[l];
			Layer& dstLayer = m_Layers[l + 1];

			// Retrieve indices for merging
			uint32_t& i = srcLayer.m_MergeLeftIndex;
			uint32_t& j = srcLayer.m_MergeRightIndex;
			uint32_t& k = srcLayer.m_MergeDstIndex;

			// Find last indices for merging
			const uint32_t iEnd = static_cast<uint32_t>(1) << l;
			const uint32_t jEnd = static_cast<uint32_t>(2) << l;

#if AVX_PARALLEL_MERGE
			if (iEnd < AVX_BITONIC_SORT_COUNT)
			{
				// Perform simple merge sort with moves (ascending order)
				while (m > 0 && i != iEnd && j != jEnd)
				{
					if (srcLayer.m_Data[i] <= srcLayer.m_Data[j])
						dstLayer.m_Data[k++] = srcLayer.m_Data[i++];
					else
						dstLayer.m_Data[k++] = srcLayer.m_Data[j++];
					m--;
				}

				// Copy remaining elements in left array
				while (m > 0 && i != iEnd)
				{
					dstLayer.m_Data[k++] = srcLayer.m_Data[i++];
					m--;
				}

				// Copy remaining elements in right array
				while (m > 0 && j != jEnd)
				{
					dstLayer.m_Data[k++] = srcLayer.m_Data[j++];
					m--;
				}
			}
			else
			{
				// At this point the number of items in the levels is a multiple
				// of AVX_BITONIC_SORT_COUNT. Sort using bitonic merge.
				__m256i _a, _b;

				if (!isPO2(i) || !isPO2(j))
				{
					// We have merged the source layers partially. Load the
					// temporarily stored vector from destination array.
					_b = load8x32i(&dstLayer.m_Data[k]);
				}
				else
				{
					// Load 8-vector values from memory
					_a = load8x32i(&srcLayer.m_Data[i]);
					_b = load8x32i(&srcLayer.m_Data[j]);

					i += AVX_BITONIC_SORT_COUNT;
					j += AVX_BITONIC_SORT_COUNT;
				
					bitonicMerge8x8(_a, _b);

					// Store lower elements (smallest elements both layers)
					store8x32i(&dstLayer.m_Data[k], _a);
					k += AVX_BITONIC_SORT_COUNT;
					m -= AVX_BITONIC_SORT_COUNT;
				}

				while (m > 0 && i != iEnd && j != jEnd)
				{
					if (srcLayer.m_Data[i] < srcLayer.m_Data[j])
					{
						_a = load8x32i(&srcLayer.m_Data[i]);
						i += AVX_BITONIC_SORT_COUNT;
					}
					else
					{
						_a = load8x32i(&srcLayer.m_Data[j]);
						j += AVX_BITONIC_SORT_COUNT;
					}

					bitonicMerge8x8(_a, _b);
					store8x32i(&dstLayer.m_Data[k], _a);
					k += AVX_BITONIC_SORT_COUNT;
					m -= AVX_BITONIC_SORT_COUNT;
				}

				// Sort remaining elements from current layer
				while (m > 0 && i != iEnd)
				{
					_a = load8x32i(&srcLayer.m_Data[i]);
					i += AVX_BITONIC_SORT_COUNT;
					bitonicMerge8x8(_a, _b);
					store8x32i(&dstLayer.m_Data[k], _a);
					k += AVX_BITONIC_SORT_COUNT;
					m -= AVX_BITONIC_SORT_COUNT;
				}

				// Sort remaining elements from merging layer
				while (m > 0 && j != jEnd)
				{
					_a = load8x32i(&srcLayer.m_Data[j]);
					j += AVX_BITONIC_SORT_COUNT;
					bitonicMerge8x8(_a, _b);
					store8x32i(&dstLayer.m_Data[k], _a);
					k += AVX_BITONIC_SORT_COUNT;
					m -= AVX_BITONIC_SORT_COUNT;
				}

				if (i == iEnd && j == jEnd)
				{
					// Store the last vector of elements.
					store8x32i(&dstLayer.m_Data[k], _b);
					k += AVX_BITONIC_SORT_COUNT;
					m -= AVX_BITONIC_SORT_COUNT;
				}
				else
				{
					// Store the vector temporarily in the destination layer.
					store8x32i(&dstLayer.m_Data[k], _b);
				}
			}
#else
			// Sequential fallback implementation
			while (m > 0 && i != iEnd && j != jEnd)
			{
				if (srcLayer.m_Data[i] <= srcLayer.m_Data[j])
					dstLayer.m_Data[k++] = srcLayer.m_Data[i++];
				else
					dstLayer.m_Data[k++] = srcLayer.m_Data[j++];
				m--;
			}

			while (m > 0 && i != iEnd)
			{
				dstLayer.m_Data[k++] = srcLayer.m_Data[i++];
				m--;
			}

			while (m > 0 && j != jEnd)
			{
				dstLayer.m_Data[k++] = srcLayer.m_Data[j++];
				m--;
			}
#endif

			// Check if we are done merging
			if (i == iEnd && j == jEnd)
			{
				// Remove full and merge flags
				m_LeftFullFlags &= ~(1 << l);
				m_RightFullFlags &= ~(1 << l);

				// Set full flags of next layer.
				if ((k >> l) == 0x2)
				{
					// We were merging into the left array
					m_LeftFullFlags |= static_cast<uint32_t>(2) << l;
					// Test if the other array is also full
					if ((m_RightFullFlags >> l) & 0x2)
						prepareMerge(l + 1);
				}
				else
				{
					m_RightFullFlags |= static_cast<uint32_t>(2) << l;
					if ((m_RightFullFlags >> l) & 0x2)
						prepareMerge(l + 1);
				}
			}
		}

		l++;
	}
}

bool AVXDeamortizedCOLA::contains(int32_t value) const
{
	for (uint8_t l = 0; l < m_LayerCount; l++)
	{
		// Size of an array in the layer is half the layer size
		const uint32_t arraySize = static_cast<uint32_t>(1) << l;

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

uint32_t AVXDeamortizedCOLA::size() const
{
	return m_LeftFullFlags + m_RightFullFlags;
}

uint32_t AVXDeamortizedCOLA::capacity() const
{
	// The actual capacity of the layers can be calculated as:
	//   Layers    0   1   2   ...   l
	//   Capacity  2 + 4 + 8 + ... + 2^(l + 1) = 2^(l + 2) - 2
	// To allow for merging, the usable capacity should be halved.
	return (static_cast<uint32_t>(1) << m_LayerCount) - 1;
}

void AVXDeamortizedCOLA::reallocLayers(uint8_t layerCount)
{
	// Allocate and copy layers to new block
	Layer* newLayers = new Layer[layerCount];

	// Copy and prepare layers in new block
	for (uint8_t l = 0; l < layerCount; l++)
	{
		if (l < m_LayerCount)
			newLayers[l] = m_Layers[l];
		else
			newLayers[l].m_Data = new int32_t[static_cast<uint32_t>(2) << l];
	}

	// Delete and set old block
	delete[] m_Layers;
	m_Layers = newLayers;
	m_LayerCount = layerCount;
}
