#pragma once

#include "matrix.h"
#include "util.h"

#include <cmath>
#include <ratio>

struct SLinearActivation
{
	template<typename T>
	const T& f( const T& Mat ) const { return Mat; }
	template<typename T>
	double df( const T& Mat ) const { return 1.0; }
};

template<typename Implementation>
struct SSimpleActivation
{
	template<typename TArray>
	TArray f( const TArray& Mat ) const { return Transform( Mat, static_cast<const Implementation*>(this)->_f ); }
	template<size_t N>
	CDiagonalMatrix<N> df( const CMatrix<N, 1>& Mat ) const
	{
		CDiagonalMatrix<N> Ret;
		for ( int row = 0; row < N; ++row )
			Ret._Diagonal[row] = static_cast<const Implementation*>(this)->_df( Mat[row][0] );
		return Ret;
	}
	template<size_t N>
	CDiagonalMatrix<N> df( const std::array<double, N>& Mat ) const
	{
		CDiagonalMatrix<N> Ret;
		for ( int row = 0; row < N; ++row )
			Ret._Diagonal[row] = static_cast<const Implementation*>(this)->_df( Mat[row] );
		return Ret;
	}
};

struct STanHActivation : public SSimpleActivation<STanHActivation>
{
	std::function<double(double)> _f = [](double x) { return std::tanh(x); };
	std::function<double(double)> _df = [](double x) { return 1.0 / std::pow(std::cosh(x), 2); };
	std::function<double( double )> _inverse = []( double x ) { return 0.5*std::log((1.0+x)/(1.0-x)); };
};
struct SAbsActivation : public SSimpleActivation<SAbsActivation>
{
	std::function<double( double )> _f = []( double x ) { return x >= 0.0 ? x : -x; };
	std::function<double( double )> _df = []( double x ) { return x >= 0.0 ? 1.0 : -1.0; };
};
struct SReluActivation : public SSimpleActivation<SReluActivation>
{
	std::function<double(double)> _f = [](double x) { return std::max(x, 0.0); };
	std::function<double(double)> _df = [](double x) { return x >= 0.0 ? 1.0 : 0.0; };
};
struct SSquashActivation : public SSimpleActivation<SSquashActivation>
{
	std::function<double( double )> _f = []( double x ) { return x*x/(1.0+x*x); };
	std::function<double( double )> _df = []( double x ) { return 2.0*x/square(x*x+1.0); };
};
struct SHeartBeatActivation : public SSimpleActivation<SHeartBeatActivation>
{
	std::function<double( double )> _f = []( double x ) { return 3.0*x/square( x*x+1.0 ); };
	std::function<double( double )> _df = []( double x ) { return 3.0-9.0*x*x/cube( x*x+1.0 ); };
};

template<size_t MILLI_THRESHOLD_UNSIGNED>
struct SSymLeakReluActivation : public SSimpleActivation<SSymLeakReluActivation<MILLI_THRESHOLD_UNSIGNED>>
{
	constexpr static double THRESHOLD = double( MILLI_THRESHOLD_UNSIGNED ) * 0.001;
	constexpr static double vAlpha = 0.05;
	std::function<double( double )> _f = []( double x ) { return x > THRESHOLD ? vAlpha * THRESHOLD + (x-THRESHOLD): x < -THRESHOLD ? vAlpha * -THRESHOLD + (x+THRESHOLD): vAlpha * x; };
	std::function<double( double )> _df = []( double x ) { return x > THRESHOLD ? 1.0 : x < -THRESHOLD ? 1.0 : vAlpha; };
};

struct SSoftMaxActivation
{
	template<size_t N>
	std::array<double, N> f( const std::array<double, N>& Mat, int nOverrideSize = -1 ) const
	{
		std::array<double, N> Ret;
		double vSum = 0.0;
		for ( int i = 0; i < N; ++i )
		{
			if ( i == nOverrideSize )
				break;
			vSum += (Ret[i] = std::exp( Mat[i] ));
		}
		Ret *= 1.0 / vSum;
		for ( int i = nOverrideSize; i < N; ++i )
			Ret[i] = Mat[i];
		return Ret;
	}
	template<typename TPropagationData>
	TPropagationData f( const TPropagationData& Data ) const { return TPropagationData( f(Data._data, Data._SizePerDepth*Data._Depth ) ); }
	template<typename T>
	double df( const T& Mat ) const
	{
		return 1.0;
	}
};