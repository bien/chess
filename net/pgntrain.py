import sys
import chess.pgn
import fentrain
import numpy as np
import pandas as pd
import keras.utils
import tensorflow as tf

def get_positions(pgnfile):
    pgn = open(pgnfile)
    while True:
        game = chess.pgn.read_game(pgn)
        if game is None:
            break
        print(game.headers)
        while game is not None:
            score = game.eval()
            game_next = game.next()
            if game_next is None:
                break
            board = game.board()
            if score is not None and game.ply() > 5 and not board.is_capture(game_next.move) and not board.is_check() and not game_next.board().is_check():
                yield game.board().fen(), game_next.move, score.pov(board.turn)
            game = game_next

EMPTY = np.zeros(64*64*10, dtype=np.ubyte)

def centipawn_result_iterator(positions, batch_size=None):
    f = fentrain.FenSequence(pd.DataFrame(columns=['fen']), None, None)
    myboards = []
    theirboards = []
    centipawns = []

    for fen, next_move, white_score in positions:
        if white_score.is_mate():
            continue

        w_king_idx, w_full, b_king_idx, b_full, white_to_play = f.read_fen(fen)
        input_bits = []
        myboard = EMPTY.copy()
        theirboard = EMPTY.copy()
        for king_idx, present_board, container in ((w_king_idx, w_full, myboard), (b_king_idx, b_full, theirboard)):
            container[king_idx * 64 * 10:(king_idx + 1) * 64 * 10] = np.unpackbits(present_board)

        if batch_size is None:
            yield (np.array([myboard]), np.array([theirboard])), (np.array([keras.utils.to_categorical(int(np.sign(white_score.cp)) + 1, 3)]).astype(np.ubyte), np.array([white_score.cp]).astype(np.int16))
        else :
            centipawns.append(white_score.cp)
            myboards.append(myboard)
            theirboards.append(theirboard)



            if len(centipawns) == batch_size:
                yield (np.array(myboards), np.array(theirboards)), (np.array(keras.utils.to_categorical(np.sign(centipawns) + 1, 3)).astype(np.ubyte), np.array(centipawns).astype(np.int16))
                myboards = []
                theirboards = []
                centipawns = []

    if batch_size is not None and len(centipawns) > 0:
        yield (np.array(myboards), np.array(theirboards)), (np.array(keras.utils.to_categorical(np.sign(centipawns) + 1, 3)).astype(np.ubyte), np.array(centipawns).astype(np.int16))


def main(pgnfile):
    print("Loading model")
    model = fentrain.make_nnue_model_mirror(fentrain.input_shape, include_centipawns=True)
    model.summary()
    print ("Calling fit")
    tf_data_generator = tf.data.Dataset.from_generator(lambda: centipawn_result_iterator(get_positions(sys.argv[1]), 2),
        output_signature=(
            (tf.TensorSpec(shape=(None, 40960), dtype=tf.uint8, name='side_0'),
            tf.TensorSpec(shape=(None, 40960), dtype=tf.uint8, name='side_1')),
            (tf.TensorSpec(shape=(None, 3), dtype=tf.uint8, name='result'),
            tf.TensorSpec(shape=(None, ), dtype=tf.int16, name='centipawns')),
        ))
    history = model.fit(tf_data_generator, batch_size=128, epochs=1)
    # history = model.fit(centipawn_result_iterator(get_positions(sys.argv[1]), 4), batch_size=128, epochs=1)
    return history

if __name__ == '__main__':
    main(sys.argv[1])