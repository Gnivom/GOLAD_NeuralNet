# GOLAD_NeuralNet
- This is the bot I made for Riddles.io's "Game of Life and Death" competition. It ended up 2nd place!
- It has 0 dependencies and is 100% my own C++ code.
- It uses my home-made neural nets, that support (among other things) dense and convolutional layers, and a linear optimizer with custom loss functions.
- The field is represented with a bitmask, for super-fast game step simulation.
- Since I decided early on to make matrices know their size at compile time, there's lots of template meta programming. It gives some nice compile-time guarantees. I still regret it though...

Code is developed for C++11 gcc due to Riddles.io constraints, but I've been using VS2017 myself.

Finals results: https://starapple.riddles.io/competitions/game-of-life-and-death/discussions/discussion/b292afb5-2e65-4deb-a20a-47d4f210d52a/1

Pre-final ranking: (Select "Ranked" or "Lockdown" in the drop down) https://starapple.riddles.io/competitions/game-of-life-and-death/leaderboard
