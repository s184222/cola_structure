#include <iostream>
#include <cstdlib>
#include <cstdint>

#include "structure/basic_cola.h"
#include "structure/deamortized_cola.h"
#include "structure/lookahead_cola.h"
#include "structure/avx_basic_cola.h"

template<typename T>
static void insert(T& cola, int64_t value)
{
	std::cout << "add(" << value << ")" << std::endl;
	cola.add(value);
}

template<typename T>
static void search(const T& cola, int64_t value)
{
	std::cout << "contains(" << value << "): " << cola.contains(value) << std::endl;
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
	for (const int64_t& value : cola)
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

	testIterator(cola);
	testContains(cola);
}

int main()
{
	//testBasicCola();
	//testDeamortizedCola();
	//testLookaheadCola();
	testAVXBasicCola();

	return 0;
}
