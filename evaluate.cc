#include "evaluate.hh"
#include "move.hh"
#include <algorithm>
#include <iostream>

unsigned distance_from_center(int rank, int file);
unsigned diagonal_moves(int rank, int file);
void count_pieces(Fenboard &b, int file, int &black, int &white, piece_t piece, int rank_to_exclude = -1);

const int QCT_SCORE = 900;
const int RCT_SCORE = 500;
const int BCT_SCORE = 300;
const int NCT_SCORE = 300;
const int PCT_SCORE = 100;
const int RPCT_SCORE =  -2;
const int PPAWN_SCORE = 10;
const int ISOPAWN_SCORE = -3;
const int DBLPAWN_SCORE = -4;
const int NSCORE_SCORE = 4;
const int BSCORE_SCORE = 3;
const int KSCORE_SCORE = 1;
const int RHOPENFILE_SCORE = 9;
const int RFOPENFILE_SCORE = 14;
const int QSCORE_SCORE = 1;

const int ABS_NUM_PIECES = 0;
const int CTRL_SQUARES_CT = 0;
const int CENTRAL_SQUARES_CTL_CT = 0;



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

int SimpleEvaluation::compute_scores(int qct, int bct, int rct, int nct, int pct, int rpct, int ppawn,
        int isopawn, int dblpawn, int nscore, int bscore, int kscore, int rhopenfile, int rfopenfile, int qscore) const
{
    return qct*QCT_SCORE + rct*RCT_SCORE + bct*BCT_SCORE + nct*NCT_SCORE + pct*PCT_SCORE + rpct*RPCT_SCORE +
        ppawn*PPAWN_SCORE + isopawn*ISOPAWN_SCORE + dblpawn*DBLPAWN_SCORE + nscore* NSCORE_SCORE +
        bscore*BSCORE_SCORE + kscore*KSCORE_SCORE + rhopenfile*RHOPENFILE_SCORE +
        rfopenfile*RFOPENFILE_SCORE + qscore+QSCORE_SCORE;
}

void count_pieces(Fenboard &b, int file, int &black, int &white, piece_t piece, int rank_to_exclude)
{
    static const uint64_t file_masks[] = {
        0x0101010101010101,
        0x0202020202020202,
        0x0404040404040404,
        0x0808080808080808,
        0x1010101010101010,
        0x2020202020202020,
        0x4040404040404040,
        0x8080808080808080
    };
    static const uint64_t rank_masks[] = {
        0x00000000000000ff,
        0x000000000000ff00,
        0x0000000000ff0000,
        0x00000000ff000000,
        0x000000ff00000000,
        0x0000ff0000000000,
        0x00ff000000000000,
        0xff00000000000000
    };
    uint64_t rank_mask = ~0;
    if (rank_to_exclude >= 0) {
        rank_mask = ~rank_masks[rank_to_exclude];
    }
    black = count_bits(b.get_bitmask(Black, piece) & file_masks[file] & rank_mask);
    white = count_bits(b.get_bitmask(White, piece) & file_masks[file] & rank_mask);
}


// computes delta score for adding piece at bp
int SimpleEvaluation::delta_evaluate_piece(Fenboard &b, piece_t piece, int rank, int file) const
{
    int net_score = 0;

    Color color = get_color(piece);
    int colormultiplier = 1;
    if (color == Black) {
        colormultiplier = -1;
    }

    switch (piece & PIECE_MASK) {
        case bb_queen: net_score += colormultiplier * QCT_SCORE;
            net_score += colormultiplier * diagonal_moves(rank, file) * QSCORE_SCORE;
            break;
        case bb_bishop: net_score += colormultiplier * BCT_SCORE;
            net_score += colormultiplier * diagonal_moves(rank, file) * BSCORE_SCORE;
            break;
        case bb_rook: net_score += colormultiplier * RCT_SCORE;
            {
                int blackpawns = 0, whitepawns = 0;
                count_pieces(b, file, blackpawns, whitepawns, bb_pawn, -1);
                if ((color == Black && blackpawns == 0) || (color == White && whitepawns == 0)) {
                    net_score += colormultiplier * RHOPENFILE_SCORE;
                }
                if (blackpawns == 0 && whitepawns == 0) {
                    net_score += colormultiplier * RFOPENFILE_SCORE;
                }
            }
            break;
        case bb_knight: net_score += colormultiplier * NCT_SCORE;
            net_score += colormultiplier * distance_from_center(rank, file) * NSCORE_SCORE;
            break;
        case bb_pawn: net_score += colormultiplier * PCT_SCORE;
            {
                if (file == 0 || file == 7) {
                    net_score += colormultiplier + RPCT_SCORE;
                }
                // count doubled pawns
                int blackpawns = 0, whitepawns = 0;
                count_pieces(b, file, blackpawns, whitepawns, bb_pawn, rank);
                if ((color == White && whitepawns == 1) || (color == Black && blackpawns == 1)) {
                    net_score += colormultiplier * DBLPAWN_SCORE;
                }
                // count open files
                int blackrooks = 0, whiterooks = 0;
                count_pieces(b, file, blackrooks, whiterooks, bb_rook, -1);
                if (color == White && whitepawns == 0) {
                    if (whiterooks > 0) {
                        net_score -= whiterooks * RHOPENFILE_SCORE;
                        if (blackpawns == 0) {
                            net_score -= whiterooks * RFOPENFILE_SCORE;
                        }
                    }
                    if (blackrooks > 0) {
                        if (blackpawns == 0) {
                            net_score += RFOPENFILE_SCORE * blackrooks;
                        }

                    }
                } else if (color == Black && blackpawns == 0) {
                    if (blackrooks > 0) {
                        net_score += RHOPENFILE_SCORE * blackrooks;
                        if (whitepawns == 0) {
                            net_score -= RFOPENFILE_SCORE * blackrooks;
                        }
                    }
                    if (whiterooks > 0) {
                        net_score += RFOPENFILE_SCORE * whiterooks;
                    }
                }
                // skip computation of isopawn and ppawn because it's complicated
            }
            break;
        case bb_king:
            net_score += colormultiplier * distance_from_center(rank, file) * KSCORE_SCORE;
            break;
    }
    return net_score;
}

int SimpleEvaluation::delta_evaluate(Fenboard &b, move_t move, int previous_score) const
{
    char dest_rank = get_board_rank(get_dest_pos(move));
    char dest_file = get_board_file(get_dest_pos(move));
    char src_rank = get_board_rank(get_source_pos(move));
    char src_file = get_board_file(get_source_pos(move));
    piece_t active_piece = b.get_piece(src_rank, src_file);
    piece_t captured_piece = get_captured_piece(move, White);

    return previous_score + delta_evaluate_piece(b, active_piece, dest_rank, dest_file) - delta_evaluate_piece(b, active_piece, src_rank, src_file)
        - delta_evaluate_piece(b, captured_piece, dest_rank, dest_file);
}

int SimpleEvaluation::evaluate(const Fenboard &b) const {
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
                case bb_queen:
                    qct += accum;
                    qscore += accum * diagonal_moves(rank, file);
                    break;
                case bb_bishop:
                    bct += accum;
                    bscore += accum * diagonal_moves(rank, file);
                    break;
                case bb_rook:
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
                            if (candpawn == bb_pawn) {
                                whitepawn = true;
                                break;
                            } else if (candpawn == (bb_pawn | BlackMask)) {
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
                case bb_knight:
                    nct += accum;
                    nscore += distance_from_center(rank, file) * accum;
                    break;
                case bb_king:
                    kscore += distance_from_center(rank, file) * accum;
                    break;
                case bb_pawn:
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

void SimpleBitboardEvaluation::get_features(const Fenboard &b, int *features) const {
    int piece_counts[bb_king+1];
    int piece_scores[bb_king+1];
    int white_pawns[8];
    int black_pawns[8];
    memset(white_pawns, 0, sizeof(white_pawns));
    memset(black_pawns, 0, sizeof(black_pawns));

    int isopawn = 0;
    int ppawn = 0;
    int dblpawn = 0;
    int rhopenfile = 0;
    int rfopenfile = 0;
    int start_pos = 0;

    while ((start_pos = get_low_bit(b.piece_bitmasks[bb_pawn], start_pos)) > -1) {
        if (white_pawns[start_pos % 8] > 0) {
            dblpawn++;
        } else {
            white_pawns[start_pos % 8] = start_pos / 8;
        }
        start_pos ++;
    }
    start_pos = 0;
    while ((start_pos = get_low_bit(b.piece_bitmasks[bb_pawn + bb_king + 1], start_pos)) > -1) {
        if (black_pawns[start_pos % 8] > 0) {
            dblpawn--;
        }
        black_pawns[start_pos % 8] = start_pos / 8;
        start_pos ++;
    }

    piece_counts[bb_pawn] = count_bits(b.piece_bitmasks[bb_pawn]) - count_bits(b.piece_bitmasks[bb_pawn + bb_king + 1]);

    for (int i = bb_knight; i <= bb_king; i++) {
        piece_counts[i] = count_bits(b.piece_bitmasks[i]) - count_bits(b.piece_bitmasks[i + bb_king + 1]);
        // remove expensive features with minor impact

        for (int color = 0; color <= 1; color++) {
            int multiplier = color == White ? 1 : -1;
            int start_pos = 0;
            piece_scores[i] = 0;
            /*
            uint64_t dest_squares = 0;
            do {
                switch(i) {
                    case bb_king: case bb_knight:
                        b.next_pnk_move(static_cast<Color>(color), i, start_pos, dest_squares, FL_CAPTURES | FL_EMPTY, false, false);
                        break;
                    case bb_queen: case bb_bishop: case bb_rook:
                        b.next_piece_slide(static_cast<Color>(color), i, start_pos, dest_squares, FL_CAPTURES | FL_EMPTY);
                        break;
                }
                if (start_pos < 0 || dest_squares == 0) {
                    break;
                }
                piece_scores[i] += multiplier * count_bits(dest_squares);
                start_pos++;
            } while (start_pos >= 0);
            */
        }

    }

    while ((start_pos = get_low_bit(b.piece_bitmasks[bb_rook], start_pos)) > -1) {
        if (white_pawns[start_pos%8] == 0) {
            rhopenfile++;
            if (black_pawns[start_pos%8] == 0) {
                rfopenfile++;
            }
        }
        start_pos ++;
    }
    start_pos = 0;
    while ((start_pos = get_low_bit(b.piece_bitmasks[bb_rook + bb_king + 1], start_pos)) > -1) {
        if (black_pawns[start_pos%8] == 0) {
            rhopenfile--;
            if (white_pawns[start_pos%8] == 0) {
                rfopenfile--;
            }
        }
        start_pos ++;
    }


    for (int i = 0; i < 8; i++) {
        if (white_pawns[i] > 0 && (i == 0 || white_pawns[i-1] == 0) && (i == 7 || white_pawns[i+1] == 0)) {
            isopawn++;
        }
        if (white_pawns[i] > black_pawns[i] && (i == 0 || white_pawns[i] > black_pawns[i-1]) && (i == 7 || white_pawns[i] > black_pawns[i+1])) {
            ppawn++;
        }

        if (black_pawns[i] > 0 && (i == 0 || black_pawns[i-1] == 0) && (i == 7 || black_pawns[i+1] == 0)) {
            isopawn--;
        }
        if (black_pawns[i] > 0 && black_pawns[i] < white_pawns[i] && (i == 0 || white_pawns[i-1] > black_pawns[i]) && (i == 7 || white_pawns[i+1] > black_pawns[i])) {
            ppawn++;
        }
    }

    const uint64_t RP_MASK = 0x8181818181818181;
    int rpct = count_bits(b.piece_bitmasks[bb_pawn] & RP_MASK) - count_bits(b.piece_bitmasks[bb_pawn + bb_king + 1] & RP_MASK);
    features[0] = piece_counts[bb_queen];
    features[1] = piece_counts[bb_rook];
    features[2] = piece_counts[bb_bishop];
    features[3] = piece_counts[bb_knight];
    features[4] = piece_counts[bb_pawn];
    features[5] = rpct;
    features[6] = ppawn;
    features[7] = isopawn;
    features[8] = dblpawn;
    features[9] = piece_scores[bb_knight];
    features[10] = piece_scores[bb_bishop];
    features[11] = piece_scores[bb_king];
    features[12] = rhopenfile;
    features[13] = rfopenfile;
    features[14] = piece_scores[bb_queen];
    features[15] = 0;
    features[16] = 0;
    features[17] = 0;
}

bool SimpleEvaluation::endgame(const Fenboard &b, int &eval) const {
    int pieces = count_bits(b.piece_bitmasks[bb_all] | b.piece_bitmasks[bb_all + bb_king + 1]);

    if (pieces < 4) {
        // K/K is draw
        if (pieces == 2) {
            eval = 0;
            return true;
        }
        // KB/K and KN/K are draws
        else if (count_bits(b.piece_bitmasks[bb_knight] | b.piece_bitmasks[bb_knight + bb_king + 1] | b.piece_bitmasks[bb_bishop] | b.piece_bitmasks[bb_bishop + bb_king + 1]) >= 1) {
            eval = 0;
            return true;
        }
    }
    return false;
}

static int weights[] = {QCT_SCORE, RCT_SCORE, BCT_SCORE, NCT_SCORE, PCT_SCORE,
    RPCT_SCORE, PPAWN_SCORE, ISOPAWN_SCORE, DBLPAWN_SCORE, NSCORE_SCORE,
    BSCORE_SCORE, KSCORE_SCORE, RHOPENFILE_SCORE, RFOPENFILE_SCORE, QSCORE_SCORE,
    ABS_NUM_PIECES, CTRL_SQUARES_CT, CENTRAL_SQUARES_CTL_CT};

int SimpleBitboardEvaluation::evaluate(const Fenboard &b) const {
    int features[NUM_FEATURES];

    get_features(b, features);
    int result = 0;
    for (int i = 0; i < NUM_FEATURES; i++) {
        result += features[i] * weights[i];
    }

    return result;
}
