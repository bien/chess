chess
=====
game.py plays chess in python.  It's super slow so I started working on a c++ version: run "make test".  It won't play you in a game though.

To download training data, run "data.sh".

The tests expect a file called "polgar - fixed.pgn" which is an unzipped version of "polgar_5334_fixed.7z", which can be found here: http://www.chess.am/pgn/


Latest test results
====
 ...
 Puzzles solved: 3544/3600 using 17869580 nodes at 125356 nodes/sec or 25.2543 puzzles/sec
 ...
 Puzzles solved: 3807/4066 using 50362439 nodes at 125046 nodes/sec or 10.0955 puzzles/sec

11/2/14:
Puzzles solved: 3362/3400 using 8752725 nodes at 160208 nodes/sec or 62.2328 puzzles/sec
