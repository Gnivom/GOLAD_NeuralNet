#pragma once

#include "NeuralNet/matrix.h"

#include "settings.h"
#include "util.h"

#include <array>
#include <vector>
#include <tuple>

int ToInt( FieldSquare x );
const auto AllFieldSquares = []() { // TODO (C++17): Make constexpr
	std::array<FieldSquare, HEIGHT*WIDTH> Ret;
	for (int i = 0; i < HEIGHT; ++i) for (int j = 0; j < WIDTH; ++j)
	{
		Ret[i*WIDTH + j] = FieldSquare(i, j);
	}
	return Ret;
}();

enum ELifeMode : signed char
{
	DEAD = 0,
	GOOD = 1,
	BAD = -1
};
ELifeMode operator-( ELifeMode x );

class CMove;
class CGameField
{
public:
	void SetSquare( unsigned char row, unsigned char col, ELifeMode value );
	ELifeMode GetSquare( unsigned char row, unsigned char col ) const;
	void SetSquare( FieldSquare s, ELifeMode value ) { SetSquare( s.first, s.second, value ); }
	ELifeMode GetSquare( FieldSquare s ) const { return GetSquare( s.first, s.second ); };

	bool IsSameField( const CGameField& Other ) const;
	CGameField GetSuccessor( const CMove& Move ) const;
	CGameField GetSuccessor( const TMoveIdentifier& Move ) const;
	bool IsValidMove( const TMoveIdentifier& Move, bool bKillOnlySelf ) const;
	bool IsValidMove( const TMovePart& Move, bool bKillOnlySelf ) const;
	void ReadFromAPI( const std::string& str );
	void ReadFromStream( std::istream& input );
	void WriteToStream( std::ostream & output ) const;

	bool operator==( const CGameField& O ) const;
	bool operator!=( const CGameField& O ) const { return !(*this == O); }

// Also public
	std::array<COLMASK, WIDTH> _GoodBitMask = {};
	std::array<COLMASK, WIDTH> _BadBitMask = {};

	enum EResult : signed char {
		UNDETERMINED = -2,
		NEGATIVE = -1,
		DRAW = 0,
		POSITIVE = 1,
	};
	EResult _winner = UNDETERMINED; // 0 means draw
	signed char _player_to_move = 1;
	short _time = 0;
	ELifeMode _last_killed = DEAD;
};

CGameField NewField( int player_to_move = 1 );

CGameField NextFieldSlow( const CGameField& Field );
CGameField NextFieldFast( const CGameField& Field );
CGameField NextFieldSuperFast( const CGameField& Field );
CGameField NextField( const CGameField& Field ); // Alias for NextFieldSuperFast

std::vector<CGameField> GetSymmetries( const CGameField& Field );

void PrintField(const CGameField& Field);
void DrawNumberFancy( double v, bool bAllowNegative = true ); // [-1,1] or [0,1]

class CMove;
class CGame
{
public:
	CGame() {}
	CGame( const CGameField& StartField ) { _FieldsAndMoves.emplace_back( StartField, TMoveIdentifier() ); }
	int GetWinner() const { return _FieldsAndMoves.back().first._winner; }
	void MakeMove( const TMoveIdentifier& Move );
	size_t size() const { return _FieldsAndMoves.size(); }
	const CGameField& GetLastField() const { return _FieldsAndMoves.back().first; }
	CGameField& GetLastField() { return _FieldsAndMoves.back().first; }

	std::vector< std::pair< CGameField, TMoveIdentifier > > _FieldsAndMoves;

	void PushFromAPI( const std::string& str );

	void ReadFromStream( std::istream& input );
	void WriteToStream( std::ostream & output ) const;
};

class CBot;
CGame PlayGame( const CBot& Bot1, const CBot& Bot2, int nStartingBot = 1 );

void PlayAndWriteGames( const CBot& Bot, const std::string& BotName, int N );

void WriteGames( const std::vector<CGame>& Games, std::ostream& output );
std::vector<CGame> ReadGames( std::istream& input );
std::vector<CGame> ReadGames( const std::string& BotName, bool bNoError = false );


struct SFieldCompare
{
	bool operator()( const CGameField& lhs, const CGameField& rhs ) const
	{
		for ( size_t col = 0; col < WIDTH; ++col )
		{
			if ( lhs._GoodBitMask[col] == rhs._GoodBitMask[col] )
				continue;
			return lhs._GoodBitMask[col] < rhs._GoodBitMask[col];
		}
		for ( size_t col = 0; col < WIDTH; ++col )
		{
			if ( lhs._BadBitMask[col] == rhs._BadBitMask[col] )
				continue;
			return lhs._BadBitMask[col] < rhs._BadBitMask[col];
		}
		return false;
	}
};
struct SFieldEqual
{
	bool operator()( const CGameField& lhs, const CGameField& rhs ) const
	{
		return lhs._GoodBitMask == rhs._GoodBitMask && lhs._BadBitMask == rhs._BadBitMask;
	}
};
struct SFieldHasher
{
	auto operator()( const CGameField& Field ) const
	{
		return SuperFastHash( Field._GoodBitMask, Field._BadBitMask );
	}
};
