#include "avx_basic_cola.h"

#include <memory>
#include <algorithm>
#include <immintrin.h>

#ifndef PARALLEL_MERGE
#define PARALLEL_MERGE 1
#endif // !PARALLEL_MERGE

#ifndef PARALLEL_SEARCH
// Note: Parallel search is slower than the sequential search in
// almost all cases because of excessive and frequent cache misses.
#define PARALLEL_SEARCH 0
#endif // !PARALLEL_SEARCH

AVXBasicCOLA::AVXBasicCOLA(uint32_t initialCapacity) :
	m_Data(nullptr),
	m_Capacity(0),
	m_Size(0)
{
	// Capacity must be a power of two (and greater than zero)
	m_Capacity = std::max(static_cast<uint32_t>(16), nextPO2MinusOne(initialCapacity - 1) + 1);
	m_Data = new int32_t[m_Capacity];
}

AVXBasicCOLA::AVXBasicCOLA(const AVXBasicCOLA& other) :
	m_Data(new int32_t[other.m_Capacity]),
	m_Capacity(other.m_Capacity),
	m_Size(other.m_Size)
{
	// Copy instead of pointing to the same memory.
	memcpy(m_Data, other.m_Data, other.m_Capacity * sizeof(int32_t));
}

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

#if PARALLEL_SEARCH
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
	_p = _mm256_set_epi32(p     , p >> 1, p >> 2, p >> 3,
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
			}
			while (k != 0);

			if (m_Data[i] == value)
				return true;
		}
	}
#endif

	return false;
}

void AVXBasicCOLA::reallocData(uint32_t capacity)
{
	// Allocate and copy memory to new block
	int32_t* newBlock = new int32_t[capacity];
	const uint32_t c = (capacity > m_Capacity) ? m_Capacity : capacity;
	memcpy(newBlock, m_Data, c * sizeof(int32_t));

	// Delete and set old block
	delete[] m_Data;
	m_Data = newBlock;
	m_Capacity = capacity;
}
