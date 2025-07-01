chess
=====
This is a chess engine you can challenge at https://lichess.org/@/lobsterbot


Implementation
====
The code uses a magic bitboard, with moves pre-computed using C++ template prepocessing.

The search function is using MTD(f) with iterative deepening for time management.  There's a
naive move sorting algorithm: checks, then captures, then other moves.  I implemented a
transposition table as well.  The code is still single-threaded because multi-threading is hard.  I
tried to put in some killer-move heuristic but it didn't perform very well, I should probably
investigate that.

The evaluation function uses a somewhat simplified NNUE network, trained over stockfish
evaluation scores on lichess games from 2024.  I had to write some C++ pgn.zst parsing code so that
the pgn parser would run faster than the actual training.  I'm using keras to build weights.

On the evaluation side I hand-coded a linear algebra library using arm/neon simd instructions.


Operation
====
Lobsterbot runs on a Silicon Macbook on my desk.


Training data for NNUE
====
Download pgn.zst from https://database.lichess.org/

See python notebooks in net/ for training.

export_nnue.py builds a C++ file from a keras model.


Opening book
====
See code in theory/ -- I tried to do some MCTS over pgn files but I'm not sure if that does better
than just stealing the opening book from my favorite lichess youtube streamers.  Copy the .bin
produced by makebook into the lichess config.


Running on lichess
====
To use UCI interface on lichess, download https://github.com/lichess-bot-devs/lichess-bot

Use ./uciinterface as the executable in the lichess-bot config.yml
