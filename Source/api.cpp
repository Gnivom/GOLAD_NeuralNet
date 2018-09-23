#include "api.h"
#include "bot.h"
#include "game_field.h"
#include "move.h"

#include <iostream>
#include <string>
#include <cassert>

using namespace std;

void MyAssert( bool bShouldBeTrue, const string& message )
{
	if ( !bShouldBeTrue )
	{
		cerr << "ASSERT FAIL!!:\n" << message << std::endl;
	}
//	assert( bShouldBeTrue );
}

class CParser
{
public:
	string _Key;

	CParser( string Key )
		: _Key( std::move( Key ) )
	{}
	virtual void ParseAndAct( const vector<string>& words, size_t nUsedWords ) = 0;
};

class CEndParser final : public CParser
{
private:
	std::function<void( const vector<string>&, size_t )> _functor;
public:
	CEndParser( string Key, decltype(_functor) && functor )
		: CParser( std::move( Key ) )
		, _functor( std::move( functor ) )
	{}
	void ParseAndAct( const vector<string>& words, size_t nUsedWords ) override
	{
		_functor( words, nUsedWords );
	}
};

class CIntermediateParser final : public CParser
{
private:
	vector<std::unique_ptr<CParser>> _subParsers;
public:
	template<typename... Ts>
	CIntermediateParser( string MyKey, std::tuple<Ts...>&& ts );
	void ParseAndAct( const vector<string>& words, size_t nUsedWords ) override
	{
		for ( const auto& pSub : _subParsers )
		{
			if ( pSub->_Key == words[nUsedWords] )
			{
				pSub->ParseAndAct( words, nUsedWords + 1 );
				return;
			}
		}
		MyAssert( false, string( "Unknown word " ) + words[nUsedWords] );
	}
};

std::unique_ptr<CParser> MakeNewParser( string Key, std::function<void( const vector<string>&, size_t )>&& functor )
{
	return std::make_unique<CEndParser>( std::move( Key ), std::move( functor ) );
}

template<typename... Ts>
std::unique_ptr<CParser> MakeNewParser( string MyKey, std::tuple<Ts...>&& ts )
{
	return std::make_unique<CIntermediateParser>( std::move( MyKey ), std::move( ts ) );
}

template<size_t I, typename... Ts>
struct SubParsers
{
	static_assert(I % 2 == 0, "Every other parser entry must be a key");
	static vector<std::unique_ptr<CParser>> Create( std::tuple<Ts...>&& ts )
	{
		auto pNewParser = MakeNewParser( std::move( std::get<I-2>( ts ) ), std::move( std::get<I-1>( ts ) ) );
		vector<std::unique_ptr<CParser>> Ret = SubParsers<I - 2, Ts...>::Create( std::move( ts ) );
		Ret.push_back( std::move( pNewParser ) );
		return Ret;
	}
};
template<typename... Ts>
struct SubParsers<0, Ts...>
{
	static vector<std::unique_ptr<CParser>> Create( std::tuple<Ts...>&& ts )
	{
		return vector<std::unique_ptr<CParser>>();
	}
};

template<typename... Ts>
CIntermediateParser::CIntermediateParser( string MyKey, std::tuple<Ts...>&& ts )
	: CParser( std::move( MyKey ) )
	, _subParsers( SubParsers< sizeof...(Ts), Ts... >::Create( std::move( ts ) ) )
{
}

vector<string> SplitString( const string& str, char ch )
{
	vector<string> output;
	output.emplace_back();
	for ( size_t i = 0; i < str.size(); ++i )
	{
		if ( str[i] == ch )
			output.emplace_back();
		else
			output.back().push_back( str[i] );
	}
	return output;
}


void CAPIInterface::Play()
{
	CIntermediateParser FullParser
	(
#define CAT std::make_tuple
#define FUNC(X) std::function<void( const vector<string>&, size_t )>( [this]( const vector<string>& words, size_t i ) { MyAssert( i < words.size(), "Too few words"  ); X } )
		"", // No name needed
		CAT(
			"settings",
			CAT(
				"your_botid", FUNC( _nMyPlyerID = words[i] == "0" ? -1 : 1; ),
				"field_height", FUNC( MyAssert( Settings::HEIGHT == std::stoi( words[i] ), "Wrong height" ); ),
				"field_width", FUNC( MyAssert( Settings::WIDTH == std::stoi( words[i] ), "Wrong width" ); ),
				"player_names", FUNC( ;),
				"your_bot", FUNC( ;),
				"timebank", FUNC( MyAssert( Settings::TIMEBANK == std::stoi( words[i] ), "Wrong timebank" ); ),
				"time_per_move", FUNC( MyAssert( Settings::TIME_PER_MOVE == std::stoi( words[i] ), "Wrong time_per_move" ); ),
				"max_rounds", FUNC( MyAssert( Settings::MAX_ROUNDS == stoi( words[i] )*2, "Wrong max_rounds" ); )
			),
			"update",
			CAT(
				"game",
				CAT(
					"field", FUNC( _Game.PushFromAPI( words[i] ); ),
					"round", FUNC( cerr << "\n----------Round " << std::stoi( words[i] ) << "----------" << std::endl; )
				),
				"player0",
				CAT(
					"living_cells", FUNC( ;),
					"move", FUNC( ;)
				),
				"player1",
				CAT(
					"living_cells", FUNC( ;),
					"move", FUNC( ;)
				)
			),
			"action",
			CAT(
				"move", FUNC( OutputBestMove( std::stoi( words[i] ) ); )
			),
			"Output", FUNC( ;) // For debugging
		)
#undef CAT
#undef FUNC
	);
	string inputline;
	while ( std::getline( cin, inputline ) )
	{
		vector<string> words = SplitString( inputline, ' ' );
		FullParser.ParseAndAct( words, 0 );
	}

	cerr << "Done with main loop" << std::endl;
}

#include <ctime>

void CAPIInterface::OutputBestMove( int nTimeLeft )
{
	std::cerr << "Asked to make move: time = " << double( std::clock() ) / CLOCKS_PER_SEC << std::endl;
	CGameField& LastField = _Game.GetLastField();
	LastField._player_to_move = _nMyPlyerID;
	LastField._time = int( _Game.size() ) - 1;
	PrintField( LastField );

	{ // Check time
		static int nLastTimeLeft = TIMEBANK;
		int nTimeUsed = nLastTimeLeft - nTimeLeft + TIME_PER_MOVE;
		if ( nTimeLeft == TIMEBANK )
			nTimeUsed -= TIME_PER_MOVE / 2;
		nLastTimeLeft = nTimeLeft;

		int nRemainingRounds = int( MAX_ROUNDS - _Game.size() + 1 )/2;
		if ( nRemainingRounds == 0 )
			std::cerr << "ASKED TO PLAY FOR TOO LONG!" << std::endl;
		if ( nRemainingRounds < 100 )
		{
			if ( nRemainingRounds > 90 )
				nRemainingRounds = 90;
			constexpr int RESERVE_TIME = 5*TIME_PER_MOVE;
			const int nRemainingTime = nTimeLeft - RESERVE_TIME + TIME_PER_MOVE * ((1+nRemainingRounds) / 2);

			const double vFactor = double( nRemainingTime ) / (nRemainingRounds * nTimeUsed);

			_Bot.NotifyTimeFactor( std::max( vFactor, 0.1 ) );
		}
	}

	TMoveIdentifier PlayedMove = _Bot.ChooseMove( _Game );
	_Game.MakeMove( PlayedMove );
	std::cerr << "Chose move: time = " << double( std::clock() ) / CLOCKS_PER_SEC << std::endl;
	std::cout << GetMoveName( PlayedMove ) << std::endl;
}
