#include <iostream>

#include "structure/cola.h"

void main()
{
	COLA cola;

	for (int i = 0; i < 1000; i++)
	{
		cola.add(i);
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

	for (const int64_t& v : cola)
		std::cout << v << ", ";
	std::cout << std::endl;
}
