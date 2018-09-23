#include "move.h"
#include "game_field.h"
#include "util.h"

#include <set>
#include <iostream>

std::string GetCoordinateName( const FieldSquare& x )
{
	return std::to_string( x.second ) + std::string( "," ) + std::to_string( x.first );
}

std::string GetMoveName( const TMoveIdentifier& Move )
{
	if ( Move.size() == 0 )
		return GetPass().GetName();
	else if ( Move.size() == 1 )
		return CKill( Move[0].first ).GetName();
	else if ( Move.size() == 3 )
	{
		if ( Move[0].second && !Move[1].second && !Move[2].second )
			return CBirth( Move[0].first, Move[1].first, Move[2].first ).GetName();
		else if ( Move[1].second && !Move[2].second && !Move[0].second )
			return CBirth( Move[1].first, Move[2].first, Move[0].first ).GetName();
		else if ( Move[2].second && !Move[0].second && !Move[1].second )
			return CBirth( Move[2].first, Move[0].first, Move[1].first ).GetName();
	}
	return "Unknown Move!";
}

std::vector<const CMove*> GetValidMoves(const CGameField& Field, int nSampleBirths )
{
	std::vector<const CMove*> Ret;
	Ret.push_back( &GetPass() );
	for ( const FieldSquare& X : AllFieldSquares )
	{
		if ( Field.GetSquare( X ) != DEAD )
			Ret.push_back( &GetKill( ToInt( X ) ) );
	}
	std::vector<int> SacrificeInts;
	std::vector<int> BirthInts;
	for ( int i = 0; i < AllFieldSquares.size(); ++i )
	{
		const ELifeMode life = Field.GetSquare( AllFieldSquares[i] );
		if ( life == DEAD )
			BirthInts.push_back( i );
		else if ( life == Field._player_to_move )
			SacrificeInts.push_back( i );
	}
	const int nValidBirths = int(SacrificeInts.size() * (SacrificeInts.size() - 1) * BirthInts.size()) / 2;
	if ( nSampleBirths == -1 || nSampleBirths >= nValidBirths )
	{
		for ( int nSacrifice1 : SacrificeInts )
		{
			for ( int nSacrifice2 : SacrificeInts ) if ( nSacrifice2 > nSacrifice1 )
			{
				for ( int nBirth : BirthInts )
					Ret.push_back( &GetBirth( nBirth, nSacrifice1, nSacrifice2 ) );
			}
		}
	}
	else
	{
		std::set<int> SelectedBirths;
		KnuthShuffle( SacrificeInts );
		KnuthShuffle( BirthInts );
		for ( int i = nValidBirths - nSampleBirths; i < nValidBirths; ++i )
		{
			const int nRand = SafeRand() % (i + 1);
			const int nSelected = SelectedBirths.count( nRand ) ? i : nRand;
			SelectedBirths.insert( nSelected );
		}
		for ( size_t i : SelectedBirths )
		{
			const size_t nBirth = i % BirthInts.size();
			i /= BirthInts.size();

			size_t nSacrifice1 = 0;
			while ( i >= SacrificeInts.size() - nSacrifice1 - 1 )
				i -= SacrificeInts.size() - nSacrifice1++ - 1;
			const size_t nSacrifice2 = nSacrifice1 + 1 + i;

			Ret.push_back( &GetBirth( BirthInts[nBirth], SacrificeInts[nSacrifice1], SacrificeInts[nSacrifice2] ) );
		}
	}
	return Ret;
}

const CPass& GetPass()
{
	static const CPass pass;
	return pass;
}
const CKill& GetKill( size_t nKill )
{
	const auto& AllKills = GetKills();
	return AllKills[nKill];
}
const CBirth& GetBirth( size_t nBirth, size_t nSacrifice1, size_t nSacrifice2 )
{
	const auto& AllBirths = GetBirths();
	if ( nSacrifice1 == nSacrifice2 ) std::cerr << "Invalid Birth move!" << std::endl;
	if ( nSacrifice1 > nSacrifice2 ) std::swap( nSacrifice1, nSacrifice2 );
	return AllBirths[nBirth][nSacrifice1][nSacrifice2 - nSacrifice1 - 1];
}
const std::vector<CKill>& GetKills()
{
	static const std::vector<CKill> Ret = []() {
		std::vector<CKill> kills;
		for ( const auto& square : AllFieldSquares )
			kills.emplace_back( square );
		return kills;
	}();
	return Ret;
}

const std::vector<std::vector<std::vector<CBirth>>>& GetBirths()
{
	static const auto CreateRet = []() {
		std::vector<std::vector<std::vector<CBirth>>> births;
		const size_t N = AllFieldSquares.size();
		births.reserve( N );
		for ( size_t nBirth = 0; nBirth < N; ++nBirth )
		{
			births.push_back( {} );
			births.back().reserve( N );
			for ( size_t nSacrifice1 = 0; nSacrifice1 < N; ++nSacrifice1 )
			{
				births.back().push_back( {} );
				births.back().back().reserve( N - nSacrifice1 - 1 );
				for ( size_t nSacrifice2 = nSacrifice1 + 1; nSacrifice2 < N; ++nSacrifice2 )
					births.back().back().emplace_back( AllFieldSquares[nBirth], AllFieldSquares[nSacrifice1], AllFieldSquares[nSacrifice2] );
			}
		}
		return births;
	};
	static const std::vector<std::vector<std::vector<CBirth>>> Ret = CreateRet();
	return Ret;
}

std::vector<CBirth> GetSimilarBirths( const CBirth& Origin, const CGameField& Field )
{
	std::vector<CBirth> Ret;
	for ( FieldSquare Other : AllFieldSquares )
	{
		if ( Other == Origin._target || Other == Origin._sacrifice1 || Other == Origin._sacrifice2 )
			continue;
		if ( Field.GetSquare( Other ) == DEAD )
		{
			CBirth NextBirth = Origin;
			NextBirth._target = Other;
			Ret.push_back( NextBirth );
		}
		else if ( Field.GetSquare( Other ) == Field._player_to_move )
		{
			{
				CBirth NextBirth = Origin;
				NextBirth._sacrifice1 = Other;
				Ret.push_back( NextBirth );
			}
			{
				CBirth NextBirth = Origin;
				NextBirth._sacrifice2 = Other;
				Ret.push_back( NextBirth );
			}
		}
	}
	return Ret;
}

inline CKill::CKill( FieldSquare target ) : _target( target ) {}

inline int CKill::GetIdentifierInt() const { return 1 + ToInt( _target ); }

inline TMoveIdentifier CKill::GetIdentifierVector() const { return { { _target, false } }; }

inline std::string CKill::GetName() const { return "kill " + GetCoordinateName( _target ); }

inline CBirth::CBirth( FieldSquare target, FieldSquare sacrifice1, FieldSquare sacrifice2 )
	: _target( target ), _sacrifice1( sacrifice1 ), _sacrifice2( sacrifice2 )
{}

inline int CBirth::GetIdentifierInt() const { return -1; }

inline TMoveIdentifier CBirth::GetIdentifierVector() const { return { { _target, true },{ _sacrifice1, false },{ _sacrifice2, false } }; }

inline std::string CBirth::GetName() const { return "birth " + GetCoordinateName( _target ) + " " + GetCoordinateName( _sacrifice1 ) + " " + GetCoordinateName( _sacrifice2 ); }


bool operator<( const CBirth& lhs, const CBirth& rhs )
{
	return std::make_tuple( lhs._target, lhs._sacrifice1, lhs._sacrifice2 )
		<  std::make_tuple( rhs._target, rhs._sacrifice1, rhs._sacrifice2 );
}