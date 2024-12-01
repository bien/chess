import numpy as np

def distance_from_center(rank, file):
	absrank = abs(3 - rank)
	absfile = abs(3 - file)
	diag = max(absrank, absfile)
	return diag + max(0, absrank - diag) + max(0, absfile - diag)

def diagonal_moves(rank, file):
	return 16 - abs(rank - file) - abs(7 - file - rank);


class Board:
    # white is capitalized
    PIECES = 'kqrbnpKQRBNP'
    WHITE = 1
    BLACK = 0
    def __init__(self):
        self.piece_boards = {piece: np.zeros(64, dtype=np.ubyte) for piece in Board.PIECES}
        self.color_to_play = Board.WHITE
        self.castle_availability = ''
        self.en_passant = None
        self.halfmove = 0
        self.fullmove = 0

    def read_fen(self, fen):
        board, self.color_to_play, self.castle_availability, self.en_passant, self.halfmove, self.fullmove = fen.split()
        if self.color_to_play in ('w', 'W'):
            self.color_to_play = self.WHITE
        elif self.color_to_play in ('b', 'B'):
            self.color_to_play = self.BLACK
        else:
            raise Exception("Unknown color {}".format(self.color_to_play))

        for row, contents in enumerate(board.split('/')):
            rank = 7 - row
            file = 0
            for square in contents:
                if '1' <= square <= '8':
                    file += int(square)
                else:
                    self.piece_boards[square][rank * 8 + file] = 1
                    file += 1

    def extract_old_features(self):
        pawns_by_file = {color: {-1:0, 8:0} | {file: self.piece_boards[color][file::8].sum() for file in range(8)} for color in ('p', 'P')}
        iso_pawns = {color: [pawns[file] if pawns_by_file[color][file-1] == 0 and pawns_by_file[color][file+1] == 0 else 0 for file in range(8)] for color, pawns in pawns_by_file.items()}
        dbl_pawns ={color: [pawns[file] - 1 if pawns[file] > 1 else 0 for file in range(8)] for color, pawns in pawns_by_file.items()}

        # pawn position is relative to player
        own_pawn_positions = {color: [[rank if color == 'P' else 7 - rank for rank, v in enumerate(self.piece_boards[color][file::8]) if v == 1] for file in range(8)] for color in 'pP'}
        # key is opponent
        opposition_pawn_positions = {'p' if color == 'P' else 'P': [min([7 - pawn for pawn in pawns[file]], default=0) for file in range(8)] for color, pawns in own_pawn_positions.items()}
        passed_pawns = {color: [sum([1 if all(pawnrank >= opp for opp in opposition_pawn_positions[color][max(0,file-1):file+2]) else 0 for pawnrank in pawns[file]]) for file in range(8)] for color, pawns in own_pawn_positions.items()}

        rhopenfile = {color: [sum(self.piece_boards[color][file::8]) if pawns_by_file[chr(ord(color) - ord('R') + ord('P'))][file] == 0 else 0 for file in range(8)] for color in 'rR'}
        rfopenfile = {color: [sum(self.piece_boards[color][file::8]) if pawns_by_file['p' if color == 'R' else 'p'][file] == 0 and rhopenfile[color][file] > 0 else 0 for file in range(8)] for color in 'rR'}

        bscore = {color: sum([diagonal_moves(pos // 8, pos % 8) for pos, val in enumerate(self.piece_boards[color]) if val == 1]) for color in 'bB'}
        qscore = {color: sum([diagonal_moves(pos // 8, pos % 8) for pos, val in enumerate(self.piece_boards[color]) if val == 1]) for color in 'qQ'}
        kscore = {color: sum([distance_from_center(pos // 8, pos % 8) for pos, val in enumerate(self.piece_boards[color]) if val == 1]) for color in 'kK'}
        nscore = {color: sum([distance_from_center(pos // 8, pos % 8) for pos, val in enumerate(self.piece_boards[color]) if val == 1]) for color in 'nN'}

    	# piece count scores
        return dict(
            qct=sum(self.piece_boards['Q']) - sum(self.piece_boards['q']),
            bct=sum(self.piece_boards['B']) - sum(self.piece_boards['b']),
            bpairct=(1 if sum(self.piece_boards['B']) >= 2 else 0) - (1 if sum(self.piece_boards['b']) >= 2 else 0),
            rct=sum(self.piece_boards['R']) - sum(self.piece_boards['r']),
            nct=sum(self.piece_boards['N']) - sum(self.piece_boards['n']),
            pct=sum(self.piece_boards['P']) - sum(self.piece_boards['p']),
            rpct=sum(self.piece_boards['P'][::8]) + sum(self.piece_boards['P'][7::8])
                - sum(self.piece_boards['p'][::8]) - sum(self.piece_boards['p'][7::8]),
            isopawn=sum(iso_pawns['P']) - sum(iso_pawns['p']),
            dblpawn=sum(dbl_pawns['P']) - sum(dbl_pawns['p']),
            ppawn=sum(passed_pawns['P']) - sum(passed_pawns['p']),
            rhopenfile=sum(rhopenfile['R']) - sum(rhopenfile['r']),
            rfopenfile=sum(rfopenfile['R']) - sum(rfopenfile['r']),
            bscore=bscore['B'] - bscore['b'],
            qscore=qscore['Q'] - qscore['q'],
            kscore=kscore['K'] - kscore['k'],
            nscore=nscore['N'] - nscore['n'],
            abs_num_pieces=sum([sum(self.piece_boards[color]) for color in 'BQKNPR']) + sum([sum(self.piece_boards[color]) for color in 'bqknpr']),
        )
	# int ppawn = 0, isopawn = 0, dblpawn = 0;
	# // piece position scores
	# int nscore = 0, bscore = 0, kscore = 0, rhopenfile = 0, rfopenfile = 0,  qscore = 0;

if __name__ == '__main__':
    b = Board()
    # b.read_fen("rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2")
    # print(b.extract_old_features())
    # b.read_fen("r5rk/p4p1p/4pPpQ/5q1P/1p1P1B2/8/2p1BP2/5K1R w - - 1 27")
    b.read_fen("r5rk/5p1p/4pPpQ/5q1P/1p1P1B2/8/2p1BP2/5KR1 b - - 2 27")
    print(b.extract_old_features())
