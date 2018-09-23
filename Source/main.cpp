#include "bot.h"
#include "game_field.h"
#include "move.h"
#include "nn_bot.h"
#include "api.h"
#include "divide_and_conquer.h"
#include "input_data.h"
#include "fast_divide_and_conquer.h"

#include <array>
#include <utility>
#include <random>
#include <memory>
#include <cmath>
#include <iostream>
#include <thread>
#include <future>

double PlayMatch(const CBot& Bot1, const CBot& Bot2, int N, bool bPrint = false) // N = 20000 for variance < 1%
{
	auto PlayXGames = [&Bot1, &Bot2](int X, std::array<int,3>* pWins, double* pDrawScore, int nSwapParity)
	{
		for (int i = 0; i < X; ++i)
		{
			const bool bSwap = i % 2 == nSwapParity; // Change who moves first
			const auto Game = PlayGame( bSwap ? Bot2 : Bot1, bSwap ? Bot1 : Bot2 );
			const int nWinner = bSwap ? Game.GetWinner() : -Game.GetWinner();
			++(*pWins)[ nWinner + 1];
			if ( nWinner == 0 )
				*pDrawScore += SDominationInput<1>::Get( Game.GetLastField(), bSwap ? -1 : 1 )[0];
		}
	};

	if (N == 0)
		return 0.0;

	constexpr size_t LOCAL_THREADS = THREADS;

	std::array<int, 3> Wins = {};
	double vDrawScore = 0.0;
	if ( LOCAL_THREADS > 1 )
	{
		std::array<std::array<int, 3>, LOCAL_THREADS> PartialWins = {};
		std::array<double, LOCAL_THREADS> PartialDrawScores = {};
		std::vector<std::thread> Threads;
		for ( int i = 0; i < LOCAL_THREADS; ++i )
		{
			int nForMe = N / LOCAL_THREADS;
			if ( i < N % LOCAL_THREADS )
				nForMe += 1;
			Threads.emplace_back( PlayXGames, nForMe, &PartialWins[i], &PartialDrawScores[i], i % 2 );
		}
		for ( int i = 0; i < LOCAL_THREADS; ++i )
		{
			Threads[i].join();
			vDrawScore += PartialDrawScores[i];
			for ( int j = 0; j < 3; ++j )
				Wins[j] += PartialWins[i][j];
		}
	}
	else
	{
		PlayXGames( N, &Wins, &vDrawScore, 0 );
	}
	if ( bPrint )
	{
		std::cout << Wins[0] << " " << Wins[1] << " " << Wins[2] << std::endl;
		const double vAverageDrawScore = Wins[1] == 0 ? 0.0 : (vDrawScore / Wins[1]);
		std::cout << vAverageDrawScore << std::endl;
	}

	return double(Wins[0] - Wins[2]) / N;
}

template<size_t N>
auto CreateGreedyBot( int nSampleBirths = -1 )
{
	CNNBot<SNeuralNetwork<DENSE<1>, STanHActivation, SSimulatedInput<N>>> GreedyBot( nSampleBirths );
	GreedyBot._Layers._W.fill( 0.1 );
	GreedyBot._Layers._B = Mat11( 0.0 );
	return GreedyBot;
}
template<size_t N>
auto CreateGreedyBotDomination( int nSampleBirths = -1 )
{
	CNNBot<SNeuralNetwork<DENSE<1>, SLinearActivation, SDominationInput<N>>> GreedyBot( nSampleBirths );
	GreedyBot._Layers._W.fill( 1.0 );
	GreedyBot._Layers._B = Mat11( 0.0 );
	return GreedyBot;
}
template<size_t N>
auto CreateGreedyBotTanDomination( int nSampleBirths = -1 )
{
	CNNBot<SNeuralNetwork<DENSE<1>, STanHActivation, SDominationInput<N>>> GreedyBot( nSampleBirths );
	GreedyBot._Layers._W.fill( 1.0 );
	GreedyBot._Layers._B = Mat11( 0.0 );
	return GreedyBot;
}

template<typename TBot>
void TryTestBirths( const TBot& Bot, const std::vector<int>& TestBirths, int nGamesPerMatch )
{
	std::vector<TBot> Bots;
	std::vector<double> Score( TestBirths.size(), 0.0 );
	for ( int i = 0; i < TestBirths.size(); ++i )
	{
		Bots.push_back( Bot );
		Bots.back()._nSampleBirths = TestBirths[i];
	}
	for ( int i = 0; i < Bots.size(); ++i ) for ( int j = i+1; j < Bots.size(); ++j )
	{
		double vScore = PlayMatch( Bots[i], Bots[j], nGamesPerMatch ) / ( Bots.size() + 1 );
		Score[i] += vScore;
		Score[j] -= vScore;
	}
	for ( int i = 0; i < Score.size(); ++i )
	{
		std::cout << TestBirths[i] << ": " << Score[i] << std::endl;
	}
	std::cout << std::endl;
}

template<typename TBot>
void TrySimulationDepth( const TBot& Bot, int nFrom, int nTo )
{
	for ( int i = nFrom; i < nTo; ++i )
	{
		TBot DeepBot = Bot;
		DeepBot._nSimulationDepth = i;
		std::cout << PlayMatch( DeepBot, Bot, 200, true ) << std::endl;
	}
	std::cout << std::endl;
}
template<typename TBot>
void TrySeriousCandidates( const TBot& Bot, std::vector<int> Values )
{
	for ( int i = 0; i < Values.size(); ++i )
	{
		TBot DeepBot = Bot;
		DeepBot._nSeriousCandidates = Values[i];
		std::cout << PlayMatch( DeepBot, Bot, 500, true ) << std::endl;
	}
	std::cout << std::endl;
}

void TestConwayRule()
{
	for ( int i = 0; i < 10000; ++i )
	{
		auto Field = NewField();
		auto SlowNext = NextFieldSlow( Field );
		auto FastNext = NextFieldFast( Field );
		auto SuperFastNext = NextFieldSuperFast( Field );
		if ( SlowNext != FastNext || FastNext != SuperFastNext )
		{
			std::cout << "NOOOOOOOOOO!!!!" << std::endl;
			PrintField( Field );
			PrintField( SlowNext );
			PrintField( FastNext );
			PrintField( SuperFastNext );
			NextFieldSlow( Field );
		}
	}
	std::cout << "Conway OK!" << std::endl;
}

TMoveIdentifier BotChooseMove( const CBot& Bot, const CGame& Game )
{
	return Bot.ChooseMove( Game );
}

template<typename TPolicy>
void FixPolicy( TPolicy& Policy ) // Builds a decent heuristic policy without training
{
	Policy._B.fill( 0.0 );
	{
		auto& W = Policy._Prev._W;
		W = typename std::remove_reference<decltype(W)>::type();
		auto& B = Policy._Prev._B;
		B.fill( 0.0 );

		const int nRadius = 1;
		using EDepths = SMoveDeltaInput::EDepths;
		for ( int row = 0; row < HEIGHT; ++row )
		{
			for ( int col = 0; col < WIDTH; ++col )
			{
				for ( int depth = 0; depth < 3; ++depth )
				{
					for ( int in_row = row - nRadius; in_row <= row + nRadius; ++in_row )
					{
						if ( in_row < 0 || in_row >= HEIGHT ) continue;
						for ( int in_col = col - nRadius; in_col <= col + nRadius; ++in_col )
						{
							if ( in_col < 0 || in_col >= WIDTH ) continue;
							const int in_depth = (in_row == row && in_col == col) ? depth + 3 : depth;
							W._entries.emplace_back(
								depth * WIDTH*HEIGHT + row * WIDTH + col,
								in_depth * WIDTH*HEIGHT + in_row * WIDTH + in_col,
								0.1, false
							);
						}
					}
				}
			}
		}
		W._entries.emplace_back( 3*WIDTH*HEIGHT, 6*WIDTH*HEIGHT, 1.0, false );
	}
}

auto GetFixedPolicy()
{
	SNeuralNetwork<
		SPARSE<SCreateInternalizer>, STanHActivation,
		SPARSE<SCreateConv<3, 1>>, SLinearActivation,
		SMoveDeltaInput
	> Policy;

	FixPolicy( Policy );

	return Policy;
}

template<typename TDividerBot>
auto TryPredictMoves( const std::vector<CGame>& Games, const TDividerBot& DividerBot, bool bPrint )
{
	const double vDecay = 0.000001;
	double vLearnRate = 0.00001;


	SNeuralNetwork<
		SPARSE<SCreateInternalizer>, STanHActivation,
		SPARSE<SCreateConv<3, 1>>, SLinearActivation,
		SMoveDeltaInput
	> Policy;

	using TPolicy = decltype(Policy);
	const size_t nTrain = Games.size();
	double vErrorSum = 0.0;
	double vErrorWeight = 0.0;
	int nActive = 0;
	for ( int rep = 0; rep < 1; ++rep )
	{
		std::cout << "New Rep\n";
		vLearnRate *= 0.5;
		std::vector<std::pair<CGameField, TMoveIdentifier>> FieldsAndMoves;
		for ( int i = 0; i < nTrain; ++i )
		{
			const CGame& Game = Games[i];
			for ( int j = 0; j < Game.size() - 1; ++j )
			{
				FieldsAndMoves.push_back( Game._FieldsAndMoves[j] );
			}
		}
		KnuthShuffle( FieldsAndMoves );
		for ( int i = 0; i < FieldsAndMoves.size(); ++i )
		{
			const CGameField& Field = FieldsAndMoves[i].first;
			const double vPassScore = DividerBot.PredictOutcome( NextField( Field ), Field._player_to_move );

			SForwardProp<TPolicy> ForwardProp( Policy, Field, Field._player_to_move );

			SForwardProp< decltype(DividerBot._Layers) > PassForwardProp( DividerBot._Layers, NextField( Field ), Field._player_to_move );
			decltype(ForwardProp._Y) Facit;
			const double vImportance = DividerBot.FillPropagationData( Facit, Field );

			auto Fail = ToArray( ForwardProp._Y ) - ToArray( Facit );

			auto LossGradient = [&Facit, vImportance]( const auto& X )
			{
				return (X - ToArray( Facit ));
			};

			volatile bool bDebug = false;
			if ( [&bDebug]( bool b ) { return b || bDebug; }(bDebug) )
			{
				PrintField( Field );
				int nMaxFail = 0;
				double vMaxFail = Fail[0];
				for ( int i = 0; i < Fail.size(); ++i )
				{
					if ( abs( Fail[i] ) > vMaxFail )
					{
						nMaxFail = i;
						vMaxFail = abs( Fail[i] );
					}
				}
				std::cout << Field._player_to_move << " " << vPassScore << std::endl;
				std::cout << nMaxFail << " " << vMaxFail << std::endl;
			}

			SBackwardProp<TPolicy> BackwardProp( Policy, ForwardProp, LossGradient, vDecay );

			vErrorSum += BackwardProp.UpdateNeuralNet( vLearnRate );
			vLearnRate *= 0.99995;

			vErrorWeight += 1.0;
			if ( vErrorWeight >= 100 )
			{
				const double vMeanError = vErrorSum / vErrorWeight;
				if ( bPrint )
					DrawNumberFancy( vMeanError, false );
				vErrorWeight = 0;
				vErrorSum = 0.0;
			}
		}
	}

	return Policy;
}


int main()
{
#ifndef TESTING
	try
	{
		srand( 1 );
		auto Divider = MakeDivideAndConquer( CreateGreedyBotDomination<3>() );
		Divider._Layers._W._data = { { 1.260, 0.946, 0.610 } };
		Divider._Layers._W *= 1.0/(1.260+0.946+0.610);
		auto PolicyNet = GetFixedPolicy();
		auto FastDivider = MakeFast( PolicyNet, Divider );
		FastDivider._ParametersPerDepth =
		{
			{ 600, 600, 200, 30 },
			{ 100, 100, 20, 15 },
			{ 25, 25, 1, 1 }
		};
		CAPIInterface API( FastDivider );
		API.Play();
	}
	catch ( std::exception& e )
	{
		std::cerr << "Standard exception: " << e.what() << std::endl;
		std::cout << "Crashed";
		return 1;
	}
#else
	srand(unsigned(time(nullptr)));
	std::cout << std::fixed;
	std::cout.precision( 3 );

	constexpr size_t DEPTH = 3;
	const int nSampleBirths = 500;

	CRandomBot RandomBot;

	auto Games = ReadGames("SuperDivider");

	auto Divider = MakeDivideAndConquer( CreateGreedyBotTanDomination<3>() );
	Divider._Layers._W._data = { { 1.260, 0.946, 0.610 } };
	auto PolicyNet = TryPredictMoves( Games, Divider, true );
	auto FastDivider = MakeFast( PolicyNet, Divider );
	FastDivider._ParametersPerDepth =
	{
		{ 600, 600, 200, 30 },
		{ 100, 100, 20, 15 },
		{ 25, 25, 1, 1 },
	};

	auto OtherDivider = MakeDivideAndConquer( CreateGreedyBotDomination<3>() );
	OtherDivider._Layers._W._data = { { 1.260, 0.946, 0.610 } };
	auto OtherFastDivider = MakeFast( GetFixedPolicy(), OtherDivider );
	OtherFastDivider._ParametersPerDepth =
	{
		{ 600, 600, 200, 30 },
		{ 100, 100, 20, 15 },
		{ 25, 25, 1, 1 },
	};
//	OtherFastDivider._bTest = true;

	auto BadBot = FastDivider; // 0.32 is expected result
	BadBot._ParametersPerDepth =
	{
		{ 300, 300, 100, 40 },
		{ 25, 25, 5, 5 },
		{ 8, 8, 1, 1 }
	};

	std::cout << PlayMatch( OtherFastDivider, FastDivider, 24, true ) << std::endl;
	std::cout << PlayMatch( OtherFastDivider, FastDivider, 64, true ) << std::endl;
	std::cout << PlayMatch( OtherFastDivider, FastDivider, 128, true ) << std::endl;
	std::cout << PlayMatch( OtherFastDivider, FastDivider, 256, true ) << std::endl;
	std::cout << PlayMatch( OtherFastDivider, FastDivider, 512, true ) << std::endl;
	std::cout << PlayMatch( OtherFastDivider, FastDivider, 1024, true ) << std::endl;
	
	int N; std::cin >> N; // Pause
#endif
}