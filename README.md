chess
=====
game.py plays chess in C++.  lichess.org/@/lobsterbot

Code is optimized for Arm Mac, but should compile on x86 and linux.

Training data for NNUE
===
wget https://database.nikonoel.fr/lichess_elite_2023-01.zip
unzip lichess_elite_2023-01.zip
See python notebooks in net/ for training

UCI
====

To use UCI interface on lichess, download https://github.com/lichess-bot-devs/lichess-bot

Use ./uciinterface as the executable in the lichess-bot config.yml
