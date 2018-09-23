#pragma once

#include "matrix.h"

#include <array>
#include <memory>

template<typename OUTPUT_TYPE, typename INPUT_TYPE>
class CConvolutionMatrix : public CSparseMatrix<OUTPUT_TYPE::_TotalSize, INPUT_TYPE::_TotalSize>
{
public:
	typedef CSparseMatrix<OUTPUT_TYPE::_TotalSize, INPUT_TYPE::_TotalSize> MatrixType;
	CConvolutionMatrix() {}
	CConvolutionMatrix( std::vector<double> SharedValues )
		: MatrixType( std::move( SharedValues ) )
	{}
	OUTPUT_TYPE operator*( const INPUT_TYPE& Input ) const
	{
		return OUTPUT_TYPE( static_cast<const MatrixType&>(*this) * Input._data );
	}
};

template<size_t DEPTH, size_t SIZE_PER_DEPTH, size_t EXTRA_SINGLES>
class CPropagationData
{
public:
	constexpr static size_t _Depth = DEPTH;
	constexpr static size_t _SizePerDepth = SIZE_PER_DEPTH;
	constexpr static size_t _ExtraSingles = EXTRA_SINGLES;
	constexpr static size_t _TotalSize = DEPTH * SIZE_PER_DEPTH + EXTRA_SINGLES;
public:
	std::array<double, _TotalSize> _data;
public:
	CPropagationData() { _data.fill( 0.0 ); }
	CPropagationData( const decltype(_data)& data ) : _data( data ) {}
	operator const std::array<double, _TotalSize>&() const { return _data; }
	double& AccessData( size_t depth, size_t position ) { return _data[depth*_SizePerDepth+position]; }
	double& AccessExtra( size_t i ) { return _data[_Depth*_SizePerDepth+i]; }
	double GetData( size_t depth, size_t position ) const { return _data[depth*_SizePerDepth+position]; }
	double GetExtra( size_t i ) const { return _data[_Depth*_SizePerDepth+i]; }

	template<size_t OUT_DEPTH>
	static CConvolutionMatrix<
		CPropagationData<OUT_DEPTH, SIZE_PER_DEPTH, EXTRA_SINGLES>,
		CPropagationData<DEPTH, SIZE_PER_DEPTH, EXTRA_SINGLES>
	> CreateConvLayer( SIZET<OUT_DEPTH> Dummy1, int nRadius );
};

template<typename TPropagationData>
TPropagationData operator+( const TPropagationData& lhs, const CMatrix<TPropagationData::_TotalSize, 1>& rhs )
{
	return TPropagationData( lhs._data + rhs );
}
template<size_t DEPTH, size_t SIZE_PER_DEPTH, size_t EXTRA_SINGLES>
CPropagationData<DEPTH, SIZE_PER_DEPTH, EXTRA_SINGLES> Transform( const CPropagationData<DEPTH, SIZE_PER_DEPTH, EXTRA_SINGLES>& X, std::function<double( double )> f )
{
	CPropagationData<DEPTH, SIZE_PER_DEPTH, EXTRA_SINGLES> Ret;
	for ( int i = 0; i < DEPTH*SIZE_PER_DEPTH; ++i )
		Ret._data[i] = f( X._data[i] );
	for ( int i = DEPTH*SIZE_PER_DEPTH; i < X._TotalSize; ++i )
		Ret._data[i] = X._data[i];
	return Ret;
}
template<typename TPropagationData>
decltype(auto) ToArray( const TPropagationData& X )
{
	return X._data;
}
template<size_t N>
decltype(auto) ToArray( const std::array<double, N>& X )
{
	return X;
}

template<size_t DEPTH, size_t SIZE_PER_DEPTH, size_t EXTRA_SINGLES>
auto GetTranspose( const CPropagationData<DEPTH,SIZE_PER_DEPTH,EXTRA_SINGLES>& X )
{
	CMatrix<1, CPropagationData<DEPTH, SIZE_PER_DEPTH, EXTRA_SINGLES>::_TotalSize> Ret;
	Ret[0] = X._data;
	return Ret;
}

template<size_t IN_DEPTH, size_t SIZE_PER_DEPTH, size_t EXTRA_SINGLES>
template<size_t OUT_DEPTH>
CConvolutionMatrix<
	CPropagationData<OUT_DEPTH, SIZE_PER_DEPTH, EXTRA_SINGLES>,
	CPropagationData<IN_DEPTH, SIZE_PER_DEPTH, EXTRA_SINGLES>
> CPropagationData<IN_DEPTH, SIZE_PER_DEPTH, EXTRA_SINGLES>::CreateConvLayer( SIZET<OUT_DEPTH> Dummy1, int nRadius )
{
	static_assert(SIZE_PER_DEPTH >= WIDTH*HEIGHT, "CPropagationData only meant for (modified) field representations");
	CConvolutionMatrix<
		CPropagationData<OUT_DEPTH, SIZE_PER_DEPTH, EXTRA_SINGLES>,
		CPropagationData<IN_DEPTH, SIZE_PER_DEPTH, EXTRA_SINGLES>
	> Ret( GetRandVector( square(1+2*nRadius)*IN_DEPTH*OUT_DEPTH ) );

	for ( int out_depth = 0; out_depth < OUT_DEPTH; ++out_depth )
	{
		for ( int in_depth = 0; in_depth < IN_DEPTH; ++in_depth )
		{
			for ( int out_row = 0; out_row < HEIGHT; ++out_row )
			{
				for ( int out_col = 0; out_col < WIDTH; ++out_col )
				{
					for ( int in_row = out_row - nRadius; in_row <= out_row + nRadius; ++in_row )
					{
						if ( in_row < 0 || in_row >= HEIGHT ) continue;
						for ( int in_col = out_col - nRadius; in_col <= out_col + nRadius; ++in_col )
						{
							if ( in_col < 0 || in_col >= WIDTH ) continue;
							Ret._entries.emplace_back(
								out_depth * SIZE_PER_DEPTH + out_row * WIDTH + out_col,
								in_depth * SIZE_PER_DEPTH + in_row * WIDTH + in_col,
								SExplicitIndex(
									in_col - (out_col - nRadius) +
									( in_row - ( out_row - nRadius) ) * (1+2*nRadius) + 
									in_depth * square(1+2*nRadius) +
									out_depth * IN_DEPTH * square(1+2*nRadius)
								),
								true
							);
						}
					}
				}
			}
			for ( int i = WIDTH*HEIGHT; i < SIZE_PER_DEPTH; ++i )
			{
				Ret._entries.emplace_back( out_depth * SIZE_PER_DEPTH + i, in_depth * SIZE_PER_DEPTH + i, GetSignedUnitRand(), true );
			}
		}
	}
	for ( int i = 0; i < EXTRA_SINGLES; ++i )
	{
		Ret._entries.emplace_back( SIZE_PER_DEPTH * OUT_DEPTH + i, SIZE_PER_DEPTH * IN_DEPTH + i, 1.0, false );
	}

	return Ret;
}

template<typename TPropagationData>
auto CreateInternalizerLayer()
{
	using TRealType = typename std::remove_reference<TPropagationData>::type;
	constexpr static size_t DEPTH = TRealType::_Depth;
	constexpr static size_t SIZE_PER_DEPTH = TRealType::_SizePerDepth;
	constexpr static size_t SINGLES = TRealType::_ExtraSingles;
	CConvolutionMatrix<
		CPropagationData<DEPTH, SIZE_PER_DEPTH, 0>,
		CPropagationData<DEPTH, SIZE_PER_DEPTH, SINGLES>
	> Ret;
	for ( size_t nDepth = 0; nDepth < DEPTH; ++nDepth )
	{
		for ( size_t i = 0; i < SIZE_PER_DEPTH; ++i )
		{
			const size_t nPos = nDepth * SIZE_PER_DEPTH + i;
			Ret._entries.emplace_back( nPos, nPos, 1.0, false );
			for ( size_t nExtra = 0; nExtra < SINGLES; ++nExtra )
			{
				Ret._entries.emplace_back(
					nPos, SIZE_PER_DEPTH * DEPTH + nExtra, 0.1, false
				);
			}
		}
	}
	return Ret;
}