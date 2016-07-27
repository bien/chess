#include "evaluate.hh"
#include <algorithm>

unsigned distance_from_center(int rank, int file);
unsigned diagonal_moves(int rank, int file);
void count_pieces(FenBoard &b, int file, int &black, int &white, piece_t piece, int rank_to_exclude = -1);

template <typename T>
T max(T a, T b)
{
	if (a < b) {
		return b;
	} else {
		return a;
	}
}

unsigned distance_from_center(int rank, int file)
{
	int absrank = abs(3 - rank);
	int absfile = abs(3 - file);
	int diag = max(absrank, absfile);
	return diag + max(0, absrank - diag) + max(0, absfile - diag);
}

unsigned diagonal_moves(int rank, int file)
{
	return 16 - abs(rank - file) - abs(7 - file - rank);
}

double SimpleEvaluation::compute_scores(int qct, int bct, int rct, int nct, int pct, int rpct, int ppawn,
		int isopawn, int dblpawn, int nscore, int bscore, int kscore, int rhopenfile, int rfopenfile, int qscore) const
{
	return qct*100 + rct*48 + bct*11 + nct*47 + pct*21 - rpct*2 + ppawn*10 - isopawn*3 - dblpawn*4 - nscore*4 + bscore*3 - kscore + rhopenfile*9 + rfopenfile*14 + qscore*1;
}

void count_pieces(FenBoard &b, int file, int &black, int &white, piece_t piece, int rank_to_exclude)
{
	black = 0;
	white = 0;
	for (int rank = 1; rank < 7; rank++) {
		if (rank == rank_to_exclude) {
			continue;
		}
		piece_t pawn_cand = b.get_piece(rank, file);
		if ((pawn_cand & PIECE_MASK) == piece) {
			if ((piece & BlackMask) != 0) {
				black++;
			} else {
				white++;
			}
		}
	}
}

// computes delta score for adding piece at bp
int SimpleEvaluation::delta_evaluate_piece(FenBoard &b, piece_t piece, int rank, int file) const
{
	// piece count scores
	int qct = 0, bct = 0, rct = 0, nct = 0, pct = 0, rpct = 0;
	// pawn structure scores
	int ppawn = 0, isopawn = 0, dblpawn = 0;
	// piece position scores
	int nscore = 0, bscore = 0, kscore = 0, rhopenfile = 0, rfopenfile = 0,  qscore = 0;

	Color color = get_color(piece);
	int colormultiplier = 1;
	if (color == Black) {
		colormultiplier = -1;
	}

	switch (piece & PIECE_MASK) {
		case QUEEN: qct += colormultiplier;
			qscore += colormultiplier * diagonal_moves(rank, file);
			break;
		case BISHOP: bct += colormultiplier;
			bscore += colormultiplier * diagonal_moves(rank, file);
			break;
		case ROOK: rct += colormultiplier;
			{
				int blackpawns = 0, whitepawns = 0;
				count_pieces(b, file, blackpawns, whitepawns, PAWN, -1);
				if ((color == Black && blackpawns == 0) || (color == White && whitepawns == 0)) {
					rhopenfile += colormultiplier;
				}
				if (blackpawns == 0 && whitepawns == 0) {
					rfopenfile += colormultiplier;
				}
			}
			break;
		case KNIGHT: nct += colormultiplier;
			nscore += colormultiplier * distance_from_center(rank, file);
			break;
		case PAWN: pct += colormultiplier;
			{
				if (file == 0 || file == 7) {
					rpct += colormultiplier;
				}
				// count doubled pawns
				int blackpawns = 0, whitepawns = 0;
				count_pieces(b, file, blackpawns, whitepawns, PAWN, rank);
				if ((color == White && whitepawns == 1) || (color == Black && blackpawns == 1)) {
					dblpawn += colormultiplier;
				}
				// count open files
				int blackrooks = 0, whiterooks = 0;
				count_pieces(b, file, blackrooks, whiterooks, ROOK, -1);
				if (color == White && whitepawns == 0) {
					if (whiterooks > 0) {
						rhopenfile -= whiterooks;
						if (blackpawns == 0) {
							rfopenfile -= whiterooks;
						}
					}
					if (blackrooks > 0) {
						if (blackpawns == 0) {
							rfopenfile += blackrooks;
						}

					}
				} else if (color == Black && blackpawns == 0) {
					if (blackrooks > 0) {
						rhopenfile += blackrooks;
						if (whitepawns == 0) {
							rfopenfile -= blackrooks;
						}
					}
					if (whiterooks > 0) {
						rfopenfile += whiterooks;
					}
				}
				// skip computation of isopawn and ppawn because it's complicated
			}
			break;
		case KING:
			kscore += colormultiplier * distance_from_center(rank, file);
			break;
	}
	return compute_scores(qct, bct, rct, nct, pct, rpct, ppawn, isopawn, dblpawn, nscore, bscore, kscore, rhopenfile, rfopenfile, qscore);
}

int SimpleEvaluation::delta_evaluate(FenBoard &b, move_t move, int previous_score) const
{
	unsigned char dest_rank, dest_file, src_rank, src_file;
	b.get_source(move, src_rank, src_file);
	b.get_dest(move, dest_rank, dest_file);
	piece_t active_piece = b.get_piece(src_rank, src_file);
	piece_t captured_piece = b.get_piece(dest_rank, dest_file);

	return previous_score + delta_evaluate_piece(b, active_piece, dest_rank, dest_file) - delta_evaluate_piece(b, active_piece, src_rank, src_file)
		- delta_evaluate_piece(b, captured_piece, dest_rank, dest_file);
}

int SimpleEvaluation::evaluate(const FenBoard &b) const {
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
			piece_t piece = b.get_piece(rank, file);
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
							piece_t candpawn = b.get_piece(rr, file);
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
	return compute_scores(qct, bct, rct, nct, pct, rpct, ppawn, isopawn, dblpawn, nscore, bscore, kscore, rhopenfile, rfopenfile, qscore);
}
