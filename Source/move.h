#pragma once
#include "game_field.h"
#include "util.h"

#include <vector>
#include <memory>
#include <string>
#include <utility>

class CMove
{
public:
	virtual int GetIdentifierInt() const { return 1; };
	virtual TMoveIdentifier GetIdentifierVector() const = 0; // true = birth, false = kill
	virtual std::string GetName() const = 0;
};

class CPass : public CMove
{
public:
	int GetIdentifierInt() const override { return 0; }
	TMoveIdentifier GetIdentifierVector() const override { return {}; }
	std::string GetName() const override { return "pass"; }
};
class CKill : public CMove
{
public:
	CKill( FieldSquare target );
	int GetIdentifierInt() const override;
	TMoveIdentifier GetIdentifierVector() const override;
	std::string GetName() const override;
private:
	FieldSquare _target;
};
class CBirth : public CMove
{
public:
	CBirth( FieldSquare target, FieldSquare sacrifice1, FieldSquare sacrifice2 );
	int GetIdentifierInt() const override;
	TMoveIdentifier GetIdentifierVector() const override;
	std::string GetName() const override;
	bool operator==( const CBirth& Other ) const { return GetIdentifierVector() == Other.GetIdentifierVector(); }
public:
	FieldSquare _target;
	FieldSquare _sacrifice1;
	FieldSquare _sacrifice2;
};

std::string GetMoveName( const TMoveIdentifier& Move );

std::vector<const CMove*> GetValidMoves(const CGameField& Field, int nSampleBirths = -1);
const CPass& GetPass();
const CKill& GetKill( size_t nKill );
const CBirth& GetBirth( size_t nBirth, size_t nSacrifice1, size_t nSacrifice2 );
const std::vector<CKill>& GetKills();
const std::vector<std::vector<std::vector<CBirth>>>& GetBirths(); // GetBirth(-1)[birth][sacrifice][sacrifice]

std::vector<CBirth> GetSimilarBirths( const CBirth& Origin, const CGameField& Field );

struct SBirthHash
{
	long long operator()(const CBirth& Birth) const
	{
		return ToInt( Birth._target ) * WIDTH * HEIGHT * WIDTH * HEIGHT
			+ ToInt( Birth._sacrifice1 ) * WIDTH * HEIGHT
			+ ToInt( Birth._sacrifice2 );
	}
};
bool operator<( const CBirth& lhs, const CBirth& rhs );