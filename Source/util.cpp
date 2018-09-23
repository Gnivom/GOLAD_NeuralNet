#include "util.h"

#include <random>
#include <mutex>

unsigned int SafeRand()
{
	static std::mutex mtx;
	mtx.lock();
	static std::mt19937 generator( unsigned( time( nullptr ) ) );  // mt19937 is a standard mersenne_twister_engine
	auto nRet = generator();
	mtx.unlock();
	return nRet;
}