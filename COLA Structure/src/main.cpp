#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <random>

#include "structure/basic_cola.h"
#include "structure/deamortized_cola.h"
#include "structure/lookahead_cola.h"
#include "structure/avx_basic_cola.h"
#include "structure/avx_deamortized_cola.h"

template<typename T>
static void insert(T& cola, int64_t value)
{
	std::cout << "add(" << value << ")" << std::endl;
	cola.add(value);
}

static void insert(AVXBasicCOLA& cola, int32_t value)
{
	std::cout << "add(" << value << ")" << std::endl;
	cola.add(value);
}

static void insert(AVXDeamortizedCOLA& cola, int32_t value)
{
	std::cout << "add(" << value << ")" << std::endl;
	cola.add(value);
}

template<typename T>
static void search(const T& cola, int64_t value)
{
	std::cout << "contains(" << value << "): " << cola.contains(value) << std::endl;
}

static void search(const AVXBasicCOLA& cola, int32_t value)
{
	std::cout << "contains(" << value << "): " << cola.contains(value) << std::endl;
}

static void search(const AVXDeamortizedCOLA& cola, int32_t value)
{
	std::cout << "contains(" << value << "): " << cola.contains(value) << std::endl;
}

template<typename T, typename V>
static std::chrono::nanoseconds timedSearch(const T& cola, V value, bool expected)
{
	const auto start = std::chrono::high_resolution_clock::now();
	bool result = cola.contains(value);
	const auto end = std::chrono::high_resolution_clock::now();

	if (result != expected)
		std::cout << "Error!!" << std::endl;

	return end - start;
}

template<typename T>
static void testIterator(const T& cola)
{
	size_t count = 0;
	for (auto itr = cola.begin(); itr != cola.end(); itr++)
	{
		auto prevItr = itr++;
		if (prevItr != --itr)
		{
			std::cout << "Iterator decrement error!" << std::endl;
			return;
		}
		count++;
	}

	if (count != cola.size())
		std::cout << "Iterator count/size mismatch!" << std::endl;
}

template<typename T>
static void testContains(const T& cola)
{
	for (const auto& value : cola)
	{
		if (!cola.contains(value))
		{
			std::cout << "Iterator contains error!" << std::endl;
			return;
		}
	}
}

static void printBasicCOLA(const BasicCOLA& cola)
{
	std::cout << "Layer Full/Empty (1/0): [values]" << std::endl;

	size_t count = 1;
	for (auto itr = cola.begin(); itr != cola.end(); )
	{
		if (cola.size() & count)
		{
			std::cout << "1: [";
			for (size_t i = count; i-- && itr != cola.end(); )
			{
				std::cout << *(itr++);
				if (i != 0)
					std::cout << ", ";
			}
		}
		else
		{
			std::cout << "0: [";
		}

		std::cout << "]" << std::endl;
		count <<= 1;
	}
}

/*
void printDeamortizedCOLA(const DeamortizedCOLA& cola)
{
	for (uint8_t l = 0; l < cola.m_LayerCount; l++)
	{
		os << "Layer [" << +l << "]: " << std::endl;

		size_t s = static_cast<size_t>(1) << l;

		os << "    [";
		for (size_t i = 0; i < s; i++)
		{
			if (i != 0)
				os << ", ";
			os << cola.m_Layers[l].m_Data[i];
		}
		os << "]" << std::endl;
		os << "    [";
		for (size_t i = 0; i < s; i++)
		{
			if (i != 0)
				os << ", ";
			os << cola.m_Layers[l].m_Data[s + i];
		}
		os << "]" << std::endl;
	}

	return os;
}
*/

static void testBasicCola()
{
	BasicCOLA cola;

	for (size_t i = 0; i < 30; i++)
	{
		cola.add(rand());

		printBasicCOLA(cola);
		std::cout << std::endl;
	}

	search(cola, 100);
	search(cola, 200);
	search(cola, 400);
	search(cola, 1000);
	search(cola, 21);

	search(cola, 1337);
	insert(cola, 1337);
	search(cola, 1337);

	testIterator(cola);
	testContains(cola);
}

static void testDeamortizedCola()
{
	DeamortizedCOLA cola;

	insert(cola, 2);
	insert(cola, 1);
	insert(cola, 6);
	insert(cola, 4);
	insert(cola, 3);

	search(cola, 1);
	search(cola, 5);
	search(cola, 10);
	insert(cola, 10);
	search(cola, 10);

	std::cout << "Add elements 10 to 999" << std::endl;
	for (int i = 10; i < 1000; i++)
		cola.add(i);

	search(cola, 100);
	search(cola, 999);
	search(cola, 1);

	testIterator(cola);
	testContains(cola);
}

static void testLookaheadCola()
{
	LookaheadCOLA cola;

	insert(cola, 1);
	insert(cola, 2);
	insert(cola, 6);
	insert(cola, 4);
	insert(cola, 3);

	search(cola, 1);
	search(cola, 5);
	search(cola, 10);
	insert(cola, 10);
	search(cola, 10);

	std::cout << "Add elements 10 to 999" << std::endl;
	for (int i = 10; i < 1000; i++)
	{
		cola.add(i);
		testIterator(cola);
		testContains(cola);
	}

	search(cola, 100);
	search(cola, 999);
	search(cola, 1);

	testIterator(cola);
	testContains(cola);
}

static void testAVXBasicCola()
{
	AVXBasicCOLA cola;

	insert(cola, 1);
	insert(cola, 2);
	insert(cola, 6);
	insert(cola, 4);
	insert(cola, 3);

	search(cola, 1);
	search(cola, 5);
	search(cola, 10);
	insert(cola, 10);
	search(cola, 10);

	search(cola, 1337);
	insert(cola, 1337);
	search(cola, 1337);

	std::cout << "Add elements 10 to 999" << std::endl;
	for (int i = 10; i < 1000; i++)
	{
		cola.add(i);
		testIterator(cola);
		testContains(cola);
	}

	search(cola, 100);
	search(cola, 999);
	search(cola, 1);

	uint32_t s = nextPO2MinusOne(1000000000) + 1;
	std::cout << "Add elements to a size of " << s << std::endl;
	auto start = std::chrono::high_resolution_clock::now();
	while (cola.size() < s)
	{
		cola.add(2 * ((cola.size() & 0x1) ? cola.size() : -(int32_t)cola.size()));
	}
	auto end = std::chrono::high_resolution_clock::now();

	std::cout << "Time to insert: " << (end - start).count() << std::endl;

	uint64_t n = 0;
	std::chrono::nanoseconds time(0);

	for (int i = 0; i < 1; i++)
	{
		time += timedSearch(cola, 0, false); n++;
		time += timedSearch(cola, 2 * s + 1, false); n++;
		// Does not have uneven elements
		time += timedSearch(cola, 1000001, false); n++;
		time += timedSearch(cola, 100003, false); n++;
	}
	// Six total searches
	time /= n;

	std::cout << "Average search time: " << time.count() << std::endl;

	testIterator(cola);
	testContains(cola);
}

static void testAVXDeamortizedCola()
{
	AVXDeamortizedCOLA cola;

	insert(cola, 1);
	insert(cola, 2);
	insert(cola, 6);
	insert(cola, 4);
	insert(cola, 3);

	search(cola, 1);
	search(cola, 5);
	search(cola, 10);
	insert(cola, 10);
	search(cola, 10);

	search(cola, 1337);
	insert(cola, 1337);
	search(cola, 1337);

	std::cout << "Add elements 10 to 999" << std::endl;
	for (int i = 10; i < 1000; i++)
	{
		cola.add(i);
		testIterator(cola);
		testContains(cola);
	}

	search(cola, 100);
	search(cola, 999);
	search(cola, 1);

	uint32_t s = nextPO2MinusOne(10000) + 1;
	std::cout << "Add elements to a size of " << s << std::endl;
	auto start = std::chrono::high_resolution_clock::now();
	while (cola.size() < s)
	{
		cola.add(2 * ((cola.size() & 0x1) ? cola.size() : -(int32_t)cola.size()));
	}
	auto end = std::chrono::high_resolution_clock::now();

	std::cout << "Time to insert: " << (end - start).count() << std::endl;

	uint64_t n = 0;
	std::chrono::nanoseconds time(0);

	for (int i = 0; i < 1; i++)
	{
		time += timedSearch(cola, 0, false); n++;
		time += timedSearch(cola, 2 * s + 1, false); n++;
		// Does not have uneven elements
		time += timedSearch(cola, 1000001, false); n++;
		time += timedSearch(cola, 100003, false); n++;
	}
	// Six total searches
	time /= n;

	std::cout << "Average search time: " << time.count() << std::endl;

	testIterator(cola);
	testContains(cola);
}

template<typename T, uint32_t MAX_LAYERS>
void timeInsertSorted()
{
	T cola;

	auto start = std::chrono::high_resolution_clock::now();
	std::chrono::nanoseconds times[MAX_LAYERS];

	for (uint32_t l = 0; l < MAX_LAYERS; l++)
	{
		uint32_t s = nextPO2MinusOne(cola.size()) + 1;
		while (cola.size() < s)
			cola.add(cola.size());

		auto end = std::chrono::high_resolution_clock::now();
		times[l] = end - start;
	}

	std::cout << "log2 N, avg. insert time" << std::endl;
	for (uint32_t l = 0; l < MAX_LAYERS; l++)
		std::cout << times[l].count() << std::endl;

	std::cout << "Size: " << cola.size() << std::endl;

	// Search for something at the end to ensure that the compiler
	// does not unwantingly optimise code away.
	std::cout << cola.contains(16) << std::endl;
}

template<typename T, uint32_t MAX_LAYERS>
void timeInsertRandom()
{
	std::default_random_engine eng(812938729);
	std::uniform_int_distribution<uint32_t> dist;

	T cola;

	auto start = std::chrono::high_resolution_clock::now();
	std::chrono::nanoseconds times[MAX_LAYERS];

	for (uint32_t l = 0; l < MAX_LAYERS; l++)
	{
		uint32_t s = nextPO2MinusOne(cola.size()) + 1;
		while (cola.size() < s)
			cola.add(static_cast<int32_t>(dist(eng)));

		auto end = std::chrono::high_resolution_clock::now();
		times[l] = end - start;
	}

	std::cout << "log2 N, avg. insert time" << std::endl;
	for (uint32_t l = 0; l < MAX_LAYERS; l++)
		std::cout << times[l].count() << std::endl;

	std::cout << "Size: " << cola.size() << std::endl;

	// Search for something at the end to ensure that the compiler
	// does not unwantingly optimise code away.
	std::cout << cola.contains(16) << std::endl;
}

template<typename T, uint32_t MAX_LAYERS>
void timeSearchRandom()
{
	std::default_random_engine eng(812938729);
	std::uniform_int_distribution<uint32_t> dist;

	T cola;

	std::chrono::nanoseconds times[MAX_LAYERS];

	size_t cntr = 0;
	for (uint32_t l = 1; l < MAX_LAYERS; l++)
	{
		uint32_t s = (1 << l) - 1;
		while (cola.size() < s)
			cola.add(static_cast<int32_t>(dist(eng)));
		
		auto start = std::chrono::high_resolution_clock::now();
		for (uint32_t i = 0; i < 10000; i++) {
			if (cola.contains(static_cast<int32_t>(dist(eng))))
				cntr++;
		}
		auto end = std::chrono::high_resolution_clock::now();
		times[l] = end - start;
	}

	std::cout << "log2(N + 1), avg. search time" << std::endl;
	for (uint32_t l = 1; l < MAX_LAYERS; l++)
		std::cout << times[l].count() << std::endl;

	std::cout << "Size: " << cola.size() << std::endl;

	// Print cntr at the end to ensure that the compiler
	// does not unwantingly optimise code away.
	std::cout << cntr << std::endl;
}

template<typename T, uint32_t MAX_LAYERS>
void timeSearchContained()
{
	T cola;

	std::chrono::nanoseconds times[MAX_LAYERS];

	size_t cntr = 0;
	for (uint32_t l = 1; l < MAX_LAYERS; l++)
	{
		uint32_t s = (1 << l) - 1;
		while (cola.size() < s)
			cola.add(cola.size());

		auto start = std::chrono::high_resolution_clock::now();
		for (uint32_t i = 0; i < 100000; i++) {
			if (cola.contains(static_cast<uint64_t>(i) * cola.size() / 100000))
				cntr++;
		}
		auto end = std::chrono::high_resolution_clock::now();
		times[l] = end - start;
	}

	std::cout << "log2(N + 1), avg. search time" << std::endl;
	for (uint32_t l = 1; l < MAX_LAYERS; l++)
		std::cout << times[l].count() << std::endl;

	std::cout << "Size: " << cola.size() << std::endl;

	// Print cntr at the end to ensure that the compiler
	// does not unwantingly optimise code away.
	std::cout << cntr << std::endl;
}

int main()
{
	//testBasicCola();
	//testDeamortizedCola();
	//testLookaheadCola();
	//testAVXBasicCola();
	//testAVXDeamortizedCola();

	system("PAUSE");
	timeInsertSorted<AVXDeamortizedCOLA, 28>();

	return 0;
}
