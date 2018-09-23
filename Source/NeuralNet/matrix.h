#pragma once

#include <array>
#include <vector>
#include <functional>
#include <type_traits>
#include <iostream>
#include <cmath>
#include <memory>

template<typename TMatrix>
class CMatrixWithFactor
{
public:
	using MatrixType = TMatrix;
	CMatrixWithFactor( const TMatrix& Mat, double vFactor ) : _Mat( Mat ), _vFactor( vFactor ) {}
	const TMatrix& _Mat;
	double _vFactor;
	operator TMatrix() const
	{
		TMatrix Ret = _Mat;
		Ret *= _vFactor;
		return Ret;
	}
};
template<typename TMatrix>
auto GetTranspose( const CMatrixWithFactor<TMatrix>& Mat )
{
	return GetTranspose( TMatrix( Mat ) );
}
template<bool B, typename LHS, typename RHS>
struct SConditionalMatrixMultiplication
{};
template<typename LHS, typename RHS>
struct SConditionalMatrixMultiplication< true, LHS, RHS >
{
	typedef decltype( typename LHS::MatrixType() * typename RHS::MatrixType() ) type;
};
template<class LHS, class RHS>
auto operator*( const LHS& lhs, const RHS& rhs ) ->
typename SConditionalMatrixMultiplication<
	!std::is_same<LHS, typename LHS::MatrixType>::value ||
	!std::is_same<RHS, typename RHS::MatrixType>::value,
	LHS, RHS
>::type
{
	return static_cast<typename LHS::MatrixType>(lhs) * static_cast<typename RHS::MatrixType>(rhs);
}

template<size_t N>
using SIZET = std::integral_constant<size_t, N>;

namespace NMatrix
{
	template<bool B>
	struct SDataType {};
	template<>
	struct SDataType<true>// true = vector
	{
		template<size_t N, size_t M>
		auto operator()( SIZET<N> n, SIZET<M> m ) { return std::vector<std::array<double, M>>( N ); }
	};
	template<>
	struct SDataType<false>// false = array
	{
		template<size_t N, size_t M>
		auto operator()( SIZET<N> n, SIZET<M> m ) { return std::array<std::array<double, M>, N>{}; }
	};
	constexpr bool IsTooLarge( size_t N )
	{
		return N > 1000;
	}
}

template<size_t N, size_t M>
class CMatrix
{
public:
	typedef CMatrix<N, M> MatrixType;
	constexpr static size_t _N = N;
	constexpr static size_t _M = M;
	using DataType = typename NMatrix::SDataType< NMatrix::IsTooLarge( N*M ) >;

	explicit CMatrix( double v = 0.0 ) : _data( DataType()( SIZET<N>(), SIZET<M>() ) ) { fill( v ); }
	CMatrix( const std::array<double, N>& data ) { for ( int n = 0; n<N; ++n ) _data[n][0] = data[n]; }
	CMatrix( const CMatrix& Other ) : _data( Other._data ) {}
	CMatrix( CMatrix&& Other ) : _data( std::move(Other._data) ) {}
	~CMatrix() {}

	CMatrix& operator=( const CMatrix& Other ) { _data = Other._data; return *this; }
	CMatrix& operator=( CMatrix&& Other ) { _data = std::move( Other._data ); return *this; }

	CMatrix<N, M>& operator-=( const CMatrix<N, M>& Other );
	CMatrix<N, M>& operator-=( const CMatrixWithFactor<CMatrix<N, M>>& Other );
	CMatrix<N, M>& operator+=( const CMatrix<N, M>& Other );
	CMatrix<N, M> operator+( const CMatrix<N, M>& Other ) const;
	CMatrix<N, M> operator-( const CMatrix<N, M>& Other ) const;
	template<size_t P>
	CMatrix<N, P> operator*(const CMatrix<M, P>& Other) const;
	std::array<double, N> operator*( const std::array<double, M>& X ) const;
	CMatrixWithFactor<CMatrix<N, M>> operator*( double x) const;
	CMatrix<N, M>& operator*=( double x );

	inline auto& operator[] (size_t i) { return _data[i]; }
	inline const auto& operator[] (size_t i) const { return _data[i]; }

	CMatrix<M, N> GetTranspose() const;
	CMatrix<N, M> MultiplyEachByTranspose( const CMatrix<M, N>& Other ) const;

	void SubtractEachRow(const CMatrix<1, M>& Dif);
	inline void fill(double x) { for (auto& Row : _data) Row.fill(x); }
	void Randomize();

	decltype(DataType()(SIZET<N>(), SIZET<M>())) _data;
};

void BringError( double dif );

template<size_t N, size_t M>
CMatrix<N, M>& CMatrix<N, M>::operator-=(const CMatrix<N, M>& Other)
{
	for ( int n = 0; n < N; ++n ) for ( int m = 0; m < M; ++m )
	{
		_data[n][m] -= Other._data[n][m];
		if ( Other._data[n][m] > 100 || Other._data[n][m] < -100 )
			BringError( Other._data[n][m] );
	}
	return *this;
}
template<size_t N, size_t M>
CMatrix<N, M>& CMatrix<N, M>::operator-=( const CMatrixWithFactor<CMatrix<N, M>>& Other )
{
	for ( int n = 0; n < N; ++n ) for ( int m = 0; m < M; ++m )
	{
		_data[n][m] -= Other._Mat._data[n][m] * Other._vFactor;
		if ( _data[n][m] != _data[n][m] || Other._Mat._data[n][m]*Other._vFactor > 100 || Other._Mat._data[n][m]*Other._vFactor < -100 )
			BringError( Other._Mat._data[n][m]*Other._vFactor );
	}
	return *this;
}
template<size_t N, size_t M>
CMatrix<N, M>& CMatrix<N, M>::operator+=( const CMatrix<N, M>& Other )
{
	for ( int n = 0; n < N; ++n ) for ( int m = 0; m < M; ++m )
	{
		_data[n][m] += Other._data[n][m];
		if ( Other._data[n][m] > 100 || Other._data[n][m] < -100 )
			BringError( Other._data[n][m] );
	}
	return *this;
}

template<size_t N, size_t M>
CMatrix<N,M> CMatrix<N, M>::operator+(const CMatrix<N, M>& Other) const
{
	CMatrix<N, M> Ret;
	for (int n = 0; n < N; ++n) for (int m = 0; m < M; ++m)
		Ret[n][m] += _data[n][m] + Other._data[n][m];
	return Ret;
}
template<size_t N, size_t M>
CMatrix<N, M> CMatrix<N, M>::operator-( const CMatrix<N, M>& Other ) const
{
	CMatrix<N, M> Ret;
	for ( int n = 0; n < N; ++n ) for ( int m = 0; m < M; ++m )
		Ret[n][m] += _data[n][m] - Other._data[n][m];
	return Ret;
}

template<size_t N, size_t M>
template<size_t P>
CMatrix<N, P> CMatrix<N,M>::operator*(const CMatrix<M, P>& Other) const
{
	CMatrix<N, P> Ret;
	if ( P == 1 ) // Optimization
	{
		const auto OtherRow = std::move(Other.GetTranspose()._data[0]);
		for ( int n = 0; n < N; ++n )
		{
			double vRetElement = 0.0;
			const auto& MyRow = _data[n];
			for ( int m = 0; m < M; ++m )
			{
				vRetElement += MyRow[m] * OtherRow[m];
			}
			Ret[n][0] = vRetElement;
		}
	}
	else
	{
		for ( int n = 0; n < N; ++n )
		{
			auto& RetRow = Ret[n];
			const auto& MyRow = _data[n];
			for ( int m = 0; m < M; ++m )
			{
				const auto& MyElement = MyRow[m];
				const auto& OtherRow = Other[m];
				for ( int p = 0; p < P; ++p )
					RetRow[p] += MyElement * OtherRow[p];
			}
		}
	}
	return Ret;
}

template<size_t N, size_t M>
std::array<double, N> CMatrix<N, M>::operator*( const std::array<double,M>& X ) const
{
	std::array<double, N> Ret = {};
	for ( int n = 0; n < N; ++n )
	{
		const auto& MyRow = _data[n];
		double& vRet = Ret[n];
		for ( int m = 0; m < M; ++m )
		{
			vRet += MyRow[m] * X[m];
		}
	}
	return Ret;
}

template<size_t N, size_t M>
CMatrixWithFactor<CMatrix<N, M>> CMatrix<N, M>::operator*(double x) const
{
	return CMatrixWithFactor<CMatrix<N, M>>( *this, x );
}
template<size_t N, size_t M>
CMatrix<N, M>& CMatrix<N, M>::operator*=( double x )
{
	for ( int n = 0; n < N; ++n ) for ( int m = 0; m < M; ++m )
		(*this)[n][m] *= x;
	return *this;
}

template<size_t N, size_t M>
inline CMatrix<M, N> CMatrix<N, M>::GetTranspose() const
{
	CMatrix<M, N> Ret;
	for (int n = 0; n < N; ++n) for (int m = 0; m < M; ++m)
		Ret[m][n] = (*this)[n][m];
	return Ret;
}
template<size_t N>
CMatrix<1, N> GetTranspose( const std::array<double, N>& X )
{
	CMatrix<1, N> Ret;
	Ret[0] = X;
	return Ret;
}
template<size_t N, size_t M>
CMatrix<M, N> GetTranspose( const CMatrix<N, M>& X )
{
	return X.GetTranspose();
}

template<size_t N, size_t M>
CMatrix<N, M> CMatrix<N, M>::MultiplyEachByTranspose( const CMatrix<M, N>& Other ) const
{
	CMatrix<N, M> Ret;
	for ( int n = 0; n < N; ++n ) for ( int m = 0; m < M; ++m )
		Ret[n][m] = (*this)[n][m] * Other[m][n];
	return Ret;
}

double GetSignedUnitRand();
std::vector<double> GetRandVector( size_t N );
template<size_t N, size_t M>
void CMatrix<N, M>::Randomize()
{
	for ( auto& Row : _data ) for ( double& x : Row )
		x = GetSignedUnitRand();
}

template<size_t N, size_t M>
void CMatrix<N, M>::SubtractEachRow(const CMatrix<1, M>& Dif)
{
	for (int n = 0; n < N; ++n) for (int m = 0; m < M; ++m)
		_data[n][m] -= Dif[0][m];
}

double Get(const CMatrix<1, 1>& M);

template<size_t N>
double MeanAbs( const CMatrix<1, N>& M )
{
	double vSum = 0.0;
	for ( int col = 0; col < N; ++col )
		vSum += abs( M[0][col] );
	return vSum / N;
}

template<size_t N>
std::array<double, N> Transform( const std::array<double, N>& Mat, std::function<double( double )> f )
{
	std::array<double, N> Ret;
	for ( int n = 0; n < N; ++n )
		Ret[n] = f( Mat[n] );
	return Ret;
}
template<size_t N, size_t M>
CMatrix<N, M> Transform(const CMatrix<N, M>& Mat, std::function<double(double)> f)
{
	CMatrix<N, M> Ret;
	for (int n = 0; n < N; ++n) for (int m = 0; m < M; ++m)
		Ret[n][m] = f(Mat[n][m]);
	return Ret;
}
template<size_t N, size_t M>
CMatrix<M, N> TransformTranspose(const CMatrix<N, M>& Mat, std::function<double(double)> f)
{
	CMatrix<M, N> Ret;
	for (int n = 0; n < N; ++n) for (int m = 0; m < M; ++m)
		Ret[m][n] = f(Mat[n][m]);
	return Ret;
}

template<size_t N>
class CDiagonalMatrix
{
public:
	CDiagonalMatrix() {}
	CDiagonalMatrix( double v ) { _Diagonal.fill( v ); }
	CMatrix<N, N> CreateMatrix() const
	{
		CMatrix<N, N> Ret( 0.0 );
		for ( int n = 0; n < N; ++n )
			Ret[n][n] = _Diagonal[n];
		return Ret;
	}
	std::array<double,N> _Diagonal;
};

template<size_t N>
CDiagonalMatrix<N> DiagonalMatrixFromCol( const CMatrix<N, 1>& Col )
{
	CDiagonalMatrix<N> Ret;
	for ( int n = 0; n < N; ++n )
		Ret._Diagonal[n] = Col[n][0];
	return Ret;
}
template<size_t N>
CDiagonalMatrix<N> DiagonalMatrixFromRow( const CMatrix<1, N>& Row )
{
	CDiagonalMatrix<N> Ret;
	for ( int n = 0; n < N; ++n )
		Ret._Diagonal[n] = Row[0][n];
	return Ret;
}

template<size_t N, size_t M>
CMatrix<N, M> ColToNRows( const CMatrix<M, 1>& Col, SIZET<N> Dummy )
{
	CMatrix<N, M> Ret;
	for ( int n = 0; n < N; ++n ) for ( int m = 0; m < M; ++m )
		Ret[n][m] = Col[m][0];
	return Ret;
}

CMatrix<1, 1> Mat11( double x );

template<size_t N, size_t M>
CMatrix<N, M>& operator*=( CMatrix<N, M>& Mat, const CDiagonalMatrix<M>& D )
{
	for ( int n = 0; n < N; ++n )
	{
		for ( int m = 0; m < M; ++m )
		{
			Mat[n][m] *= D._Diagonal[m];
		}
	}
	return Mat;
}
template<size_t N, size_t M>
CMatrix<N, M> operator*( const CMatrix<N, M>& Mat, const CDiagonalMatrix<M>& D )
{
	auto Ret = Mat;
	Ret *= D;
	return Ret;
}

template<size_t N>
std::array<double, N> operator+( const std::array<double, N>& lhs, const CMatrix<N, 1>& rhs )
{
	std::array<double, N> Ret = lhs;
	for ( int n = 0; n < N; ++n )
		Ret[n] += rhs[n][0];
	return Ret;
}

struct SExplicitIndex
{
	explicit SExplicitIndex( size_t i ) : _i(i) {}
	operator size_t() const { return _i; }
	size_t _i;
};

template<size_t N, size_t M>
class CSparseMatrix
{
private:
	std::vector<double> _SharedValues;
public:
	typedef CSparseMatrix<N, M> MatrixType;
	constexpr static size_t _N = N;
	constexpr static size_t _M = M;
	struct SEntry
	{
		SEntry( size_t n, size_t m, double v, bool bDynamic ) : _n( n ), _m( m ), _v( v ), _bDynamic( bDynamic ) {}
		SEntry( size_t n, size_t m, SExplicitIndex nSharedValue, bool bDynamic ) : _n( n ), _m( m ), _nSharedValue( nSharedValue ), _bDynamic( bDynamic ) {}
		size_t _n; size_t _m; bool _bDynamic;
		inline double& value(CSparseMatrix& parent) { return _nSharedValue == -1 ? _v : parent._SharedValues[_nSharedValue]; }
		inline const double& value(const CSparseMatrix& parent) const { return _nSharedValue == -1 ? _v : parent._SharedValues[_nSharedValue]; }
	private:
		double _v = 0.0;
		size_t _nSharedValue = -1;
	};
	std::vector< SEntry > _entries;
	CSparseMatrix() {}
	explicit CSparseMatrix( std::vector<double> SharedValues ) : _SharedValues( std::move( SharedValues ) ) {}
	explicit CSparseMatrix( const decltype(_entries)& entries ) : _entries( entries ) {}

	void fill( double v ) { for ( SEntry& entry : _entries ) entry.value(*this) = v; }
	void Randomize() { for ( SEntry& entry : _entries ) if ( entry._bDynamic ) entry.value(*this) = GetSignedUnitRand(); }

	std::array<double, N> operator*( const std::array<double, M>& X ) const;
	template<size_t P>
	CMatrix<N, P> operator*( const CMatrix<M, P>& Other ) const;
	CSparseMatrix& operator-=( const CMatrix<N, M>& Other );
	CSparseMatrix& operator-=( const CSparseMatrix<N, M>& Other );
	CSparseMatrix& operator-=( const CMatrixWithFactor<CMatrix<N, M>>& Other );
	CSparseMatrix& operator-=( const CMatrixWithFactor<CSparseMatrix<N, M>>& Other );
	CSparseMatrix& operator+=( const CSparseMatrix<N, M>& Other );
	CSparseMatrix& operator*=( double v );
	CMatrixWithFactor<CSparseMatrix> operator*( double v ) const;
};
template<size_t N, size_t M>
CSparseMatrix<N, M> MakeSparse( const CMatrix<N, M>& Mat, bool bDynamic )
{
	CSparseMatrix<N, M> Ret;
	for ( int n = 0; n < N; ++n ) for ( int m = 0; m < M; ++m )
		Ret._entries.emplace_back( n, m, Mat[n][m], bDynamic );
	return Ret;
}

template<size_t N, size_t M>
std::array<double, N> CSparseMatrix<N,M>::operator*( const std::array<double, M>& X ) const
{
	std::array<double, N> Ret = {};
	for ( const auto& entry : _entries )
		Ret[entry._n] += X[entry._m] * entry.value(*this);
	return Ret;
}
template<size_t N, size_t M>
template<size_t P>
CMatrix<N, P> CSparseMatrix<N, M>::operator*( const CMatrix<M, P>& Other ) const
{
	CMatrix<N, P> Ret( 0.0 );
	for ( const auto& entry : _entries )
	{
		auto& RetRow = Ret[entry._n];
		auto& OtherRow = Other[entry._m];
		for ( int p = 0; p < P; ++p )
			RetRow[p] += OtherRow[p] * entry.value();
	}
	return Ret;
}
template<size_t N, size_t M>
CSparseMatrix<N,M>& CSparseMatrix<N, M>::operator-=( const CMatrix<N, M>& Other )
{
	for ( auto& entry : _entries )
		if ( entry._bDynamic )
			entry.value() -= Other[entry._n][entry._m];
	return *this;
}
template<size_t N, size_t M>
CSparseMatrix<N, M>& CSparseMatrix<N, M>::operator-=( const CMatrixWithFactor<CMatrix<N, M>>& Other )
{
	for ( auto& entry : _entries )
		if ( entry._bDynamic )
			entry.value() -= Other._Mat[entry._n][entry._m] * Other._vFactor;
	return *this;
}
template<size_t N, size_t M>
CSparseMatrix<N, M>& CSparseMatrix<N, M>::operator+=( const CSparseMatrix<N, M>& Other )
{
	if ( _entries.size() != Other._entries.size() )
		std::cerr << "FATAL ERROR: _entries mismatch in CSparseMatrix" << std::endl;
	for ( int i = 0; i < _entries.size(); ++i )
		if ( _entries[i]._bDynamic )
			_entries[i].value(*this) += Other._entries[i].value(Other);
	return *this;
}
template<size_t N, size_t M>
CSparseMatrix<N, M>& CSparseMatrix<N, M>::operator-=( const CSparseMatrix<N, M>& Other )
{
	if ( _entries.size() != Other._entries.size() )
		std::cerr << "FATAL ERROR: _entries mismatch in CSparseMatrix" << std::endl;
	for ( int i = 0; i < _entries.size(); ++i )
		if ( _entries[i]._bDynamic )
			_entries[i].value() -= Other._entries[i].value();
	return *this;
}
template<size_t N, size_t M>
CSparseMatrix<N, M>& CSparseMatrix<N, M>::operator-=( const CMatrixWithFactor<CSparseMatrix<N, M>>& Other )
{
	if ( _entries.size() != Other._Mat._entries.size() )
		std::cerr << "FATAL ERROR: _entries mismatch in CSparseMatrix" << std::endl;
	for ( int i = 0; i < _entries.size(); ++i )
		if ( _entries[i]._bDynamic )
			_entries[i].value(*this) -= Other._Mat._entries[i].value(Other._Mat) * Other._vFactor;
	return *this;
}
template<size_t N, size_t M, size_t P>
CMatrix<N, P> operator*( const CMatrix<N, M>& lhs, const CSparseMatrix<M, P>& rhs )
{
	CMatrix<N, P> Ret(0.0);
	for ( int n = 0; n < N; ++n )
	{
		auto& RetRow = Ret[n];
		const auto& lhsRow = lhs[n];
		for ( const auto& entry : rhs._entries )
		{
			const auto m = entry._n;
			const auto p = entry._m;
			RetRow[p] += lhsRow[m] * entry.value(rhs);
		}
	}
	return Ret;
}
template<size_t N, size_t M>
CSparseMatrix<N,M>& CSparseMatrix<N, M>::operator*=( double v )
{
	for ( SEntry& entry : _entries )
		if ( entry._bDynamic )
			entry.value(*this) *= v;
	return *this;
}
template<size_t N, size_t M>
CMatrixWithFactor<CSparseMatrix<N, M>> CSparseMatrix<N, M>::operator*( double v ) const
{
	return CMatrixWithFactor<CSparseMatrix<N, M>>( *this, v );
}

template<typename MatrixT>
struct SMatrixMultiplier{};
template<size_t N, size_t P>
struct SMatrixMultiplier<CMatrix<N, P>>
{
	SMatrixMultiplier( const CMatrix<N,P>& Filter ) {}
	template<size_t M>
	CMatrix<N, P> operator()( const CMatrix<N, M>& lhs, const CMatrix<M, P>& rhs ) const { return lhs * rhs; }
};
template<size_t N, size_t P>
struct SMatrixMultiplier<CSparseMatrix<N, P>>
{
	const CSparseMatrix<N, P>& _Filter;
	SMatrixMultiplier( const CSparseMatrix<N,P>& Filter ) : _Filter( Filter ) {}
	template<size_t M>
	CSparseMatrix<N, P> operator()( const CMatrix<N, M>& lhs, const CMatrix<M, P>& rhs ) const
	{
		CSparseMatrix<N, P> Ret;
		Ret._entries.reserve( _Filter._entries.size() );
		for ( const auto& FilterEntry : _Filter._entries )
		{
			const size_t n = FilterEntry._n;
			const size_t p = FilterEntry._m;
			double vSum = 0.0;
			for ( size_t m = 0; m < M; ++m )
				vSum += lhs[n][m] * rhs[m][p];
			Ret._entries.emplace_back( n, p, vSum, true );
		}
		return Ret;
	}
};

template<size_t N>
std::array<double, N> operator-( const std::array<double, N> lhs, const std::array<double, N> rhs )
{
	std::array<double, N> Ret;
	for ( int i = 0; i < N; ++i )
		Ret[i] = lhs[i] - rhs[i];
	return Ret;
}
template<size_t N>
std::array<double, N>& operator*=( std::array<double, N>& lhs, double rhs )
{
	for ( double& x : lhs )
		x *= rhs;
	return lhs;
}
template<size_t N>
std::array<double, N>& operator*( const std::array<double, N>& lhs, double rhs )
{
	auto Ret = lhs;
	Ret *= rhs;
	return Ret;
}