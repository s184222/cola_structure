#include <iostream>
#include <cstdlib>
#include <cstdint>

#include "structure/basic_cola.h"

void printCOLA(const BasicCOLA& cola)
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

void main()
{
	BasicCOLA cola;

	for (size_t i = 0; i < 30; i++)
	{
		cola.add(rand());
		
		printCOLA(cola);
		std::cout << std::endl;
	}

#define TEST(v) std::cout << "contains(" << v << "): " << cola.contains(v) << std::endl

	TEST(100);
	TEST(200);
	TEST(400);
	TEST(1000);
	TEST(21);

	TEST(1337);
	cola.add(1337);
	TEST(1337);

}
