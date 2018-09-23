#include "game_field.h"
#include "move.h"
#include "bot.h"
#include "util.h"

#include <iostream>
#include <fstream>
#include <string>

ELifeMode operator-( ELifeMode x )
{
	return ELifeMode( -(signed char) x );
}

int ToInt( FieldSquare x )
{
	return x.first*WIDTH+x.second;
}

CGameField NewField( int player_to_move )
{
	static_assert(HEIGHT % 2 == 0, "");
	CGameField Ret = {};
	Ret._player_to_move = player_to_move;
	for (int i = 0; i < START_LIVES; ++i)
	{
		FieldSquare EmptySquare = { -1, -1 };
		do {
			EmptySquare = { SafeRand() % (HEIGHT / 2), SafeRand() % WIDTH };
		} while ( Ret.GetSquare( EmptySquare ) != DEAD );
		Ret.SetSquare( EmptySquare, ELifeMode(-player_to_move) );
	}
	for (int i = 0; i < HEIGHT / 2; ++i) for (int j = 0; j < WIDTH; ++j)
	{
		Ret.SetSquare( { HEIGHT - 1 - i,WIDTH - 1 - j }, -Ret.GetSquare( { i, j } ) );
	}
	return Ret;
}

CGameField NextFieldSlow( const CGameField& Field )
{
	CGameField Ret = {};
	int total[3] = {}; // For win conditions

	for ( int row = 0; row < HEIGHT; ++row ) for ( int col = 0; col < WIDTH; ++col )
	{
		int dominant = 0;
		int neighbors = 0; // Including self
		for ( int i : { row - 1, row, row + 1 } )
		{
			if ( i < 0 || i >= HEIGHT )
				continue;
			for ( int j : {col - 1, col, col + 1} )
			{
				if ( j < 0 || j >= WIDTH )
					continue;
				if ( Field.GetSquare( {i, j} ) != DEAD )
				{
					++neighbors;
					dominant += Field.GetSquare( { i, j } );
				}
			}
		}
		if ( Field.GetSquare( { row, col } ) != DEAD )
		{
			if ( neighbors == 3 || neighbors == 4 ) // NOTE: including self
				Ret.SetSquare( { row,col }, Field.GetSquare( { row, col } ) ); // Stay alive
			else
				Ret.SetSquare( { row,col }, DEAD ); // Die
		}
		else
		{
			if ( neighbors == 3 ) // NOTE: Self does not exist
				Ret.SetSquare( { row,col }, ELifeMode( dominant > 0 ? 1 : -1 ) ); // Birth
		}
		++total[Ret.GetSquare( { row,col } ) +1];
	}
	Ret._time = Field._time + 1;
	Ret._player_to_move = -Field._player_to_move;
	{
		// Win conditions
		if ( total[0] == 0 && total[2] != 0 )
			Ret._winner = CGameField::POSITIVE;
		else if ( total[2] == 0 && total[0] != 0 )
			Ret._winner = CGameField::NEGATIVE;
		else if ( total[0] == 0 || Ret._time >= MAX_ROUNDS )
			Ret._winner = CGameField::DRAW;
	}
	return Ret;
}

CGameField NextFieldFast( const CGameField& Field ) // Heavily optimized version of NextFieldSlow
{
	CGameField Ret;
	int nGood = 0;
	int nBad = 0;

	// Optimization preparation
	std::array<std::array<short, WIDTH+2>, HEIGHT> RowSum;
	std::array<std::array<short, WIDTH+2>, HEIGHT> RowTot;
	for ( int row = 0; row < HEIGHT; ++row )
	{
		auto& RowSumRow = RowSum[row];
		auto& RowTotRow = RowTot[row];
		short nSum = 0, nTot = 0;
		ELifeMode X;
		for ( int col = 0; col < WIDTH; ++col )
		{
			RowSumRow[col] = nSum;
			RowTotRow[col] = nTot;
			X = Field.GetSquare( row, col );
			if ( X != DEAD )
			{
				++nTot;
				if ( X == GOOD )
					++nSum;
				else
					--nSum;
			}
		}
		RowSumRow[WIDTH+1] = RowSumRow[WIDTH] = nSum;
		RowTotRow[WIDTH+1] = RowTotRow[WIDTH] = nTot;
	}
	std::array<std::array<short, WIDTH+2>, HEIGHT+2> WholeSum;
	std::array<std::array<short, WIDTH+2>, HEIGHT+2> WholeTot;
	for ( int col = 0; col < WIDTH+2; ++col )
	{
		const int nPrevCol = std::max( col - 3, 0 );
		short nSum = 0, nTot = 0;
		for ( int row = 0; row < HEIGHT; ++row )
		{
			WholeSum[row][col] = nSum;
			WholeTot[row][col] = nTot;
			nSum += RowSum[row][col] - RowSum[row][nPrevCol];
			nTot += RowTot[row][col] - RowTot[row][nPrevCol];
		}
		WholeSum[HEIGHT+1][col] = WholeSum[HEIGHT][col] = nSum;
		WholeTot[HEIGHT+1][col] = WholeTot[HEIGHT][col] = nTot;
	}


	for ( int row = 0; row < HEIGHT; ++row )
	{
		const int nPrevRow = std::max( row - 1, 0 );
		const auto& WholeSumRow = WholeSum[row+2];
		const auto& WholeSumRowPrev = WholeSum[nPrevRow];
		const auto& WholeTotRow = WholeTot[row+2];
		const auto& WholeTotRowPrev = WholeTot[nPrevRow];
		for ( int col = 0; col < WIDTH; ++col )
		{
			const ELifeMode X = Field.GetSquare( row, col );
			ELifeMode nRet;
			const short neighbors = WholeTotRow[col+2] - WholeTotRowPrev[col+2]; // Including self

			if ( X != DEAD )
			{
				if ( neighbors == 3 || neighbors == 4 ) // NOTE: including self
					nRet = X; // Stay alive
				else
					nRet = DEAD; // Die
			}
			else
			{
				if ( neighbors == 3 ) // NOTE: Self does not exist
					nRet = ELifeMode( WholeSumRow[col+2] > WholeSumRowPrev[col+2] ? 1 : -1 ); // Birth
				else
					nRet = DEAD;
			}
			if ( nRet == GOOD )
				++nGood;
			else if ( nRet == BAD )
				++nBad;
			Ret.SetSquare( row, col, nRet );
		}
	}
	Ret._time = Field._time + 1;
	Ret._player_to_move = -Field._player_to_move;
	{
		// Win conditions
		if ( nBad == 0 && nGood != 0 )
			Ret._winner = CGameField::POSITIVE;
		else if ( nGood == 0 && nBad != 0 )
			Ret._winner = CGameField::NEGATIVE;
		else if ( nBad == 0 || Ret._time >= MAX_ROUNDS )
			Ret._winner = CGameField::DRAW;
	}
	return Ret;
}
CGameField NextFieldSuperFast( const CGameField& Field )
{
	auto GetNeighborsAt = []( const std::array<COLMASK, WIDTH>& BitMask, int col )
	{
		std::array<COLMASK, 8> Ret = {};
		Ret[0] = BitMask[col] << 1;
		Ret[1] = BitMask[col] >> 1;
		if ( col > 0 )
		{
			Ret[2] = BitMask[col-1];
			Ret[3] = BitMask[col-1] << 1;
			Ret[4] = BitMask[col-1] >> 1;
		}
		if ( col < WIDTH - 1 )
		{
			Ret[5] = BitMask[col+1];
			Ret[6] = BitMask[col+1] << 1;
			Ret[7] = BitMask[col+1] >> 1;
		}
		return Ret;
	};

	CGameField Ret;

	std::array<COLMASK, WIDTH> Alive;
	for ( int col = 0; col < WIDTH; ++col )
		Alive[col] = Field._GoodBitMask[col] | Field._BadBitMask[col];

	for ( int col = 0; col < WIDTH; ++col )
	{
		COLMASK TooManyNeighbors = 0;
		COLMASK EnoughNeighbors = 0;
		COLMASK Self = Alive[col];
		{
			COLMASK Sum1 = 0;
			COLMASK Sum2 = 0;
			for ( COLMASK x : GetNeighborsAt( Alive, col ) )
			{
				TooManyNeighbors |= Sum1 & Sum2 & x; // 1 + 2 + 1 -> Too Large
				Sum2 ^= Sum1 & x;
				Sum1 ^= x;
				EnoughNeighbors |= Sum2 & (Sum1 | Self); // 2 + (Self or 1)
			}
		}
		COLMASK GoodDominates = 0; // If at least 2 good neighbors
		{
			COLMASK Good1 = 0;
			for ( COLMASK x : GetNeighborsAt( Field._GoodBitMask, col ) )
			{
				GoodDominates |= Good1 & x;
				Good1 ^= x;
			}
		}
		Ret._GoodBitMask[col] = EnoughNeighbors & ~TooManyNeighbors & ~Field._BadBitMask[col] & (GoodDominates | Self);
		Ret._BadBitMask[col] = EnoughNeighbors & ~TooManyNeighbors & ~Field._GoodBitMask[col] & (~GoodDominates | Self);
	}

	bool bAnyGood = false;
	bool bAnyBad = false;
	for ( int col = 0; col < WIDTH; ++col )
	{
		bAnyGood = bAnyGood || bool(Ret._GoodBitMask[col]);
		bAnyBad = bAnyBad || bool(Ret._BadBitMask[col]);
	}

	Ret._time = Field._time + 1;
	Ret._player_to_move = -Field._player_to_move;
	{
		// Win conditions
		if ( !bAnyBad && bAnyGood )
			Ret._winner = CGameField::POSITIVE;
		else if ( !bAnyGood && bAnyBad )
			Ret._winner = CGameField::NEGATIVE;
		else if ( !bAnyBad || Ret._time >= MAX_ROUNDS )
			Ret._winner = CGameField::DRAW;
	}
	return Ret;
}

CGameField NextField( const CGameField& Field )
{
	return NextFieldSuperFast( Field );
}

// Symmetries
CGameField HorizontalFlip( const CGameField& Field )
{
	CGameField Ret = Field;
	for ( int col = 0; col < WIDTH/2; ++col )
	{
		std::swap( Ret._GoodBitMask[col], Ret._GoodBitMask[WIDTH-1-col] );
		std::swap( Ret._BadBitMask[col], Ret._BadBitMask[WIDTH-1-col] );
	}
	return Ret;
}
CGameField VerticalFlip( const CGameField& Field )
{
	CGameField Ret = Field;
	for ( int row = 0; row < HEIGHT/2; ++row ) for ( int col = 0; col < WIDTH; ++col )
	{
		ELifeMode temp = Ret.GetSquare( row, col );
		Ret.SetSquare( row, col, Ret.GetSquare( HEIGHT-1-row, col ) );
		Ret.SetSquare( HEIGHT-1-row, col, temp );
	}
	return Ret;
}
std::vector<CGameField> GetSymmetries( const CGameField& Field )
{
	std::vector<CGameField> Ret = { Field };
	for ( size_t i = 0, N = Ret.size(); i < N; ++i )
		Ret.push_back( HorizontalFlip( Ret[i] ) );
	for ( size_t i = 0, N = Ret.size(); i < N; ++i )
		Ret.push_back( VerticalFlip( Ret[i] ) );
	return Ret;
}

void PrintField(const CGameField& Field)
{
	std::cerr << "Game at time: " << Field._time << std::endl;
	for (int i = 0; i < HEIGHT; ++i)
	{
		for (int j = 0; j < WIDTH; ++j)
		{
			int x = Field.GetSquare( i, j );
			std::cerr << (x == 1 ? '+' : (x == -1 ? 'o' : '.'));
		}
		std::cerr << std::endl;
	}
	std::cerr << std::endl;
}
void DrawNumberFancy( double v, bool bAllowNegative )
{
	const size_t DOTS = bAllowNegative ? 20 : 40;
	const static std::string Plain = [DOTS]()
	{
		std::string Ret;
		for ( int i = 0; i < DOTS; ++i )
			Ret.push_back( '.' );
		Ret.push_back( '|' );
		for ( int i = 0; i < DOTS; ++i )
			Ret.push_back( '.' );
		return Ret;
	}();
	auto Ret = Plain;
	int nFromZero = int( v * DOTS );
	int nPosition = int( nFromZero + DOTS );
	if ( nPosition >= Ret.size() ) nPosition = int(Ret.size());
	if ( nPosition < 0 ) nPosition = 0;
	Ret[nPosition] = '#';
	std::cout << Ret.substr(bAllowNegative ? 0 : DOTS) << "  " << v << std::endl;
}

std::vector<CGame> ReadGames( std::istream& input )
{
	int N; input >> N;
	std::vector<CGame> Ret( N );
	for ( int i = 0; i < N; ++i )
		Ret[i].ReadFromStream( input );
	return Ret;
}
void WriteGames( const std::vector<CGame>& Games, std::ostream& output )
{
	output << Games.size() << std::endl;
	for ( const CGame& Game : Games )
		Game.WriteToStream( output );
}

CGame PlayGame( const CBot& Bot1, const CBot& Bot2, int nStartingBot ) // Returns all positions as they occured
{
	CGame Game( NewField( nStartingBot ) );
	while ( Game.GetWinner() == -2 ) // none
	{
		const CBot& Bot = Game.GetLastField()._player_to_move == 1 ? Bot1 : Bot2;
		TMoveIdentifier BestMove = Bot.ChooseMove( Game );

		Game.MakeMove( BestMove );
	}
	return Game;
}

namespace
{
	std::string GetGamesFileName( const std::string& BotName )
	{
		std::string FileName = DATA_DIR + BotName;
		FileName += "_" + std::to_string( HEIGHT ) + "_" + std::to_string( WIDTH );
		return FileName + ".games";
	}
}
void PlayAndWriteGames( const CBot& Bot, const std::string& BotName, int N )
{
	// Play games
	auto PlayXGames = [&]( int X )
	{
		std::vector<CGame> Ret;
		for ( int i = 0; i < X; ++i )
			Ret.push_back( PlayGame( Bot, Bot ) );
		return Ret;
	};
	std::vector<CGame> Games = ReadGames( BotName, true );
	auto GamesPerThread = RunThreaded( PlayXGames, ( N - int(Games.size()) ) / THREADS );
	for ( auto& ThreadGames : GamesPerThread )
		for ( CGame& Game : ThreadGames )
			Games.push_back( std::move( Game ) );
	// Write to file
	std::string FileName = GetGamesFileName( BotName );
	std::ofstream output( FileName );
	if ( !output.is_open() )
	{
		std::cerr << "File can't open: " << FileName << std::endl;
		throw std::exception();
	}
	WriteGames( Games, output );
	output.close();
}
std::vector<CGame> ReadGames( const std::string& BotName, bool bNoError )
{
	std::string FileName = GetGamesFileName( BotName );
	std::ifstream input( FileName );
	if ( !input.is_open() )
	{
		if ( bNoError )
			return {};
		std::cerr << "File does not exist: " << FileName << std::endl;
		throw std::exception();
	}
	return ReadGames( input );
}

bool CGameField::operator==( const CGameField& O ) const
{
	return std::make_tuple( _GoodBitMask, _BadBitMask, _winner, _player_to_move, _time, _last_killed )
		== std::make_tuple( O._GoodBitMask, O._BadBitMask, O._winner, O._player_to_move, O._time, O._last_killed );
}

void CGameField::SetSquare( unsigned char row, unsigned char col, ELifeMode value )
{
	if ( value == GOOD )
		_GoodBitMask[col] |= (1 << row);
	else if ( value == BAD )
		_BadBitMask[col] |= (1 << row);
	else
	{
		_GoodBitMask[col] &= ~(1 << row);
		_BadBitMask[col] &= ~(1 << row);
	}
}

ELifeMode CGameField::GetSquare( unsigned char row, unsigned char col ) const
{
	if ( _GoodBitMask[col] & (1 << row) )
		return GOOD;
	else if ( _BadBitMask[col] & (1 << row) )
		return BAD;
	else
		return DEAD;
}

bool CGameField::IsSameField( const CGameField & Other ) const
{
	if ( _GoodBitMask != Other._GoodBitMask || _BadBitMask != Other._BadBitMask )
		return false;
	return true;
}

CGameField CGameField::GetSuccessor( const CMove& Move ) const
{
	return GetSuccessor( Move.GetIdentifierVector() );
}
CGameField CGameField::GetSuccessor( const TMoveIdentifier& Move ) const
{
	CGameField Copy = *this;
	for ( const TMovePart& MovePart : Move )
	{
		Copy.SetSquare( MovePart.first, MovePart.second ? ELifeMode( _player_to_move ) : DEAD );
	}
	return NextField( Copy );
}

bool CGameField::IsValidMove( const TMovePart& MovePart, bool bKillOnlySelf ) const
{
	ELifeMode X = GetSquare( MovePart.first );
	if ( MovePart.second )
	{
		if ( X != DEAD )
			return false;
	}
	else
	{
		if ( bKillOnlySelf )
		{
			if ( X != _player_to_move )
				return false;
		}
		else if ( X == DEAD )
			return false;
	}
	return true;
}
bool CGameField::IsValidMove( const TMoveIdentifier& Move, bool bKillOnlySelf ) const
{
	for ( const TMovePart& MovePart : Move )
	{
		if ( !IsValidMove( MovePart, bKillOnlySelf ) )
			return false;
	}
	return true;
}

void CGameField::ReadFromAPI( const std::string& str )
{
	size_t i = 0;
	for ( char ch : str )
	{
		if ( i >= AllFieldSquares.size() ) std::cerr << "Failed to read Field" << std::endl;
		switch ( ch )
		{
		case '.':
			SetSquare(AllFieldSquares[i++], DEAD);
			break;
		case '0':
			SetSquare( AllFieldSquares[i++], BAD );
			break;
		case '1':
			SetSquare( AllFieldSquares[i++], GOOD );
			break;
		default:
			if( ch != ',' ) std::cerr << "Unknown character when reading Field: " + ch << std::endl;
			break;
		}
	}
}

inline std::istream& operator>>( std::istream& source, ELifeMode& dest )
{
	char a;
	source >> a;
	dest = a == '.' ? DEAD : a == '+' ? GOOD : BAD;
	return source;
}
inline std::ostream& operator<<( std::ostream& dest, ELifeMode source )
{
	return dest << ( source == DEAD ? '.' : source == GOOD ? '+' : '-' );
}

void CGameField::ReadFromStream( std::istream& input )
{
	int winner;
	input >> _time >> winner >> _player_to_move;
	_winner = EResult(winner);
	for ( int row = 0; row < HEIGHT; ++row )
	{
		for ( int col = 0; col < WIDTH; ++col )
		{
			ELifeMode X;
			input >> X;
			SetSquare( row, col, X );
		}
	}
}
void CGameField::WriteToStream( std::ostream& output ) const
{
	output << _time << " " << int(_winner) << " " << _player_to_move << std::endl;
	for ( int row = 0; row < HEIGHT; ++row )
	{
		for ( int col = 0; col < WIDTH; ++col )
		{
			output << GetSquare( row, col ) << " ";
		}
		output << std::endl;
	}
}

void CGame::MakeMove( const TMoveIdentifier& Move )
{
	_FieldsAndMoves.back().second = Move;
	_FieldsAndMoves.emplace_back( _FieldsAndMoves.back().first.GetSuccessor( Move ), TMoveIdentifier() );
}

void CGame::PushFromAPI( const std::string& str )
{
	CGameField Field;
	Field.ReadFromAPI( str );
	_FieldsAndMoves.emplace_back( Field, TMoveIdentifier() );
}

void CGame::ReadFromStream( std::istream& input )
{
	int N; input >> N;
	if ( N == 0 )
		return;
	_FieldsAndMoves.reserve( N );
	CGameField Next;
	Next.ReadFromStream( input );
	for ( int i = 0; i < N; ++i )
	{
		int M; input >> M;
		TMoveIdentifier Move( M );
		for ( int j = 0; j < M; ++j )
		{
			std::string t; input >> t;
			unsigned int row, col; input >> row >> col;
			Move[j].first.first = (unsigned char)row;
			Move[j].first.second = (unsigned char)col;
			Move[j].second = t == "birth";
		}
		_FieldsAndMoves.emplace_back( Next, Move );
		Next = Next.GetSuccessor( Move );
	}
}
void CGame::WriteToStream( std::ostream& output ) const
{
	output << _FieldsAndMoves.size() << std::endl;
	if ( _FieldsAndMoves.size() == 0 )
		return;
	_FieldsAndMoves[0].first.WriteToStream( output );
	for ( const auto& FieldAndMove : _FieldsAndMoves )
	{
		output << FieldAndMove.second.size() << std::endl;
		for ( const std::pair< FieldSquare, bool >& MovePart : FieldAndMove.second )
		{
			if ( MovePart.second )
				output << "birth ";
			else
				output << "kill ";
			output << int( MovePart.first.first ) << " " << int( MovePart.first.second ) << std::endl;
		}
	}
}
