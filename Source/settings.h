#pragma once
#include <string>

#define TESTING

#ifndef TESTING
constexpr int THREADS = 2;
#else

#ifdef _DEBUG
constexpr int THREADS = 1;
#else
constexpr int THREADS = 12;
#endif

//#define SMALL_FIELD
#endif

namespace Settings
{
	constexpr int TIMEBANK = 10000;
	constexpr int TIME_PER_MOVE = 100;
	constexpr int MAX_ROUNDS = 200;
#ifdef SMALL_FIELD
	constexpr int WIDTH = 10;
	constexpr int HEIGHT = 8;
	using COLMASK = uint8_t;

	constexpr int START_LIVES = 18;
#else
	constexpr int WIDTH = 18;
	constexpr int HEIGHT = 16;
	using COLMASK = uint16_t;

	constexpr int START_LIVES = 40;
#endif

	static const std::string DATA_DIR = "../saved_data/";
}
using namespace Settings;