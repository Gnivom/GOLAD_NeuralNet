#pragma once

#include "NeuralNet/matrix.h"
#include "NeuralNet/activation.h"

#include "settings.h"
#include "game_field.h"

#include <array>
#include <vector>
#include <memory>
#include <tuple>
#include <type_traits>

class CGameField;
class CMove;
class CGame;

class CBot
{
public:
	virtual TMoveIdentifier ChooseMove( const CGame& Game ) const = 0;
	virtual void NotifyTimeFactor( double vTimeFactor ) {}
};

class CHeuristicBot : public CBot
{
public:
	CHeuristicBot( int nSampleBirths ) : _nSampleBirths( nSampleBirths ) {}
	TMoveIdentifier ChooseMove( const CGame& Game ) const override;

	virtual double PredictOutcome( const CGameField& Field, int nPlayer ) const = 0;

	void NotifyTimeFactor( double vTimeFactor ) override;

	int _nSampleBirths = -1;
};

class CRandomBot : public CBot
{
public:
	TMoveIdentifier ChooseMove( const CGame& Game ) const override;
};
class CPassBot : public CBot
{
public:
	TMoveIdentifier ChooseMove( const CGame& Game ) const override { return {}; }
};

class CGameGenerator
{
public:
	CGameGenerator( int nOnce = 0 ) : _nOnce( nOnce ) {}
	virtual CGame Generate() { return GenerateConst( _nOnce++ ); };
	virtual CGame GenerateConst( int nOnce ) const = 0;
protected:
	int _nOnce;
};
class CGamePlayer : public CGameGenerator
{
public:
	CGamePlayer( const CBot& Bot ) : _Bot( Bot ) {}
	virtual CGame GenerateConst( int nOnce ) const override
	{
		return PlayGame( _Bot, _Bot );
	};
	const CBot& _Bot;
};
class CGameMemory : public CGameGenerator
{
public:
	CGameMemory( std::vector<CGame>&& Games ) : _Games( std::move(Games) ) {}
	CGameMemory( const std::vector<CGame>& Games ) : _Games( Games ) {}
	virtual CGame GenerateConst( int nOnce ) const override
	{
		return _Games[nOnce % _Games.size()];
	};
	std::vector<CGame> _Games;
};
class CThreadSafeGameGenerator : public CGameGenerator
{
public:
	CThreadSafeGameGenerator( const CGameGenerator& Base, int nOnce )
		: CGameGenerator( nOnce ), _Base( Base )
	{}
	virtual CGame GenerateConst( int nOnce ) const override
	{
		return _Base.GenerateConst( nOnce );
	}
	const CGameGenerator& _Base;
};