import sys
import chess.pgn
import chessdecode
import numpy as np
import pandas as pd
import keras.utils
import tensorflow as tf
from keras import layers
import random
import cProfile, pstats, io
from pstats import SortKey

def mirror_vertical(board):
    return np.concatenate([board[(8*(7-i)):(8*(7-i))+8] for i in range(8)])

def mirror_horizontal(board):
    return np.concatenate([board[i*8+7:i*8-1 if i else None:-1] for i in range(8)])

def mirror_square(sq):
    return sq % 8 + 8 * (7 - (sq // 8))

def relu_sat(x):
    return keras.activations.relu(x, max_value=1)

def get_positions(pgnfile):
    pgn = open(pgnfile)
    while True:
        game = chess.pgn.read_game(pgn)
        if game is None:
            break
        next_ply = random.randint(1, 10)
        while game is not None:
            score = game.eval()
            game_next = game.next()
            if game_next is None:
                break
            board = game.board()
            if score is not None and game.ply() >= next_ply and not board.is_capture(game_next.move) and not board.is_check() and not game_next.board().is_check():
                yield game.board().fen(), game_next.move, score.pov(board.turn)
                next_ply += random.randint(1, 10)
            game = game_next



class NNUEModel:

    def __init__(self, num_hidden_layers=2, hidden_layers_width=32, relu_fn=relu_sat):
        self.CASTLE_KING_IDX = {'k': 32, 'q': 33, 'kq': 34, 'qk': 34}
        self.num_hidden_layers = num_hidden_layers
        self.hidden_layers_width = hidden_layers_width
        self.half_len_concat = 256
        self.relu_fn = relu_fn
        self.white_piece_list = self._piece_list().upper() + self._piece_list().lower()
        self.black_piece_list = self._piece_list().lower() + self._piece_list().upper()
        self.INPUT_LENGTH = self._num_king_buckets() * len(self.white_piece_list) * 64
        self.EMPTY = np.zeros(self.INPUT_LENGTH, dtype=np.ubyte)

    def _num_king_buckets(self):
        return 35

    def _piece_list(self):
        return 'PNBRQ'

    def _king_idx_map(self, present_board, castle_rights, king_idx):
        if castle_rights:
            king_idx = self.CASTLE_KING_IDX[castle_rights]
        elif king_idx % 8 < 4:
            king_idx = (king_idx // 8) * 4 + (king_idx % 8)
        else:
            # horizontal mirror to save effort
            king_idx = (king_idx // 8) * 4 + 7 - (king_idx % 8)
            present_board = [mirror_horizontal(b) for b in present_board]
        return king_idx, present_board


    def read_fen(self, fen):
        """Return side-to-move-king, s-t-m-board-sequence, opp-king, opp-board, white-to-move, invert result"""
        b = chessdecode.Board()
        b.read_fen(fen)
        w_full = [b.piece_boards[piece] for piece in self.white_piece_list]
        b_full_mirror = [mirror_vertical(b.piece_boards[piece]) for piece in self.black_piece_list]
        w_king_idx = np.where(b.piece_boards['K'] == 1)[0][0]
        b_king_idx = np.where(b.piece_boards['k'] == 1)[0][0]
        white_castling = ''.join([s.lower() for s in b.castle_availability if s in 'KQ'])
        black_castling = ''.join([s for s in b.castle_availability if s in 'kq'])

        if b.color_to_play == chessdecode.Board.WHITE:
            return w_king_idx, w_full, white_castling, mirror_square(b_king_idx), b_full_mirror, black_castling
        else:
            return mirror_square(b_king_idx), b_full_mirror, black_castling, w_king_idx, w_full, white_castling

    def centipawn_result_iterator(self, positions):
        board_step_size = 64 * len(self.white_piece_list)

        for fen, next_move, white_score in positions:
            if white_score.is_mate():
                continue

            w_king_idx, w_full, w_castle, b_king_idx, b_full, b_castle = self.read_fen(fen)
            input_bits = []
            myboard = self.EMPTY.copy()
            theirboard = self.EMPTY.copy()
            for king_idx, present_board, castle_rights, container in ((w_king_idx, w_full, w_castle, myboard), (b_king_idx, b_full, b_castle, theirboard)):
                king_idx, present_board = self._king_idx_map(present_board, castle_rights, king_idx)
                container[king_idx * board_step_size:(king_idx + 1) * board_step_size] = np.concatenate(present_board)

            yield (np.array([myboard]), np.array([theirboard])), (np.array([keras.utils.to_categorical(int(np.sign(white_score.cp)) + 1, 3)]).astype(np.ubyte), np.array([white_score.cp]).astype(np.int16))


    def make_nnue_model_mirror(self, num_classes=3, include_centipawns=False):
        inputs = []
        hidden = []
        l1hidden = layers.Dense(self.half_len_concat, activation=self.relu_fn)
        for color in range(2):
            x = keras.Input(shape=(self.INPUT_LENGTH,), name='side_{}'.format(color))
            inputs.append(x)
            hidden.append(l1hidden(x))
        x = layers.concatenate(hidden)
        for i in range(self.num_hidden_layers):
            x = layers.Dense(self.hidden_layers_width, activation=self.relu_fn)(x)
        result = layers.Dense(num_classes, activation="softmax", name="result")(x)
        if include_centipawns:
            centipawns = layers.Dense(1, name="centipawns")(x)
            outputs = [result, centipawns]
        else:
            outputs = result

        model = keras.Model(inputs=tuple(inputs), outputs=outputs, name="lobsternet_nnue")
        if include_centipawns:
            model.compile(
                loss=["categorical_crossentropy", "mean_squared_error"],
                loss_weights=[1, .000002],
                optimizer="adam",
                metrics=["categorical_accuracy", "mean_squared_error"])
        else:
            model.compile(loss=["categorical_crossentropy"], optimizer="adam", metrics=["categorical_accuracy"])
        return model

    def train(self, train_pgn, valid_pgn, profile=False, **fit_args):
        model = self.make_nnue_model_mirror(include_centipawns=True)
        model.summary()
        sign = ((tf.TensorSpec(shape=(None, self.INPUT_LENGTH), dtype=tf.uint8, name='side_0'),
                tf.TensorSpec(shape=(None, self.INPUT_LENGTH), dtype=tf.uint8, name='side_1')),
                (tf.TensorSpec(shape=(None, 3), dtype=tf.uint8, name='result'),
                tf.TensorSpec(shape=(None, ), dtype=tf.int16, name='centipawns')))
        tf_data_generator = tf.data.Dataset.from_generator(lambda: self.centipawn_result_iterator(get_positions(train_pgn)),
            output_signature=sign)
        validation_data_generator = tf.data.Dataset.from_generator(lambda: self.centipawn_result_iterator(get_positions(valid_pgn)),
            output_signature=sign)
        if profile:
            pr = cProfile.Profile()
            pr.enable()
        history = model.fit(tf_data_generator, validation_data=validation_data_generator, batch_size=128, steps_per_epoch=256, **fit_args)
        if profile:
            pr.disable()
            pr.dump_stats('out.stats')

        return model

def sqrelu_clamp(x):
    return tf.square(keras.activations.relu(x, max_value=1))

class BasicNNUE(NNUEModel):
    def __init__(self, num_hidden_layers=0, hidden_layers_width=32):
        super().__init__(num_hidden_layers=num_hidden_layers, hidden_layers_width=hidden_layers_width, relu_fn=sqrelu_clamp)

    def _num_king_buckets(self):
        return 1

    def _piece_list(self):
        return 'PNBRQK'

    def _king_idx_map(self, present_board, castle_rights, king_idx):
        return 0, present_board

def main(pgnfile, validation_pgn):
    print("Loading model")
    n = BasicNNUE()
    model = n.train(pgnfile, validation_pgn, profile=True, epochs=1)
    return model

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])