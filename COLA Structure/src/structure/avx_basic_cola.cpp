#include "avx_basic_cola.h"

#include <memory>
#include <algorithm>
#include <immintrin.h>

#ifndef BASIC_PARALLEL_MERGE
#define BASIC_PARALLEL_MERGE 1
#endif // !BASIC_PARALLEL_MERGE

#ifndef BASIC_MERGE_UNSAFE_CAST
#define BASIC_MERGE_UNSAFE_CAST 1
#endif // !BASIC_MERGE_UNSAFE_CAST

#ifndef BASIC_PARALLEL_SEARCH
// Note: Parallel search is slower than the sequential search in
// almost all cases because of excessive and frequent cache misses.
#define BASIC_PARALLEL_SEARCH 1
#endif // !BASIC_PARALLEL_SEARCH

AVXBasicCOLA::AVXBasicCOLA(uint32_t initialCapacity) :
	m_DataUnaligned(nullptr),
	m_Data(nullptr),
	m_Capacity(0),
	m_Size(0)
{
	// Capacity must be a power of two (and greater than zero)
	m_Capacity = std::max(static_cast<uint32_t>(16), nextPO2MinusOne(initialCapacity - 1) + 1);
	allocateData(m_DataUnaligned, m_Data, m_Capacity);
}

AVXBasicCOLA::AVXBasicCOLA(const AVXBasicCOLA& other) :
	m_DataUnaligned(nullptr),
	m_Data(nullptr),
	m_Capacity(other.m_Capacity),
	m_Size(other.m_Size)
{
	allocateData(m_DataUnaligned, m_Data, m_Capacity);
	// Copy instead of pointing to the same memory.
	memcpy(m_Data, other.m_Data, other.m_Capacity * sizeof(int32_t));
}

AVXBasicCOLA::~AVXBasicCOLA()
{
	delete[] m_DataUnaligned;
}

#if BASIC_PARALLEL_MERGE
/* Helpers for insert operation during merges */
static const __m256i _reverse_idx = _mm256_set_epi32(0, 1, 2, 3, 4, 5, 6, 7);
static const __m256i _swap128_idx = _mm256_set_epi32(3, 2, 1, 0, 7, 6, 5, 4);
#if !BASIC_MERGE_UNSAFE_CAST
static const __m256i _loadMask = _mm256_set_epi32(-1, -1, -1, -1, -1, -1, -1, -1);
#endif

#define BASIC_BITONIC_SORT_COUNT 8

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
#if BASIC_MERGE_UNSAFE_CAST
	return *((__m256i*)src);
#else
	return _mm256_maskload_epi32(src, _loadMask);
#endif
}

static inline void store8x32i(int32_t* dst, __m256i src)
{
#if BASIC_MERGE_UNSAFE_CAST
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

void AVXBasicCOLA::add(int32_t value)
{
	const uint32_t nSize = m_Size + 1;
	if (nSize >= m_Capacity)
	{
		// Allocate a new layer
		reallocData(m_Capacity << 1);
	}

	// Find first position of empty array (merge-layer)
	const uint32_t m = leastZeroBits(nSize) + 1;
	const uint32_t mEnd = m << 1;

	m_Data[mEnd - 1] = value;

	// Iteratively merge arrays
	uint32_t i = 1;

#if BASIC_PARALLEL_MERGE
	// Merge the first eight elements (three layers) sequentially
	while (i < BASIC_BITONIC_SORT_COUNT && i != m)
	{
		// Index after last element in current layer
		const uint32_t iEnd = i << 1;

		// Index of first element in merging layer and new index
		uint32_t j = mEnd - i;
		uint32_t k = mEnd - iEnd;

		// Simple merge sort (ascending order)
		while (i != iEnd && j != mEnd)
		{
			if (m_Data[i] <= m_Data[j])
				m_Data[k++] = m_Data[i++];
			else
				m_Data[k++] = m_Data[j++];
		}

		// Copy remaining elements in current layer
		while (i != iEnd)
			m_Data[k++] = m_Data[i++];
	}

	// Sort remaining elements using AVX
	if (i != m)
	{
		// At this point we know that we can sort the next eight
		// elements. This can be done with a single bitonic sort.
		uint32_t j = mEnd - BASIC_BITONIC_SORT_COUNT;
		uint32_t k = mEnd - 2 * BASIC_BITONIC_SORT_COUNT;

		__m256i _a, _b;

		// Load 8-vector values from memory
		_a = load8x32i(&m_Data[i]);
		_b = load8x32i(&m_Data[j]);

		i += BASIC_BITONIC_SORT_COUNT;
		//j += BASIC_BITONIC_SORT_COUNT;

		// Perform bitonic merge on the two vectors
		bitonicMerge8x8(_a, _b);

		// Store lower half of sorted vectors
		store8x32i(&m_Data[k], _a);
		k += BASIC_BITONIC_SORT_COUNT;
		// Store higher half of sorted vectors
		store8x32i(&m_Data[k], _b);
		//k += BASIC_BITONIC_SORT_COUNT;

		// Sort remaining elements (layers have size 16 * 2^(l - 4))
		while (i != m)
		{
			// Index after last element in current layer
			const uint32_t iEnd = i << 1;

			// Index of first element in merging layer and new index
			j = mEnd - i;
			k = mEnd - iEnd;

			_a = load8x32i(&m_Data[i]);
			_b = load8x32i(&m_Data[j]);

			i += BASIC_BITONIC_SORT_COUNT;
			j += BASIC_BITONIC_SORT_COUNT;

			do
			{
				bitonicMerge8x8(_a, _b);

				// Store lower elements (smallest elements both layers)
				store8x32i(&m_Data[k], _a);
				k += BASIC_BITONIC_SORT_COUNT;

				if (m_Data[i] < m_Data[j])
				{
					_a = load8x32i(&m_Data[i]);
					i += BASIC_BITONIC_SORT_COUNT;
				}
				else
				{
					_a = load8x32i(&m_Data[j]);
					j += BASIC_BITONIC_SORT_COUNT;
				}
			} while (i != iEnd && j != mEnd);

			// At least one pair is remaining to be merged
			bitonicMerge8x8(_a, _b);
			store8x32i(&m_Data[k], _a);
			k += BASIC_BITONIC_SORT_COUNT;

			// Sort remaining elements from current layer
			while (i != iEnd)
			{
				_a = load8x32i(&m_Data[i]);
				i += BASIC_BITONIC_SORT_COUNT;
				bitonicMerge8x8(_a, _b);
				store8x32i(&m_Data[k], _a);
				k += BASIC_BITONIC_SORT_COUNT;
			}

			// Sort remaining elements from merging layer
			while (j != mEnd)
			{
				_a = load8x32i(&m_Data[j]);
				j += BASIC_BITONIC_SORT_COUNT;
				bitonicMerge8x8(_a, _b);
				store8x32i(&m_Data[k], _a);
				k += BASIC_BITONIC_SORT_COUNT;
			}

			// Store the remaining pair of higher elements.
			store8x32i(&m_Data[k], _b);
			//k += BASIC_BITONIC_SORT_COUNT;
		}
	}
#else
	// Sequential fallback implementation
	while (i != m)
	{
		// Index after last element in current layer
		const uint32_t iEnd = i << 1;

		// Index of first element in merging layer and new index
		uint32_t j = mEnd - i;
		uint32_t k = mEnd - iEnd;

		// Simple merge sort (ascending order)
		while (i != iEnd && j != mEnd)
		{
			if (m_Data[i] <= m_Data[j])
				m_Data[k++] = m_Data[i++];
			else
				m_Data[k++] = m_Data[j++];
		}

		// Copy remaining elements in current layer
		while (i != iEnd)
			m_Data[k++] = m_Data[i++];
	}
#endif

	m_Size = nSize;
}

bool AVXBasicCOLA::contains(int32_t value) const
{
	// Binary search in each of the layers is based on Alg 3, LEADBIT,
	// from the article: A Fast and Vectorizable Alternative to Binary
	// Search in O(1) with Wide Applicability to Arrays of Floating
	// Point Numbers, by Fabio Cannizzo:
	//
	// function LEADBIT(input: z, P, output: i) ; P = N > 1 ? 2^floor(log2(N - 1)) : 0
	// 	   i <- 0
	// 	   k <- P
	// 	   repeat
	// 	   	   r <- i|k                         ; bitwise OR
	// 	   	   if z >= X_r then
	// 	   	   	   i <- r                       ; conditional assignment
	// 	   	   end if
	// 	   	   k <- k/2                         ; bitwise right shift
	// 	   until k = 0
	// end function
	//
	// See: https://arxiv.org/pdf/1506.08620.pdf
	// 
	// Since the size of the remaining layers are always a power of two,
	// we have that P = N and we do not need to include padding elements.

	// Compute P = N ? 2^floor(log2(N - 1)) : 0 of the last layer.
	uint32_t p = (nextPO2MinusOne(m_Size) >> 1) + 1;

#if BASIC_PARALLEL_SEARCH
	// Nearby layers will be searched in parallel, such that when searching
	// layer l, we also search l + 1, ..., l + 7 in parallel. This results
	// in at most seven redundant iterations for layer l.
	__m256i _i, _k, _p, _r, _z, _x, _mask1, _mask2, _zero, _size;

	// Prepare constants used for checks
	_zero = _mm256_set1_epi32(0u);
	_size = _mm256_set1_epi32(m_Size);
	// Prepare a vector with search value
	_z = _mm256_set1_epi32(value);
	// Prepare end of the first layers to be searched
	_p = _mm256_set_epi32(p, p >> 1, p >> 2, p >> 3,
		p >> 4, p >> 5, p >> 6, p >> 7);

	// Comments for 4 element vectors (we now have 8).
	for (; p != 0; p >>= 8)
	{
		// Compute non-zero values for layers that are non-empty in the current block
		// mask1 = p & size
		//       = |0...1000...000|0...0100...000|0...0010...000|0...0001...000| AND
		//         |0...1110...010|0...1110...010|0...1110...010|0...1110...010|
		//       = |0...1000...000|0...0100...000|0...0010...000|0...0000...000|
		_mask1 = _mm256_and_si256(_p, _size);

		// Compute masks for layers that are empty in the current block.
		// mask1 = if (mask1 == 0)
		//       = |if (mask[0] == 0)|if (mask[1] == 0)|if (mask[2] == 0)|if (mask[3] == 0)|
		//       = |0000000...0000000|0000000...0000000|0000000...0000000|1111111...1111111|
		//       = |00...00|00...00|00...00|11...11| // Shortened for better comments
		_mask1 = _mm256_cmpeq_epi32(_mask1, _zero);

		// Check if we should search layers (i.e. if not all layers are empty).
		// i.e. if either of the masks are non-one, search the layers.
		//     |00...00|00...00|00...00|11...11| != |11...11| =>
		//     |   0   |   0   |   0   |   1   | != 0b1111
		if (_mm256_movemask_ps(_mm256_castsi256_ps(_mask1)) != 0b11111111)
		{
			// Prepare index of first element in each of the four layers.
			// i = p
			_i = _p;
			// Prepare relative offset of middle element in each of the four layers.
			// Since there are i elements in the array, we have k = i / 2.
			// k = i >> 1
			//   = |0...10000...0|0...01000...0|0...00100...0|0...00010...0| >> 1
			//   = |0...01000...0|0...00100...0|0...00010...0|0...00001...0|
			_k = _mm256_srli_epi32(_i, 1);

			// Optimization for memory access. There is no need to access elements in
			// the layers that do not need to be searched. Load m_Data[0] for these to
			// lower the amount of cache-misses we can have.
			// k = k if (m_Size & p) else 0 = k AND NOT mask1
			//   = |0...10000...0|0...01000...0|0...00100...0|0...00010...0| AND NOT
			//     |0...00000...0|0...00000...0|0...00000...0|1...11111...1|
			//   = |0...00000...0|0...00000...0|0...00000...0|0...00010...0|
			_k = _mm256_andnot_si256(_mask1, _k);
			// Same for start index, i.
			_i = _mm256_andnot_si256(_mask1, _i);

		repeat:
			// Compute index of middle element in each of the ranges for each of the
			// four layers. E.g. for the first iteration, we have:
			// r = i | k
			//   = |0...10000...0|0...01000...0|0...00100...0|0...00010...0| OR
			//     |0...01000...0|0...00100...0|0...00010...0|0...00001...0|
			//   = |0...11000...0|0...01100...0|0...00110...0|0...00011...0|
			_r = _mm256_or_si256(_i, _k);

			// Load the elements in each of the layers from their respective index.
			// x = m_Data[r]
			//   = |m_Data[0...11000...0]|m_Data[0...01100...0]|m_Data[0...00110...0]|m_Data[0...00011...0]|
			_x = _mm256_i32gather_epi32(m_Data, _r, sizeof(int32_t));

			// Compute comparison mask for x > z for each of the four layers.
			// mask2 = if (x > z)
			//       = |if (x[0] > z[0])|if (x[1] > z[1])|if (x[2] > z[2])|if (x[3] > z[3])|
			//       = |00...00|11...11|00...00|11...11|
			_mask2 = _mm256_cmpgt_epi32(_x, _z);

			// The next two instructions are for calculating the new index for the
			// search. This is done by a single OR and AND NOT operation, as follows:
			//     i = i OR (r AND NOT (x > z)) = i OR (r AND (z >= x)).

			// Compute new part of the index depending on the mask calculated above.
			// Note that the operands are swapped to respect the instruction order.
			// r = r AND NOT (x > z) = r AND (z >= x)
			//   = |0...11000...0|0...01100...0|0...00110...0|0...00011...0| AND NOT
			//     |00000...00000|11111...11111|00000...00000|11111...11111|
			//   = |00000...00000|0...01100...0|00000...00000|0...00011...0|
			_r = _mm256_andnot_si256(_mask2, _r);

			// Compute the new index as explained above (i.e. i = r if (z >= x) else i).
			// i = i OR r
			//   = |0...10000...0|0...01000...0|0...00100...0|0...00010...0| OR
			//     |00000...00000|0...01100...0|00000...00000|0...00011...0|
			//   = |0...10000...0|0...01100...0|0...00100...0|0...00011...0|
			_i = _mm256_or_si256(_i, _r);

			// Halve the range as seen in the pseudocode
			// k = k / 2 = k >> 1
			//   = |0...10000...0|0...01000...0|0...00100...0|0...00010...0| >> 1
			//   = |0...01000...0|0...00100...0|0...00010...0|0...00001...0|
			_k = _mm256_srli_epi32(_k, 1);

			// Compute whether we are done searching and have found i. This is done by
			// checking if k != 0, in which case we have not found i. Note that we do
			// not have a not equal operation, so use the inverse (k = 0).
			// mask2 = if (k = 0)
			//       = |if (k[0] = 0)|if (k[1] = 0)|if (k[2] = 0)|if (k[3] = 0)|
			//       = |00...00|00...00|11...11|11...11|
			_mask2 = _mm256_cmpeq_epi32(_k, _zero);

			// Check if we should continue iterating by checking all the masks.
			// if (k != 0) goto repeat
			// i.e. if either of the masks are zero, go to repeat.
			if (_mm256_movemask_ps(_mm256_castsi256_ps(_mask2)) != 0b11111111)
				goto repeat;

			// Load the resulting elements in each of the layers.
			// x = m_Data[i]
			_x = _mm256_i32gather_epi32(m_Data, _i, sizeof(int32_t));

			// Searching has finished. Check if found elements are equal.
			// if (z == x)
			_mask2 = _mm256_cmpeq_epi32(_z, _x);

			// Mask result with empty masks, to ensure we do not get false positives.
			// mask2 = (NOT mask1) AND mask2
			_mask2 = _mm256_andnot_si256(_mask1, _mask2);

			// If either of the masks are all ones, we have found the element.
			// if (z == x && (size & p) != 0) return true
			if (_mm256_movemask_ps(_mm256_castsi256_ps(_mask2)) != 0b00000000)
				return true;
		}

		// Pointer to the last element in the next four layers.
		// p = p >> 4
		//   = |0...10000000...0|0...01000000...0|0...00100000...0|0...00010000...0| >> 4
		//   = |0...00001000...0|0...00000100...0|0...00000010...0|0...00000001...0|
		_p = _mm256_srli_epi32(_p, 8);
	}
#else
	// Sequential fallback implementation
	for (; p != 0; p >>= 1)
	{
		uint32_t i = p;
		uint32_t k = i >> 1;

		if (m_Size & i)
		{
			do
			{
				uint32_t r = i | k;
				if (value >= m_Data[r])
					i = r;
				k >>= 1;
			} while (k != 0);

			if (m_Data[i] == value)
				return true;
		}
	}
#endif

	return false;
}

void AVXBasicCOLA::allocateData(int32_t*& unalignedPtr, int32_t*& alignedPtr, uint32_t capacity) const
{
#if BASIC_PARALLEL_SEARCH && BASIC_MERGE_UNSAFE_CAST
	// Data must be aligned to 32 bytes (256 bits) when using parallel search. This
	// will then align elements used for bitonic merges at 32 bytes (256 bits).
	unalignedPtr = new int32_t[static_cast<size_t>(capacity) + 31];
	// Ensure that we have an alignment with lower bits as zero.
	alignedPtr = (int32_t*)(((uintptr_t)unalignedPtr + 31) & ~(uintptr_t)0x1F);
#else
	unalignedPtr = alignedPtr = new int32_t[capacity];
#endif
}

void AVXBasicCOLA::reallocData(uint32_t capacity)
{
	// Allocate and copy memory to new block
	int32_t* newBlockUnaligned;
	int32_t* newBlock;
	allocateData(newBlockUnaligned, newBlock, capacity);

	// Copy memory from old aligned ptr to new aligned ptr.
	const uint32_t c = (capacity > m_Capacity) ? m_Capacity : capacity;
	memcpy(newBlock, m_Data, c * sizeof(int32_t));

	// Delete old unaligned data and set new block
	delete[] m_DataUnaligned;
	m_DataUnaligned = newBlockUnaligned;
	m_Data = newBlock;
	m_Capacity = capacity;
}
