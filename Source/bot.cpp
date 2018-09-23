#include "bot.h"
#include "game_field.h"
#include "move.h"
#include "util.h"

#include <iostream>
#include <iomanip>
#include <thread>


TMoveIdentifier CHeuristicBot::ChooseMove( const CGame& Game ) const
{
	int nBestMove = 0;
	double vBestScore = std::numeric_limits<double>::lowest();
	CGameField Field = Game.GetLastField();
	const auto& Moves = GetValidMoves( Field, _nSampleBirths );
	for ( int i = 0; i < Moves.size(); ++i )
	{
		const CMove& Move = *Moves[i];
		const CGameField Next = Field.GetSuccessor( Move );
		double vScore = PredictOutcome( Next, Field._player_to_move );
		if ( vScore > vBestScore )
		{
			vBestScore = vScore;
			nBestMove = i;
		}
	}
	return Moves[nBestMove]->GetIdentifierVector();
}

void CHeuristicBot::NotifyTimeFactor( double vTimeFactor )
{
	if ( _nSampleBirths != -1 )
	{
		_nSampleBirths = std::max( 1, int( vTimeFactor * _nSampleBirths ) );
	}
	std::cerr << "Changing _nSampleBirths to " << _nSampleBirths << std::endl;
}

TMoveIdentifier CRandomBot::ChooseMove( const CGame& Game ) const
{
	const auto& Moves = GetValidMoves( Game.GetLastField(), 200 );
	return Moves[SafeRand() % Moves.size()]->GetIdentifierVector();
}
