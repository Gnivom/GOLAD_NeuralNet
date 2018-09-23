#pragma once

#include "NeuralNet/neural_net.h"
#include "bot.h"

#include <iostream>
#include <thread>

template<typename NetworkType>
class CNNBot : public CHeuristicBot
{
public:
	CNNBot() {}
	CNNBot( int nSampleBirths ) : CHeuristicBot(nSampleBirths) {}
	double LearnFrom( const CGameField& Field, double vLearnRate, double vWinner, NetworkType* pUpdateToMe ) const;
	void LearnFrom( const CGame& Game, double vLearnRate, NetworkType* pUpdateToMe ) const;

	double PredictOutcome( const CGameField& Field, int nPlayer ) const override;

	NetworkType _Layers;
	constexpr static size_t N_LAYERS = decltype(_Layers)::_N;

	bool _bTest = false;
};

template<typename NetworkType>
double CNNBot<NetworkType>::PredictOutcome( const CGameField& Field, int nPlayer ) const // [-1,1]
{
	const SForwardProp<NetworkType, N_LAYERS> ForwardPropagation( _Layers, Field, nPlayer );
	return Get( ForwardPropagation._Y );
}

template<typename NetworkType>
double CNNBot<NetworkType>::LearnFrom( const CGameField& Field, double vLearnRate, double vWinner, NetworkType* pUpdateToMe ) const
{
	double vRet = 0.0;
	for ( int nPlayer : { -1, 1 } )
	{
		auto MSEGradient = [vFacit = vWinner*nPlayer]( const std::array<double, 1>& Guess )
		{
			return std::array<double, 1>{ Guess[0] - vFacit };
		};
		const double vDecay = 0.00001; // Weight regularization
		const SForwardProp<NetworkType, N_LAYERS> ForwardPropagation( _Layers, Field, nPlayer );
		SBackwardProp<NetworkType, N_LAYERS, 1> BackwardPropagation( *pUpdateToMe, ForwardPropagation, MSEGradient, vDecay );
		const double vError = BackwardPropagation.UpdateNeuralNet( vLearnRate * 0.5 );
		if ( nPlayer == Field._player_to_move )
			vRet = vError;
	}
	return vRet;
}

template<typename NetworkType>
void CNNBot<NetworkType>::LearnFrom( const CGame& Game, double vLearnRate, NetworkType* pUpdateToMe ) const
{
	const double vWinner = double( Game.GetWinner() );
	for ( int i = int( Game.size() - 1 ); i >= 0 && vLearnRate > 0.0; --i )
	{
		const CGameField& Field = Game._FieldsAndMoves[i].first;
		for ( const CGameField& Mirror : GetSymmetries( Field ) )
			LearnFrom( Mirror, vLearnRate, vWinner, pUpdateToMe );
		const double vError = LearnFrom( Field, vLearnRate, vWinner, pUpdateToMe ); // Learn from Field twice
		vLearnRate *= 1.0 - abs( vError )/2;
	}
}


template<typename TBot>
void TrainBot( TBot& StudentBot, const CGameGenerator& GameGenerator, double vLearnRate, int nBatches, int nBatchSize, const CBot* pTestOpponent = nullptr, int nTestFreq = 1 )
{
	constexpr size_t LOCAL_THREADS = THREADS;
	if ( pTestOpponent )
		std::cout << "Start Training:\n";
	for ( int nBatch = 0; nBatch < nBatches; ++nBatch )
	{
		if ( pTestOpponent && nBatch % ((nTestFreq/nBatchSize)+1) == 0 )
			DrawNumberFancy( PlayMatch( StudentBot, *pTestOpponent, nTestFreq, false ) );
		std::vector<decltype(StudentBot._Layers)> Gradients( LOCAL_THREADS, 0.0 );
		const TBot& ConstStudentBot = StudentBot;
		auto TrainBatch = [&Gradients, &ConstStudentBot, vLearnRate = vLearnRate / nBatchSize]( int nThread, int N, CThreadSafeGameGenerator Generator )
		{
			for ( int i = 0; i < N; ++i )
			{
				const CGame Game = Generator.Generate();
				ConstStudentBot.LearnFrom( Game, vLearnRate, &Gradients[nThread] );
			}
		};
		if ( LOCAL_THREADS > 1 )
		{
			std::vector<std::thread> Threads;
			int nOnce = nBatch * nBatchSize;
			for ( int i = 0; i < LOCAL_THREADS; ++i )
			{
				int nForMe = nBatchSize / LOCAL_THREADS;
				if ( i < nBatchSize % LOCAL_THREADS )
					nForMe += 1;
				Threads.emplace_back( TrainBatch, i, nForMe, CThreadSafeGameGenerator( GameGenerator, nOnce ) );
				nOnce += nForMe;
			}
			for ( int i = 0; i < LOCAL_THREADS; ++i )
				Threads[i].join();
			for ( int i = 0; i < LOCAL_THREADS; ++i )
				StudentBot._Layers += Gradients[i];
		}
		else
		{
			TrainBatch( 0, nBatchSize, CThreadSafeGameGenerator( GameGenerator, nBatch * nBatchSize ) );
			StudentBot._Layers += Gradients[0];
		}
	}
}