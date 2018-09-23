#include "matrix.h"
#include <iostream>

void BringError(double dif)
{
	std::cout << "ERROR " << dif << std::endl;
	throw std::exception();
}

extern unsigned int SafeRand();

double GetSignedUnitRand() // Between -1 and 1
{
	return (double( SafeRand() % (1 << 16) ) / (1 << 16)) * (SafeRand() % 2 ? 1 : -1);
}

std::vector<double> GetRandVector( size_t N ) // Between -1 and 1
{
	std::vector<double> Ret;
	for ( int i = 0; i < N; ++i )
		Ret.push_back( GetSignedUnitRand() );
	return Ret;
}

double Get(const CMatrix<1, 1>& M)
{
	return M[0][0];
}

CMatrix<1, 1> Mat11( double x ) { return CMatrix<1, 1>( x ); }

long long nMatrices = 0;