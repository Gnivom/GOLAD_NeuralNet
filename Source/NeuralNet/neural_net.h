#pragma once
#include <type_traits>
#include <functional>
#include <iostream>

#include "game_field.h"
#include "matrix.h"
#include "propagation_data.h"

template<size_t N>
struct DENSE {};
template<typename Constructor>
struct SPARSE {};

template<typename T, typename INPUT_TYPE, size_t INPUT_SIZE>
struct SMatrixType {};
template<size_t N, typename INPUT_TYPE, size_t INPUT_SIZE>
struct SMatrixType<DENSE<N>, INPUT_TYPE, INPUT_SIZE>
{
	static auto Create()
	{
		CMatrix<N, INPUT_SIZE> Ret;
		Ret.Randomize();
		return Ret;
	}
};
template<typename Constructor, typename INPUT_TYPE, size_t INPUT_SIZE>
struct SMatrixType<SPARSE<Constructor>, INPUT_TYPE, INPUT_SIZE>
{
	static auto Create() { return Constructor::template Create<INPUT_TYPE>(); }
};

#define AutoVar( lhs, rhs ) decltype( rhs ) lhs = rhs; // Because auto doesn't work
#define AutoVarRef( lhs, rhs ) decltype( rhs )& lhs = rhs;
#define AutoVarCRef( lhs, rhs ) const decltype( rhs )& lhs = rhs;

template<typename... Ts>
struct SNeuralNetwork {};
template<typename TInputGenerator>
struct SNeuralNetwork<TInputGenerator> // 'Layer 0'
{
	SNeuralNetwork( double vFillAll = 0.0 ) {}
	using MyType = SNeuralNetwork<TInputGenerator>;
	constexpr static size_t _N = 0;
	static auto FieldRepresentation( const CGameField& Field, int nPlayer ) -> decltype(TInputGenerator::Get( Field, nPlayer ))
	{
		return TInputGenerator::Get( Field, nPlayer );
	}
	MyType& operator+=( const MyType& Other ) { return *this; }

	typedef decltype(FieldRepresentation( NewField(), 1 )) TOut;
	TOut TOutDummy() const { return TOut(); }
	constexpr static size_t NOut = TInputGenerator::SIZE;
};
template<typename WType, typename Activation, typename... Ts>
struct SNeuralNetwork<WType, Activation, Ts...>
{
	using MyType = SNeuralNetwork<WType, Activation, Ts...>;

	SNeuralNetwork() { _W.Randomize(); _B.Randomize(); }
	SNeuralNetwork( double vFillAll ) : _Prev( vFillAll ) { _B.fill( vFillAll ); _W.fill( vFillAll ); }

	MyType& operator+=( const MyType& Other )
	{
		_Prev += Other._Prev;
		_W += Other._W;
		_B += Other._B;
		return *this;
	}

	SNeuralNetwork<Ts...> _Prev;
	constexpr static size_t In = decltype(_Prev)::NOut;
	constexpr static size_t _N = decltype(_Prev)::_N+1;
	using MatrixType = SMatrixType<WType, decltype(_Prev.TOutDummy()), In>;
	decltype(MatrixType::Create()) _W = MatrixType::Create();
	constexpr static size_t NOut = decltype(_W)::_N;
	CMatrix<NOut, 1> _B;
	Activation _A;
	typedef decltype(_A.f( _W * _Prev.TOutDummy() + _B )) TOut;
	TOut TOutDummy() const { return TOut(); }
};

template<size_t N>
struct SGetLayer
{
	template<typename TLayer>
	static auto Get( const TLayer& Network ) ->
		typename std::enable_if<N == TLayer::_N,const decltype(Network)&>::type
	{ return Network; }
	template<typename TLayer>
	static auto Get( const TLayer& Network ) ->
		typename std::enable_if<N < TLayer::_N, const decltype(Get( Network._Prev ))&>::type
	{ return Get( Network._Prev ); }

	template<typename TLayer>
	static auto Access( TLayer& Network ) ->
		typename std::enable_if<N == TLayer::_N, decltype(Network)&>::type
	{ return Network; }
	template<typename TLayer>
	static auto Access( TLayer& Network ) ->
		typename std::enable_if<N < TLayer::_N, decltype(Access( Network._Prev ))&>::type
	{ return Access( Network._Prev ); }
};

template<typename TNeuralNet>
using FinalOutput = std::array< double, TNeuralNet::NOut >;
template<typename TneuralNet>
using FLossGradient = std::function< FinalOutput<TneuralNet>( const FinalOutput<TneuralNet>& ) >;

template<size_t N>
using SizeTConst = SIZET< N>;

template<typename TNeuralNet, size_t N = TNeuralNet::_N>
struct SForwardProp
{
	constexpr static size_t _N = N;
	SForwardProp<TNeuralNet, N - 1> _Prev;

	const TNeuralNet& _NeuralNet;

	SForwardProp( const TNeuralNet& NeuralNet, const CGameField& Field, int nPlayer )
		: _Prev( NeuralNet, Field, nPlayer ), _NeuralNet( NeuralNet )
	{}

	AutoVarCRef( _Layer, SGetLayer<N>::Get( _NeuralNet ) );
	AutoVarCRef( _X, _Prev._Y );
	AutoVar( _Z, _Layer._W * _X + _Layer._B );
	AutoVar( _Y, _Layer._A.f( _Z ) );
};
template<typename TNeuralNet>
struct SForwardProp<TNeuralNet, 0>
{
	const TNeuralNet& _NeuralNet;
	const CGameField& _Field;
	const int _nPlayer;
	SForwardProp( const TNeuralNet& NeuralNet, const CGameField& Field, int nPlayer ) 
		: _NeuralNet( NeuralNet ), _Field( Field ), _nPlayer( nPlayer )
	{}
	AutoVarCRef( _Layer, SGetLayer<0>::Get( _NeuralNet ) );
	AutoVar( _Y, _Layer.FieldRepresentation( _Field, _nPlayer ) );
};

template<typename TNeuralNet, size_t MAX_N = TNeuralNet::_N, size_t N = 1>
struct SBackwardProp
{
	const SForwardProp<TNeuralNet, N>& _ForwardProp;
	SBackwardProp<TNeuralNet, MAX_N, N + 1> _Prev;
	TNeuralNet& _NeuralNet;
	const double _vDecay;

	SBackwardProp( TNeuralNet& NeuralNet, const SForwardProp<TNeuralNet, MAX_N>& ForwardProp, const FLossGradient<TNeuralNet>& LossGradient, double vDecay )
		: _ForwardProp( SGetLayer<N>::Get( ForwardProp ) ), _Prev( NeuralNet, ForwardProp, LossGradient, vDecay ), _NeuralNet( NeuralNet ), _vDecay( vDecay )
	{}

	double UpdateNeuralNet( double vLearnRate ) const
	{
		_Layer._B *= 1.0 - _vDecay;
		_Layer._W *= 1.0 - _vDecay;
		_Layer._B -= GetTranspose( _E_B ) * vLearnRate;
		_Layer._W -= _E_W * vLearnRate;
		return _Prev.UpdateNeuralNet( vLearnRate );
	}

	AutoVarRef( _Layer, SGetLayer<N>::Access( _NeuralNet ) );

	// Prep
	AutoVarRef( _E_Y, _Prev._E_X ); // For each i, ROW
	AutoVar( _Y_Z, _Layer._A.df( ToArray( _ForwardProp._Z ) ) ); // For each i
	AutoVar( _E_Z, _E_Y * _Y_Z ); // For each i

	// For W
	AutoVar( _E_W, SMatrixMultiplier<typename decltype(_Layer._W)::MatrixType>( _Layer._W )(GetTranspose( _E_Z ), GetTranspose( _ForwardProp._X )) );

	// For B
	AutoVarCRef( _E_B, _E_Z );

	// For next step
	AutoVarCRef( _Z_X, _Layer._W ); // dZ[i] / dX[j] = _Z_X[i][j]
	AutoVar( _E_X, _E_Z * _Z_X ); // For each j, ROW
};

template<typename TNeuralNet, size_t MAX_N>
struct SBackwardProp<TNeuralNet, MAX_N, MAX_N>
{
	constexpr static size_t N = MAX_N;
	const SForwardProp<TNeuralNet, N>& _ForwardProp;
	TNeuralNet& _NeuralNet;
	const FLossGradient<TNeuralNet>& _LossGradient;
	const double _vDecay;

	SBackwardProp( TNeuralNet& NeuralNet, const SForwardProp<TNeuralNet, MAX_N>& ForwardProp, const FLossGradient<TNeuralNet>& LossGradient, double vDecay )
		: _ForwardProp( SGetLayer<MAX_N>::Get( ForwardProp ) ), _NeuralNet( NeuralNet ), _LossGradient( LossGradient ), _vDecay( vDecay )
	{}

	double UpdateNeuralNet( double vLearnRate ) const
	{
		_Layer._B *= 1.0 - _vDecay;
		_Layer._W *= 1.0 - _vDecay;
		_Layer._B -= GetTranspose( _E_B ) * vLearnRate;
		_Layer._W -= _E_W * vLearnRate;
		return MeanAbs( _E_Y );
	}

	AutoVarRef( _Layer, SGetLayer<N>::Access( _NeuralNet ) );

	// Prep
	AutoVarCRef( _E_Y, GetTranspose( _LossGradient( _ForwardProp._Y ) ) );
	AutoVar( _Y_Z, _Layer._A.df( ToArray( _ForwardProp._Z ) ) ); // For each i
	AutoVar( _E_Z, _E_Y * _Y_Z ); // For each i

	// For W
	AutoVar( _E_W, SMatrixMultiplier<typename decltype(_Layer._W)::MatrixType>(_Layer._W)( GetTranspose( _E_Z ), GetTranspose( _ForwardProp._X ) ) );

	// For B
	AutoVarCRef( _E_B, _E_Z );

	// For next step
	AutoVarCRef( _Z_X, _Layer._W ); // dZ[i] / dX[j] = _Z_X[i][j]
	AutoVar( _E_X, _E_Z * _Z_X ); // For each j, ROW
};

template<size_t OUT_DEPTH, size_t RADIUS>
struct SCreateConv
{
	template<typename TPropagationData> // Input type
	static auto Create()
	{
		return TPropagationData::CreateConvLayer( SIZET<OUT_DEPTH>(), RADIUS );
	}
};
struct SCreateInternalizer
{
	template<typename TPropagationData>
	static auto Create()
	{
		return CreateInternalizerLayer<TPropagationData>();
	}
};

#undef AutoVar
#undef AutoVarRef
#undef AutoVarCRef