#include "evaluate.hh"

unsigned int distance_from_center(int rank, int file);
unsigned int diagonal_moves(int rank, int file);


template <typename T>
T max(T a, T b)
{
	if (a < b) {
		return b;
	} else {
		return a;
	}
}

unsigned int distance_from_center(int rank, int file)
{
	int absrank = abs(3 - rank);
	int absfile = abs(3 - file);
	int diag = max(absrank, absfile);
	return diag + max(0, absrank - diag) + max(0, absfile - diag);
}

unsigned int diagonal_moves(int rank, int file)
{
	return 16 - abs(rank - file) - abs(7 - file - rank);
}

int SimpleEvaluation::evaluate(const Board &b) const {
	// piece count scores
	int qct = 0, bct = 0, rct = 0, nct = 0, pct = 0, rpct = 0;
	// pawn structure scores
	char pawns[16];
	memset(pawns, 0, sizeof(pawns));
	int ppawn = 0, isopawn = 0, dblpawn = 0;
	// piece position scores
	int nscore = 0, bscore = 0, kscore = 0, rhopenfile = 0, rfopenfile = 0,  qscore = 0;

	for (int rank = 0; rank < 8; rank++) {
		for (int file = 0; file < 8; file++) {
			piece_t piece = b.get_piece(make_board_pos(rank, file));
			if (piece & PIECE_MASK) {
				int accum = 1;
				int colorindex = 0;
				if (piece & BlackMask) {
					accum = -1;
					colorindex = 1;
				}
				switch (piece & PIECE_MASK) {
				case QUEEN: 
					qct += accum;
					qscore += accum * diagonal_moves(rank, file);
					break;
				case BISHOP: 
					bct += accum; 
					bscore += accum * diagonal_moves(rank, file);
					break;
				case ROOK: 
					rct += accum; 
					if (colorindex == 1 && pawns[file+8] == 0) {
						rhopenfile--;
						if (pawns[file] == 0) {
							rfopenfile--;
						}
					} else if (colorindex == 0) {
						bool whitepawn = false, blackpawn = false;
						for (int rr = rank + 1; rr < 7; rr++) {
							piece_t candpawn = b.get_piece(make_board_pos(rr, file));
							if (candpawn == PAWN) {
								whitepawn = true;
								break;
							} else if (candpawn == (PAWN | BlackMask)) {
								blackpawn = true;
							}
						}
						if (whitepawn == false) {
							rhopenfile++;
							if (blackpawn == false) {
								rfopenfile++;
							}
						}
					}
					break;
				case KNIGHT: 
					nct += accum; 
					nscore += distance_from_center(rank, file) * accum;
					break;
				case KING:
					kscore += distance_from_center(rank, file) * accum;
					break;
				case PAWN: 
					pct += accum; 
					if (file == 0 || file == 7) {
						rpct += accum;
					}
					if (pawns[colorindex * 8 + file] != 0) {
						dblpawn += accum;
					}
					if (colorindex == 0 || pawns[colorindex * 8 + file] == 0) {
						pawns[colorindex * 8 + file] = rank; 
					}
					break;
				}
			}
		}
	}
	for (int i = 0; i < 8; i++) {
		if (pawns[i] != 0 && (i == 0 || pawns[i - 1] == 0) && (i == 7 || pawns[i+1] == 0)) {
			isopawn += 1;
		}
		if (pawns[i+8] != 0 && (i == 0 || pawns[i+7] == 0) && (i == 7 || pawns[i+9] == 0)) {
			isopawn -= 1;
		}
		if (pawns[i] != 0 && pawns[i] > pawns[i+8] && (i == 0 || pawns[i] >= pawns[i+7]) && (i == 7 || pawns[i] >= pawns[i+9])) {
			ppawn += 1;
		}
		if (pawns[i+8] != 0 && (pawns[i] == 0 || pawns[i] > pawns[i+8]) && (i == 0 || pawns[i-1] == 0 || pawns[i-1] >= pawns[i+8]) && (i == 7 || pawns[i+1] == 0 || pawns[i+1] >= pawns[i+8])) {
			ppawn -= 1;
		}
	}
	return qct*100 + rct*48 + bct*11 + nct*47 + pct*21 - rpct*2 + ppawn*10 - isopawn*3 - dblpawn*4 - nscore*4 + bscore*3 - kscore + rhopenfile*9 + rfopenfile*14 + qscore*1;
}

