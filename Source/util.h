#pragma once

#include "settings.h"

#include <vector>
#include <array>
#include <thread>
#include <set>
#include <future>
#include <chrono>

template<typename T>
constexpr T square( T x )
{
	return x*x;
}
template<typename T>
constexpr T cube( T x )
{
	return x*x*x;
}

using FieldSquare = std::pair<unsigned char, unsigned char>;
using TMovePart = std::pair<FieldSquare, bool>;
using TMoveIdentifier = std::vector<TMovePart>;

template<typename Functor, typename... Args>
auto RunThreaded( const Functor& func, const Args&... args )
{
	std::array< decltype(func( args... )), THREADS > Ret;
	auto RunFunc = [&Ret, &func, &args...]( int i )
	{
		Ret[i] = func( args... );
	};
	std::vector<std::thread> Threads;
	for ( int i = 0; i < THREADS; ++i )
		Threads.emplace_back( RunFunc, i );
	for ( int i = 0; i < THREADS; ++i )
		Threads[i].join();
	return Ret;
}

unsigned int SafeRand();

template<typename T>
void KnuthShuffle( T& V )
{
	const size_t N = V.size();
	if ( N == 0 ) return;
	for ( size_t i = 0; i < N-1; ++i )
	{
		const size_t j = (SafeRand() % (N-i-1)) + i;
		std::swap( V[i], V[j] );
	}
}

template<typename TData, typename NumericType = double>
class CCandidateList
{
public:
	CCandidateList( int nMaxCandidates ) : _nMaxCandidates( nMaxCandidates ) {}
	double GetLeastScore() const { return _vLeastScore; }
	void Propose( NumericType vScore, const TData& Entry )
	{
		if ( vScore > _vLeastScore )
		{
			if ( _data.size() >= _nMaxCandidates )
				_data.erase( _data.begin() );
			_data.emplace( vScore, Entry );
			if ( _data.size() >= _nMaxCandidates )
				_vLeastScore = _data.begin()->first;
		}
	};
	const auto& Get() const { return _data; }
	auto& Access() { return _data; }
private:
	std::set<std::pair<NumericType, TData>> _data;
	NumericType _vLeastScore = std::numeric_limits<NumericType>::lowest();
	int _nMaxCandidates;
};

template<size_t N>
constexpr uint32_t SuperFastHash( const std::array<uint16_t, N>& data1, const std::array<uint16_t, N>& data2 )
{
	uint32_t hash = N << 2;
	for ( size_t i = 0; i < N; ++i )
	{
		hash += data1[i];
		uint32_t tmp = (uint32_t(data2[i]) << 11) ^ hash;
		hash = (hash << 16) ^ tmp;
		hash += hash >> 11;
	}
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;
	return hash;
}