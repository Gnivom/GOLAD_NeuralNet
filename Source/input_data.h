#pragma once

#include "game_field.h"
#include "move.h"
#include "NeuralNet/propagation_data.h"
#include "bot.h"

template<size_t DEPTH = 1, size_t ABS = 0, typename TBot = CPassBot>
struct SSimulatedInput
{
	constexpr static size_t OUT_DEPTH = DEPTH * (ABS+1);
	static CGameField GetSuccessorField( const CGameField& Field )
	{
		static const TBot Bot;
		return Field.GetSuccessor( Bot.ChooseMove( CGame( Field ) ) );
	}
	static auto Get( CGameField Field, int nPlayer )
	{
		CPropagationData<OUT_DEPTH, HEIGHT*WIDTH+1, 1> Ret;
		const int nPlayerToMove = Field._player_to_move;
		const double vGood = double( nPlayer ) / OUT_DEPTH;
		for ( size_t d = 0; d < DEPTH; ++d )
		{
			if ( d != 0 && Field._winner == CGameField::UNDETERMINED )
				Field = GetSuccessorField( Field );
			for ( size_t nAbs = 0; nAbs <= ABS; ++nAbs )
			{
				const size_t nDepth = d*(ABS+1)+nAbs;
				for ( int row = 0; row < HEIGHT; ++row )
				{
					for ( int col = 0; col < WIDTH; ++col )
					{
						Ret.AccessData( nDepth, row*WIDTH+col ) = nAbs
							? abs( vGood * Field.GetSquare( row, col ) )
							: vGood * Field.GetSquare( row, col );
					}
				}
				Ret.AccessData( nDepth, WIDTH*HEIGHT ) = Field._winner == CGameField::UNDETERMINED ? 0.0 : vGood * Field._winner * 10;
			}
		}
		Ret.AccessExtra( 0 ) = vGood * nPlayerToMove;
		return Ret;
	}
	constexpr static size_t SIZE = decltype(Get( NewField(), 1 ))::_TotalSize;
};

struct SUniformInputScore
{
	static auto Get()
	{
		std::vector<double> Ret( 1 << HEIGHT, 0.0 );
		for ( int i = 0; i < Ret.size(); ++i )
		{
			for ( int row = 0; row < HEIGHT; ++row )
				if ( (i >> row) & 1 )
					Ret[i] += 1.0;
		}
		return Ret;
	}
	constexpr static bool BOOST_ENEMY = false;
};
struct SSpecialInputScore
{
	static auto Get()
	{
		std::vector<double> Ret( 1 << HEIGHT, 0.0 );
		for ( int i = 0; i < Ret.size(); ++i )
		{
			for ( int row = 0; row < HEIGHT; ++row )
				if ( (i >> row) & 1 )
					Ret[i] += 1.0;
		}
		return Ret;
	}
	constexpr static bool BOOST_ENEMY = true;
};

template<size_t DEPTH, typename TInputScore = SUniformInputScore>
struct SDominationInput
{
	static auto Get( CGameField Field, int nPlayer )
	{
		std::array<double, DEPTH> Ret = {};
		for( int nDepth = 0; nDepth < DEPTH; ++nDepth )
		{
			if ( nDepth != 0 && Field._winner == CGameField::UNDETERMINED )
				Field = NextField( Field );
			
			const auto& GoodBitMasks = nPlayer == 1 ? Field._GoodBitMask : Field._BadBitMask;
			const auto& BadBitMasks = nPlayer == -1 ? Field._GoodBitMask : Field._BadBitMask;
			
			static const auto Score = TInputScore::Get();

			double vGood = 0;
			double vBad = 0;
			for ( int col = 0; col < WIDTH; ++col )
			{
				vGood += Score[GoodBitMasks[col]];
				vBad += Score[BadBitMasks[col]];
			}
			if ( TInputScore::BOOST_ENEMY )
				vBad *= 2;

			if ( vGood == 0.0 && vBad == 0.0 )
				Ret[nDepth] = 0.0;
			else
				Ret[nDepth] = ( ( vGood / ( vGood + vBad ) ) -0.5 ) * 2;
		}
		return Ret;
	}
	constexpr static size_t SIZE = DEPTH;
};

template<size_t DEPTH = 1>
struct SMixedInput
{
	constexpr static size_t OUT_DEPTH = DEPTH;
	static auto Get( CGameField Field, int nPlayer )
	{
		CPropagationData<OUT_DEPTH, HEIGHT*WIDTH, DEPTH> Ret;
		const int nPlayerToMove = Field._player_to_move;
		const double vGood = double( nPlayer ) / ( OUT_DEPTH * HEIGHT*WIDTH );
		for ( size_t d = 0; d < DEPTH; ++d )
		{
			if ( d != 0 && Field._winner == CGameField::UNDETERMINED )
				Field = NextField( Field );
			int nGood = 0;
			int nBad = 0;
			for ( int row = 0; row < HEIGHT; ++row )
			{
				for ( int col = 0; col < WIDTH; ++col )
				{
					ELifeMode X = Field.GetSquare( row, col );
					if ( X == nPlayer )
						++nGood;
					else if ( X == -nPlayer )
						++nBad;
					Ret.AccessData( d, row*WIDTH+col ) = vGood * X;
				}
			}
			if ( nGood == 0 && nBad == 0 )
				Ret.AccessExtra( d ) = 0.0;
			else
				Ret.AccessExtra( d ) = (((double( nGood ) / double( nGood + nBad )) -0.5) * 2) / DEPTH;
		}
		return Ret;
	}
	constexpr static size_t SIZE = decltype(Get( NewField(), 1 ))::_TotalSize;
};

struct SMoveDeltaInput
{
	enum EDepths { BIRTH_NEAR, KILL_ME_NEAR, KILL_ENEMY_NEAR, BIRTH_HERE, KILL_ME_HERE, KILL_ENEMY_HERE, NUM_OF };
	static CPropagationData< NUM_OF, WIDTH*HEIGHT, 1 > Get( const CGameField& Field, int nPlayer )
	{
		CPropagationData< NUM_OF, WIDTH*HEIGHT, 1 > Ret;
		// Depth 0: how am I affected by 1 extra neighbor (of nPlayer type).
		// Depth 1: how am I affected by 1 less neighbor (of nPlayer type).
		// Depth 2: how am I affected by 1 less neighbor (of -nPlayer type).
		// Depth 3: how am I affected by BIRTH here
		// Depth 4: how am I affected by KILL_ME here
		// Depth 5: how am I affected by KILL_ENEMY here
		// Extra 0: Domination after pass

		for ( int row = 0; row < HEIGHT; ++row )
		{
			for ( int col = 0; col < WIDTH; ++col )
			{
				int nNeighbors = 0;
				int nDominant = 0;
				for ( int nOtherRow = row - 1; nOtherRow <= row + 1; ++nOtherRow )
				{
					if ( nOtherRow < 0 || nOtherRow >= HEIGHT )
						continue;
					for ( int nOtherCol = col - 1; nOtherCol <= col + 1; ++nOtherCol )
					{
						if ( nOtherCol < 0 || nOtherCol >= WIDTH )
							continue;
						if ( nOtherRow == row && nOtherCol == col )
							continue;
						ELifeMode Y = Field.GetSquare( nOtherRow, nOtherCol );
						if ( Y != DEAD )
						{
							++nNeighbors;
							nDominant += int( Y ) * nPlayer;
						}
					}
				}
				int X = ELifeMode( int( Field.GetSquare( row, col ) ) * nPlayer );
				const int nPos = row*WIDTH+col;
				const double vImpossible = -100000.0;
				if ( X != DEAD )
				{
					Ret.AccessData( BIRTH_HERE, nPos ) = vImpossible;
					if ( X == GOOD )
					{
						Ret.AccessData( KILL_ENEMY_HERE, nPos ) = vImpossible;
						if ( nNeighbors == 2 )
							Ret.AccessData( KILL_ME_HERE, nPos ) = -1;
						else if ( nNeighbors == 3 && nDominant < 0 )
							Ret.AccessData( KILL_ME_HERE, nPos ) = -2;
					}
					else
					{
						Ret.AccessData( KILL_ME_HERE, nPos ) = vImpossible;
						if ( nNeighbors == 2 )
							Ret.AccessData( KILL_ENEMY_HERE, nPos ) = 1;
						else if ( nNeighbors == 3 && nDominant > 0 )
							Ret.AccessData( KILL_ENEMY_HERE, nPos ) = 2;
					}

					if ( nNeighbors == 1 ) // Increase to 2
						Ret.AccessData( BIRTH_NEAR, nPos ) = X;
					else if ( nNeighbors == 3 ) // Increase to 4
						Ret.AccessData( BIRTH_NEAR, nPos ) = -X;
					else if ( nNeighbors == 4 ) // Decrease to 3
					{
						Ret.AccessData( KILL_ME_NEAR, nPos ) = X;
						Ret.AccessData( KILL_ENEMY_NEAR, nPos ) = X;
					}
					else if ( nNeighbors == 2 ) // Decrease to 1
					{
						Ret.AccessData( KILL_ME_NEAR, nPos ) = -X;
						Ret.AccessData( KILL_ENEMY_NEAR, nPos ) = -X;
					}
				}
				else
				{
					Ret.AccessData( KILL_ME_HERE, nPos ) = vImpossible;
					Ret.AccessData( KILL_ENEMY_HERE, nPos ) = vImpossible;
					if ( nNeighbors == 2 )
					{
						Ret.AccessData( BIRTH_HERE, nPos ) = 1;
						Ret.AccessData( BIRTH_NEAR, nPos ) = nDominant + 1 > 0 ? 1 : -1;
					}
					else if ( nNeighbors == 3 )
					{
						if ( nDominant < 0 )
							Ret.AccessData( BIRTH_HERE, nPos ) = 2;
						Ret.AccessData( BIRTH_NEAR, nPos ) = nDominant > 0 ? -1 : 1;
						Ret.AccessData( KILL_ME_NEAR, nPos ) = nDominant - 1 > 0 ? -1 : 1;
						Ret.AccessData( KILL_ENEMY_NEAR, nPos ) = nDominant + 1 > 0 ? -1 : 1;
					}
					else if ( nNeighbors == 4 )
					{
						Ret.AccessData( KILL_ME_NEAR, nPos ) = nDominant - 1 > 0 ? 1 : -1;
						Ret.AccessData( KILL_ENEMY_NEAR, nPos ) = nDominant + 1 > 0 ? 1 : -1;
					}
				}
			}
		}
		Ret.AccessExtra( 0 ) = SDominationInput<1>::Get( NextField( Field ), nPlayer )[0];
		return Ret;
	}
	constexpr static size_t SIZE = decltype(Get( NewField(), 1 ))::_TotalSize;
};