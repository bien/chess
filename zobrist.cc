#include "genarray.hh"
#include <stdlib.h>
#include <iostream>

uint64_t shuffle(int index)
{
	uint64_t seed = 0x1234567890abcdef;
	seed |= index;
	seed |= seed << 6;
	seed |= seed >> 11;
	return seed;
}
/*
template<size_t index>
struct ZobristRandom {
	static const uint64_t value = shuffle(index);
};
	

template<int size, typename width>
width[size] invert(width[size] value)
{
}

*/
int main()
{
	typedef generate_array<6*2*64 + 1 + 4 + 8, ZobristRandom>::result zobrist_hashes;
	
	std::cout << std::hex;
	for (int i = 0; i < sizeof(zobrist_hashes::data) / sizeof(uint64_t); i++)
	{
		std::cout << zobrist_hashes::data[i] << std::endl;
	}
	
	return 0;
}