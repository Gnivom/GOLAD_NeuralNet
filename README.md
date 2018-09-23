# GOLAD_NeuralNet
- This is the bot I made for Riddles.io's "Game of Life and Death" competition. It ended up 2nd place!
- It has 0 dependencies and is 100% my own C++ code.
- It uses my home-made neural nets, that support (among other things) dense and convolutional layers, and a linear optimizer with custom loss functions.
- The field is represented with a bitmask, for super-fast game step simulation.
- Since I decided early on to make matrices know their size at compile time, there's lots of template meta programming. It gives some nice compile-time guarantees. I still regret it though...

Code is developed for C++11 gcc due to Riddles.io constraints, but I've been using VS2017 myself.

Finals results: https://starapple.riddles.io/competitions/game-of-life-and-death/discussions/discussion/b292afb5-2e65-4deb-a20a-47d4f210d52a/1

Pre-final ranking: (Select "Ranked" or "Lockdown" in the drop down) https://starapple.riddles.io/competitions/game-of-life-and-death/leaderboard

# How it works
Rules: https://docs.riddles.io/game-of-life-and-death/rules

In short, my bot uses a minimax search with alpha-beta pruning. It evaluates nodes based on one neural net and suggests moves based on another. To reduce the complexity of the search, we suggest partial moves only, which are then combined into full moves (one "Birth" move consists of one "Birth" part and two "Kill" parts).

## Watch it play

### Against average bot (ranked 47 of 104 in qualification)
https://starapple.riddles.io/competitions/game-of-life-and-death/matches/ddfd1c37-d625-48f1-bd3d-9fdd8087629a
### Against bot #3
https://starapple.riddles.io/competitions/game-of-life-and-death/leaderboard
### Against bot #1
Of ~500 games in the finals (48 of which against bot #1), this is the only one I lost :)
https://starapple.riddles.io/competitions/game-of-life-and-death/matches/6a0b02ff-e2da-487d-9d32-498ee933c08f
### All finals games:
https://starapple.riddles.io/competitions/game-of-life-and-death/match-log/finals/1
