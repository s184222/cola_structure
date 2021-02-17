#include <iostream>

#include "structure/cola.h"

void main()
{
	COLA cola;

	for (int i = 0; i < 100; i++)
	{
		cola.add(5);
		cola.add(2);
		cola.add(3);
		cola.add(7);
		cola.add(9);
	}

	std::cout << cola.size() << std::endl;
}
