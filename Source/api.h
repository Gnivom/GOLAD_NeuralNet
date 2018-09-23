#pragma once
#include "game_field.h"
#include "bot.h"

class CAPIInterface
{
public:
	CAPIInterface() = delete;
#ifndef SMALL_FIELD
	CAPIInterface( CBot& Bot ) : _Bot( Bot ) {}
#endif
	void Play();
	void OutputBestMove( int nTimeLeft );
private:
	CBot& _Bot;
	CGame _Game;
	int _nMyPlyerID = 0; // convert to -1 or 1
};