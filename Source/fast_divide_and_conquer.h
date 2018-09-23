#pragma once
#include "divide_and_conquer.h"

template<typename TPolicyNet, typename... Ts>
class CFastDivideAndConquer : public CDivideAndConquer<Ts...>
{
public:
	CFastDivideAndConquer( const TPolicyNet& PolicyNet, const CDivideAndConquer<Ts...>& BaseBot )
		: CDivideAndConquer<Ts...>( BaseBot ), _PolicyNet( PolicyNet )
	{}
	TPolicyNet _PolicyNet;
protected:
	typename CDivideAndConquer<Ts...>::SDivision CreateDivisionFast( const CGameField& Field, int nRecursionDepth ) const override;
	typename CDivideAndConquer<Ts...>::SDivision CreateDivisionFastX( const CGameField& Field, int nRecursionDepth ) const;
	
	void ProposeMovesFromDivision( CCandidateList<TMoveIdentifier>& Candidates, const typename CDivideAndConquer<Ts...>::SDivision& Division, const CGameField& Field, int nSamples ) const override;
};

template<typename TPolicyNet, typename... Ts>
auto MakeFast( const TPolicyNet& PolicyNet, const CDivideAndConquer<Ts...>& BaseBot )
{
	return CFastDivideAndConquer<TPolicyNet, Ts...>( PolicyNet, BaseBot );
}

template<typename TPolicyNet, typename ...Ts>
auto CFastDivideAndConquer<TPolicyNet, Ts...>::CreateDivisionFast( const CGameField& Field, int nRecursionDepth ) const
-> typename CDivideAndConquer<Ts...>::SDivision
{
	CPropagationData<3, WIDTH*HEIGHT, 0> Policy =
		SForwardProp<TPolicyNet>( _PolicyNet, Field, Field._player_to_move )._Y;

	const int nSamples = this->_ParametersPerDepth[nRecursionDepth]._nDivisionSamples;

	CCandidateList<TMovePart> BirthCandidates( std::max( nSamples / 2, 2 ) );
	CCandidateList<TMovePart> KillMeCandidates( std::max( nSamples / 3, 4 ) );
	CCandidateList<TMovePart> KillEnemyCandidates( std::max( nSamples / 6, 2 ) );

	std::vector< decltype(BirthCandidates)* > Candidates = { &BirthCandidates, &KillMeCandidates, &KillEnemyCandidates };
	for ( bool bBirth : { true, false } )
	{
		for ( const FieldSquare& Square : AllFieldSquares )
		{
			TMovePart MovePart( Square, bBirth );
			if ( !Field.IsValidMove( MovePart, false ) )
				continue;

			ELifeMode X = Field.GetSquare( Square );
			if ( X == DEAD )
				Candidates[0]->Propose( Policy.GetData( 0, ToInt( Square ) ), MovePart );
			else
			{
				if ( X == Field._player_to_move )
					Candidates[1]->Propose( Policy.GetData( 1, ToInt( Square ) ), MovePart );
				else
					Candidates[2]->Propose( Policy.GetData( 2, ToInt( Square ) ), MovePart );
			}
		}
	}
	bool bFirstNullBirth = true;
	const CGameField PassField = NextField( Field );
	typename CDivideAndConquer<Ts...>::SDivision Division;
	std::vector< decltype(Division._Birth)* > Output = { &Division._Birth, &Division._KillMe, &Division._KillEnemy };
	for ( int i = 0; i < 3; ++i )
	{
		Output[i]->reserve( Candidates[i]->Get().size() );
		for ( const auto& Candidate : Candidates[i]->Get() )
		{
			const CGameField Next = Field.GetSuccessor( { Candidate.second } );
			if ( Candidate.second.second && Next == PassField )
			{
				if ( !bFirstNullBirth )
					continue;
				bFirstNullBirth = false;
			}
			Output[i]->emplace_back( this->PredictOutcome( Next, Field._player_to_move ), Candidate.second );
		}
	}

	std::sort( Division._Birth.rbegin(), Division._Birth.rend() );
	std::sort( Division._KillMe.rbegin(), Division._KillMe.rend() );
	std::sort( Division._KillEnemy.rbegin(), Division._KillEnemy.rend() );

	return Division;
}

template<typename TPolicyNet, typename... Ts>
void CFastDivideAndConquer<TPolicyNet, Ts...>::ProposeMovesFromDivision(
	CCandidateList<TMoveIdentifier>& Candidates,
	const typename CDivideAndConquer<Ts...>::SDivision& Division,
	const CGameField& Field, int nSamples ) const
{
	this->CDivideAndConquer<Ts...>::ProposeMovesFromDivision( Candidates, Division, Field, nSamples );
}