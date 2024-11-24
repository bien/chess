from collections import defaultdict
import math
import random
import sys
import chess
import chess.pgn
from chess.polyglot import zobrist_hash
from polyglot_writer import Polyglot_Move, Polyglot_Position, Polyglot_Writer

class Node:
    def __init__(self, fen):
        self.moves = {}
        self.white_wins = 0
        self.draws = 0
        self.black_wins = 0
        self.fen = fen

    def __repr__(self):
        return "{}-{}-{}".format(self.white_wins, self.draws, self.black_wins)

class FenWriter:
    def __init__(self):
        self.multiplier = 100
        self.positions = []
        self.seen_hashes = set()

    def emit(self, fen, move, weight):
        board = chess.Board(fen)
        hash = zobrist_hash(board)
        if (hash, move) not in self.seen_hashes and weight > 0:
            self.seen_hashes.add((hash, move))
            self.positions.append(Polyglot_Position(hash,
                          Polyglot_Move.from_chess_move(board, chess.Move.from_uci(move)),
                          int(weight * self.multiplier),
                          0))

    def write(self, writer_file):
        Polyglot_Writer.write(self.positions, writer_file)


def norm_fen(board):
    return ' '.join(board.fen().split()[:-2])

def tree_search(posdb, as_white=True, root_fen='rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -'):
    root_node = posdb[root_fen]
    white, draw, black, move = _tree_search_node(root_node, as_white, as_white)
    return move

def _tree_search_node(node, white_to_move:bool, as_white=True, max_depth=20, fen_writer=None):
    """
    Return: (white-win, draw, black-win, move)
    """
    if not node.moves or max_depth < 0:
        return node.white_wins, node.draws, node.black_wins, ''
    elif len(node.moves) == 1 or node.white_wins + node.draws + node.black_wins <= 1:
        # only move
        return node.white_wins, node.draws, node.black_wins, list(node.moves.keys())[0]

    if white_to_move == as_white:
        # we choose maximizing score
        choices = []
        total_games = node.white_wins + node.draws + node.black_wins
        for move, dest in node.moves.items():
            w, d, b, m = _tree_search_node(dest, not white_to_move, as_white, max_depth=max_depth-1, fen_writer=fen_writer)
            num_games = w + d + b
            # remove options with less than 1% of moves
            if num_games * 100 < total_games:
                continue
            raw_score = (w - b) / num_games
            mcts_bound_term = math.sqrt(math.log(node.white_wins + node.draws + node.black_wins) / num_games)
            choices.append((move, dest, raw_score, mcts_bound_term, num_games))

        if as_white:
            scores = [math.log(c[4] + 1) * (1+c[2]/c[3]) for c in choices]
        else:
            scores = [math.log(c[4] + 1) * (1-c[2]/c[3]) for c in choices]

        if len(scores) == 1 or sum(scores) <= 0:
            best = random.choice(choices)
            if fen_writer:
                for choice in choices:
                    fen_writer.emit(node.fen, choice[0], 1)
        else:
            best = random.choices(choices, scores)[0]
            if fen_writer:
                for choice, score in zip(choices, scores):
                    fen_writer.emit(node.fen, choice[0], score)
        return best[1].white_wins, best[1].draws, best[1].black_wins, best[0]
    else:
        # assume opponent moves according to distribution in pgn
        n = node.white_wins + node.draws + node.black_wins
        white, draw, black = 0, 0, 0
        mode_move = (0, '?')
        for move, dest in node.moves.items():
            w, d, b, m = _tree_search_node(dest, not white_to_move, as_white, max_depth=max_depth-1, fen_writer=fen_writer)
            white += w
            draw += d
            black += b
            if (w+d+b > mode_move[0]):
                mode_move = (w + d + b, move)
        return white, draw, black, mode_move[1]


def update_result(node, result):
    if result == '1-0':
        node.white_wins += 1
    elif result == '0-1':
        node.black_wins += 1
    elif result == '1/2-1/2':
        node.draws += 1
    else:
        raise Exception("Unknown result {}".format(result))


def build_positions(pgnfiles, max_ply=20):
    position_db = {}
    games = 0
    for pgnfile in pgnfiles:
        with open(pgnfile) as pgn:
            while True:
                game = chess.pgn.read_game(pgn)
                if not game:
                    break
                result = game.headers['Result']
                games += 1
                board = game.board()
                start_pos = norm_fen(board)
                if start_pos not in position_db:
                    position_db[start_pos] = Node(start_pos)
                ply = 1
                update_result(position_db[start_pos], result)

                for move in game.mainline_moves():
                    source_fen = norm_fen(board)
                    board.push(move)
                    dest_fen = norm_fen(board)
                    if dest_fen not in position_db:
                        position_db[dest_fen] = Node(dest_fen)
                    dest_pos = position_db[dest_fen]
                    position_db[source_fen].moves[move.uci()] = dest_pos
                    update_result(dest_pos, result)
                    ply += 1
                    if ply >= max_ply:
                        break
    print("Parsed {} games".format(games))
    return position_db

def sample_line(as_white=True, num_games=10, num_plies=20, fen_writer=None):
    for gameno in range(num_games):
        moves = []
        board = chess.Board()
        for i in range(num_plies):
            for white_to_move in (True, False):
                if as_white == white_to_move:
                    w, d, b, move = _tree_search_node(pos[norm_fen(board)], as_white, white_to_move, fen_writer=fen_writer)
                    if not move:
                        break
                    moves.append(move)
                    board.push(chess.Move.from_uci(moves[-1]))
                else:
                    known_moves = [(move, node.white_wins + node.draws + node.black_wins) for move, node in pos[norm_fen(board)].moves.items()]
                    if len(known_moves) < 1:
                        break
                    moves.append(random.choices([m[0] for m in known_moves], [m[1] for m in known_moves])[0])
                    board.push(chess.Move.from_uci(moves[-1]))
        print(' '.join(moves))


if __name__ == '__main__':
    if len(sys.argv) <= 1:
        raise Exception("Usage: output.bin [input.pgn ...]")
    output_file = sys.argv[1]
    if len(sys.argv) == 2:
        inputs = [sys.stdin]
    else:
        inputs = sys.argv[2:]
    pos = build_positions(inputs)
    fen_writer = FenWriter()
    print('first move', tree_search(pos))
    root_fen = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -'

    print("Sample white games")
    sample_line()
    print("Sample black games")
    sample_line(False)

    print("Write out white repertoire")
    _tree_search_node(pos[root_fen], as_white=True, white_to_move=True, fen_writer=fen_writer)
    print("Write out black reportoire")
    for move, dest in pos[root_fen].moves.items():
        _tree_search_node(dest, as_white=False, white_to_move=False, fen_writer=fen_writer)


    print("initial move")
    white_to_move=True
    as_white=True
    node = pos[root_fen]
    choices = []
    for move, dest in node.moves.items():
        w, d, b, m = _tree_search_node(dest, not white_to_move, as_white)
        num_games = w + d + b
        raw_score = (w - b) / num_games
        mcts_bound_term = math.sqrt(2) * math.sqrt(math.log(node.white_wins + node.draws + node.black_wins) / num_games)
        choices.append((move, dest, raw_score, mcts_bound_term, num_games))

    scores = [math.log(c[4]+1) * (1+c[2]/c[3]) for c in choices]
    for s,c in zip(scores,choices):
        print(s, c)

    print("e4 responses")


    white_to_move=False
    as_white=False
    kpg = pos['rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq -']
    choices = []
    node = kpg
    for move, dest in node.moves.items():
        w, d, b, m = _tree_search_node(dest, not white_to_move, as_white)
        num_games = w + d + b
        raw_score = (w - b) / num_games
        mcts_bound_term = math.sqrt(math.log(node.white_wins + node.draws + node.black_wins) / num_games)
        choices.append((move, dest, raw_score, mcts_bound_term, num_games))

    scores = [math.log(c[4]) * (1-c[2]/c[3]) for c in choices]
    for s,c in zip(scores,choices):
        print(s, c)

    print("writing bin file", output_file)
    fen_writer.write(output_file)
