#pragma once

#include "NeuralNet/propagation_data.h"
#include "NeuralNet/activation.h"

#include "nn_bot.h"
#include "game_field.h"
#include "input_data.h"
#include <algorithm>

template<typename... Ts>
class CDivideAndConquer : public CNNBot<Ts...>
{
protected:
	struct SDivision
	{
		std::vector<std::pair<double, TMovePart>> _Birth;
		std::vector<std::pair<double, TMovePart>> _KillMe;
		std::vector<std::pair<double, TMovePart>> _KillEnemy;
		template<typename T>
		bool operator==( const T& Other ) const { return _Birth == Other._Birth && _KillMe == Other._KillMe && _KillEnemy == Other._KillEnemy; }
	};
public:
	using TBaseClass = CNNBot<Ts...>;
	template<typename... TInits>
	CDivideAndConquer( TInits... Inits ) : TBaseClass( std::forward<TInits>(Inits)... ) {}

	template<size_t RECURSION>
	TMoveIdentifier ChooseMoveFromDivision( const CGameField& Field, const SDivision& Division,
		double* pAverageScore = nullptr, double vAlpha = -2.0, double vBeta = 2.0 ) const;
	TMoveIdentifier ChooseMove( const CGame& Game ) const override;

	void NotifyTimeFactor( double vTimeFactor ) override;

	void PrintRanking( const CGameField& Field, const std::vector<TMoveIdentifier>& Moves ) const;
	template<size_t SIZE_PER_DEPTH, size_t EXTRA_SINGLES>
	double FillPropagationData( CPropagationData<3, SIZE_PER_DEPTH, EXTRA_SINGLES>& Output, const CGameField& Field ) const;

	double PredictOutcome( const CGameField& Field, int nPlayer ) const override;
private:
	SDivision CreateDivision( const CGameField& Field ) const;
protected:
	virtual SDivision CreateDivisionFast( const CGameField& Field, int nRecursionDepth ) const { return CreateDivision( Field ); }
	virtual void ProposeMovesFromDivision( CCandidateList<TMoveIdentifier>& Candidates, const SDivision& Division, const CGameField& Field, int nSamples ) const;
	template<size_t RECURSION>
	CCandidateList<TMoveIdentifier> DoDeepSearch( const CGameField& Field, const CCandidateList<TMoveIdentifier>& Suggested, int nOutput, double vAlpha, double vBeta, bool bUseThreads ) const;
public:
	struct SParameters
	{
		int _nDivisionSamples;
		int _nCombinationSamples;
		int _nBroadSearch;
		int _nDeepSearch;
		SParameters operator*( double v )
		{
			SParameters Ret = *this;
			Ret._nDivisionSamples = int(v*_nDivisionSamples);
			Ret._nCombinationSamples = int(v*_nCombinationSamples);
			Ret._nBroadSearch = std::max( int( v*_nBroadSearch ), 1 );
			Ret._nDeepSearch = std::max( int( v*_nDeepSearch ), 1 );
			return Ret;
		}
	};
	std::vector<SParameters> _ParametersPerDepth = { {100,100,10} };

	mutable long long _TotalTime = 0;
};

template<typename... Ts>
auto MakeDivideAndConquer( const CNNBot<Ts...>& Base )
{
	return CDivideAndConquer<Ts...>( Base );
}

template<typename... Ts>
double CDivideAndConquer<Ts...>::PredictOutcome( const CGameField& Field, int nPlayer ) const
{
	return this->CNNBot<Ts...>::PredictOutcome( Field, nPlayer );
}

template<typename ...Ts>
typename CDivideAndConquer<Ts...>::SDivision CDivideAndConquer<Ts...>::CreateDivision( const CGameField& Field ) const
{
	SDivision Ret;
	const CGameField PassField = NextField( Field );
	bool bFirstPassBirth = true;
	for ( bool bBirth : { true, false } )
	{
		for ( const FieldSquare& Square : AllFieldSquares )
		{
			TMoveIdentifier Move = { std::make_pair( Square, bBirth ) };
			if ( !Field.IsValidMove( Move, false ) )
				continue;
			CGameField NextField = Field.GetSuccessor( Move );
			if ( bBirth && NextField == PassField )
			{
				if ( !bFirstPassBirth )
					continue;
				bFirstPassBirth = false;
			}
			const double vScore = this->PredictOutcome( NextField, Field._player_to_move );
			ELifeMode X = Field.GetSquare( Square );
			if ( X == DEAD )
				Ret._Birth.emplace_back( vScore, Move[0] );
			else
			{
				if ( X == Field._player_to_move )
					Ret._KillMe.emplace_back( vScore, Move[0] );
				else
					Ret._KillEnemy.emplace_back( vScore, Move[0] );
			}
		}
	}
	// Sort, greatest first
	std::sort( Ret._Birth.rbegin(), Ret._Birth.rend() );
	std::sort( Ret._KillMe.rbegin(), Ret._KillMe.rend() );
	std::sort( Ret._KillEnemy.rbegin(), Ret._KillEnemy.rend() );

	return Ret;
}

template<typename ...Ts>
void CDivideAndConquer<Ts...>::ProposeMovesFromDivision( CCandidateList<TMoveIdentifier>& Candidates, const SDivision& Division, const CGameField& Field, int nSamples ) const
{
	const size_t nMaxProduct = size_t( std::ceil( 0.5852 * std::pow( nSamples*2, 0.7042 ) ) ); // nSamples*2 ~ A061201(nMaxProduct). A061201(n) is the number of ordered triples (a,b,c) such that a*b*c <= n.
	for ( size_t nBirth = 0; nBirth < Division._Birth.size(); ++nBirth )
	{
		const size_t nMaxSacrifice1 = std::min( Division._KillMe.size(), nMaxProduct / (1 + nBirth) );
		for ( size_t nSacrifice1 = 0; nSacrifice1 < nMaxSacrifice1; ++nSacrifice1 )
		{
			const size_t nMaxSacrifice2 = std::min(
				Division._KillMe.size(),
				1 + nMaxProduct / ((1+nBirth) * (1+nSacrifice1))
			);
			for ( size_t nSacrifice2 = nSacrifice1+1; nSacrifice2 < nMaxSacrifice2; ++nSacrifice2 )
			{
				TMoveIdentifier Candidate = {
					Division._Birth[nBirth].second,
					Division._KillMe[nSacrifice1].second,
					Division._KillMe[nSacrifice2].second
				};
				const double vScore = this->PredictOutcome( Field.GetSuccessor( Candidate ), Field._player_to_move );
				Candidates.Propose( vScore, Candidate );
			}
		}
	}
}

template<typename ...Ts>
template<size_t RECURSION>
CCandidateList<TMoveIdentifier> CDivideAndConquer<Ts...>::DoDeepSearch(
	const CGameField& Field, const CCandidateList<TMoveIdentifier>& Suggested, int nOutput,
	double vAlpha, double vBeta, bool bUseThreads ) const
{
#ifdef TESTING
	bUseThreads = false;
#endif
	std::vector<double> AllScores;
	const int nNextSeriousCandidates = RECURSION + 1 < _ParametersPerDepth.size()
		? _ParametersPerDepth[RECURSION+1]._nDeepSearch : 0;
	CCandidateList<TMoveIdentifier> Output( nOutput );

	const auto& Me = *this;
	auto DoSearch = [&Field, &Me, nNextSeriousCandidates]( TMoveIdentifier Move, double vScore, double vAlpha, double vBeta )
	{
		constexpr size_t NEXT_RECURSION = RECURSION < 10 ? RECURSION + 1 : 0;
		CGameField SimulatedField = Field.GetSuccessor( Move );
		if ( SimulatedField._winner == CGameField::UNDETERMINED && nNextSeriousCandidates > 0 )
		{
			Me.ChooseMoveFromDivision<NEXT_RECURSION>(
				SimulatedField, Me.CreateDivisionFast( SimulatedField, NEXT_RECURSION ),
				&vScore, vAlpha, vBeta );
			vScore *= -1;
		}
		return std::make_pair( Move, vScore );
	};
	auto RecordResult = [&vAlpha, &vBeta, &AllScores, &Output, &Field]( TMoveIdentifier Move, double vScore )
	{
		AllScores.push_back( vScore );
		if ( vScore > Output.GetLeastScore() )
		{
			Output.Propose( vScore, Move );
			if ( Field._player_to_move == 1 )
				vAlpha = std::max( Output.GetLeastScore(), vAlpha );
			else
				vBeta = std::min( -Output.GetLeastScore(), vBeta );
			if ( vAlpha >= vBeta )
				return true;
		}
		return false;
	};

	std::future<std::pair<TMoveIdentifier, double>> Future;

	for ( auto it = Suggested.Get().rbegin(); it != Suggested.Get().rend(); ++it )
	{
		const auto& Candidate = *it;
		if ( bUseThreads )
		{
			if ( Future.valid() &&
				Future.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready )
			{
				auto Result = Future.get();
				if ( RecordResult( Result.first, Result.second ) )
					break;
			}
			if ( !Future.valid() )
			{
				Future = std::async( DoSearch, Candidate.second, Candidate.first, vAlpha, vBeta );
				continue;
			}
		}
		auto Result = DoSearch( Candidate.second, Candidate.first, vAlpha, vBeta );
		if ( RecordResult( Result.first, Result.second ) )
			break;
	}
	if ( bUseThreads && Future.valid() )
	{
		auto Result = Future.get();
		RecordResult( Result.first, Result.second );
	}
#ifdef _DEBUG
	if ( RECURSION == 0 && nOutput == 1 )
	{
		int i = 0;
		std::cout << std::endl;
		for ( auto x : AllScores )
		{
			if ( x == Output.GetLeastScore() )
				break;
//				std::cout << "#";
//			std::cout << x << " ";
			++i;
		}
//		std::cout << std::endl;
		std::cout << i;
	}
#endif
	return Output;
}

template<typename ...Ts>
template<size_t RECURSION>
TMoveIdentifier CDivideAndConquer<Ts...>::ChooseMoveFromDivision(
	const CGameField& Field,
	const typename CDivideAndConquer<Ts...>::SDivision& Division,
	double* pAverageScore, double vAlpha, double vBeta ) const
{
	const int nDeepSearch = RECURSION < _ParametersPerDepth.size()
		? _ParametersPerDepth[RECURSION]._nDeepSearch : 0;
	if ( nDeepSearch == 0 )
	{
		if ( pAverageScore )
			*pAverageScore = this->PredictOutcome( Field, Field._player_to_move );
		return {};
	}

	SForwardProp<decltype(this->_Layers)> ForwardProp( this->_Layers, Field, Field._player_to_move );

	CCandidateList<TMoveIdentifier> Candidates( _ParametersPerDepth[RECURSION]._nBroadSearch );

	Candidates.Propose( this->PredictOutcome( NextField( Field ), Field._player_to_move ), {} );
	for ( const auto& Kill : Division._KillMe )
		Candidates.Propose( Kill.first, { Kill.second } );
	for ( const auto& Kill : Division._KillEnemy )
		Candidates.Propose( Kill.first, { Kill.second } );

	ProposeMovesFromDivision( Candidates, Division, Field, _ParametersPerDepth[RECURSION]._nCombinationSamples );

	if ( RECURSION < _ParametersPerDepth.size() - 2 )
	{
		Candidates = DoDeepSearch<RECURSION+1>( Field, Candidates, _ParametersPerDepth[RECURSION]._nDeepSearch, -1.0, 1.0, RECURSION == 0 );
	}

	auto Result = DoDeepSearch<RECURSION>( Field, Candidates, 1, vAlpha, vBeta, RECURSION == 0 );
	auto itBest = Result.Get().begin();
	if ( pAverageScore )
		*pAverageScore = itBest->first;
	return itBest->second;
}

template<typename ...Ts>
TMoveIdentifier CDivideAndConquer<Ts...>::ChooseMove( const CGame& Game ) const
{
	const CGameField& Field = Game.GetLastField();

	SDivision Division = CreateDivision( Field );
	return ChooseMoveFromDivision<0>( Field, Division );
}

template<typename... Ts>
void CDivideAndConquer<Ts...>::NotifyTimeFactor( double vTimeFactor )
{
	static std::vector<SParameters> OriginalParams;
	if ( OriginalParams.size() != _ParametersPerDepth.size() )
		OriginalParams = _ParametersPerDepth;
	static double vTotalFactor = 1.0;
	vTotalFactor *= std::pow( vTimeFactor, 1.0/4 );
	for ( int i = 0; i < _ParametersPerDepth.size(); ++i )
	{
		_ParametersPerDepth[i] = OriginalParams[i] * vTotalFactor;
	}
	std::cerr << "vTotalFactor changed to " << vTotalFactor << " (" << vTimeFactor << ")" << std::endl;
}

template<typename... Ts>
void CDivideAndConquer<Ts...>::PrintRanking( const CGameField& Field, const std::vector<TMoveIdentifier>& Moves ) const
{
	SDivision Division = CreateDivisionFast( Field, this->_nSampleBirths );

	auto PrintPosition = []( const TMovePart& MovePart, const decltype(Division._Birth)& Container )
	{
		for ( int i = 0; i < Container.size(); ++i )
		{
			if ( MovePart == Container[i].second )
			{
				std::cout << i << " ";
				return i;
			}
		}
		std::cout << "X ";
		return -2;
	};
	for ( const auto& Move : Moves )
	{
		if ( Move.size() == 0 )
			std::cout << "-";
		else if ( Move.size() == 1 )
		{
			PrintPosition( Move[0], Field.GetSquare( Move[0].first ) == Field._player_to_move ? Division._KillMe : Division._KillEnemy );
		}
		else
		{
			int x = 1;
			x *= PrintPosition( Move[0], Division._Birth )+1;
			x *= PrintPosition( Move[1], Division._KillMe )+1;
			x *= PrintPosition( Move[2], Division._KillMe );
			std::cout << "= " << x;
		}
		std::cout << std::endl;
	}
	std::cout << Division._Birth.size() << " " << Division._KillMe.size() << std::endl << std::endl;
}

template<typename... Ts>
template<size_t SIZE_PER_DEPTH, size_t EXTRA_SINGLES>
double CDivideAndConquer<Ts...>::FillPropagationData( CPropagationData<3, SIZE_PER_DEPTH, EXTRA_SINGLES>& Output, const CGameField& Field ) const
{
	auto Activation = STanHActivation();
	auto f = Activation._f;

	const double vPassScore = this->PredictOutcome( NextField( Field ), Field._player_to_move );
	double vBestScore = vPassScore;
	double vWorstScore = vPassScore;
	for ( int nDepth = 0; nDepth < 3; ++nDepth ) for ( int i = 0; i < WIDTH*HEIGHT; ++i )
	{
		Output.AccessData( nDepth, i ) = -1.0;
	}
	SDivision Division = CreateDivision( Field );
	std::vector< const decltype(Division._Birth)* > Data = { &Division._Birth, &Division._KillMe, &Division._KillEnemy };
	for ( int nDepth = 0; nDepth < 3; ++nDepth )
	{
		const std::vector<std::pair<double, TMovePart>>& DepthData = *Data[nDepth];
		for ( const auto& Entry : DepthData )
		{
			const TMovePart& MovePart = Entry.second;
			const double vScore = Entry.first;
			if ( vScore > vBestScore ) vBestScore = vScore;
			if ( vScore < vWorstScore ) vWorstScore = vScore;
			Output.AccessData( nDepth, ToInt(MovePart.first) ) = vScore;
		}
	}
	return vBestScore - vWorstScore; // Importance
}