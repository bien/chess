import math
import random
import pandas as pd
import numpy as np
import matplotlib
import seaborn as sns
from sklearn.metrics import confusion_matrix, roc_curve, roc_auc_score, auc
from sklearn.model_selection import train_test_split
import matplotlib.pyplot as plt
import chessdecode
import keras
from keras import layers
import tensorflow as tf
import keras.utils
import functools
import scipy.sparse
import glob
import subprocess
from subprocess import Popen, PIPE

def mirror(board):
    return np.concatenate([board[(8*(7-i)):(8*(7-i))+8] for i in range(8)])

def mirror_square(sq):
    return sq % 8 + 8 * (7 - (sq // 8))

def get_line_count(filename):
    if filename.endswith('.gz'):
        return int(subprocess.check_output(['wc', '-l', filename]).decode('utf-8').split()[0])
    else:
        p1 = Popen(["zgrep", "-Ec", "$", filename], stdout=PIPE)
        p2 = Popen(["wc", "-l"], stdin=p1.stdout, stdout=PIPE)
        p1.stdout.close()  # Allow p1 to receive a SIGPIPE if p2 exits.
        output = p2.communicate()[0]

        return int(output.decode('utf-8').split()[0])

def interpret_stockfish(stockfish):
    return pd.to_numeric(stockfish, errors='coerce')

def use_stockfish_score(df):
    df['score'] = df.result
    df['centipawns'] = interpret_stockfish(df.stockfish)
    # remove non-quiescent positions -- should be done in cleaning step
    # return df[np.logical_and(np.logical_and(df.centipawns > -40, df.centipawns < 40), np.logical_and(~df.next_move.str.contains('x'), ~df.next_move.str.contains('\\+')))]


class PositionSequence(keras.utils.Sequence):
    colnames = ['uci', 'moveno', 'url', 'timecontrol', 'whiteelo', 'blackelo', 'result', 'clock', 'stockfish', 'last_move', 'next_move', 'termination', 'fen']

    def __init__(self, pathglob, batch_size, mirror=False, include_centipawns=True, **kwargs):
        super().__init__(**kwargs)
        self.batch_size = batch_size
        self.include_centipawns = include_centipawns
        self.mirror = mirror
        self.filelist = glob.glob(pathglob)
        self.filelengths = {f: get_line_count(f) for f in self.filelist}
        self.openfilename = None
        self.file_df = None

    def __len__(self):
        return math.ceil(sum(self.filelengths.values()) / self.batch_size)

    def __getitem__(self, idx):
        if idx >= len(self):
            raise IndexError()
        # get file
        lineno = 0
        current_file = self.filelist[0]
        fileidx = 0
        while idx >= lineno + self.filelengths[current_file]:
            print(f"check {idx} in {current_file} from [{lineno}, {lineno + self.filelengths[current_file]}]")
            lineno += self.filelengths[current_file]
            fileidx += 1
            current_file = self.filelist[fileidx]
        if self.openfilename != current_file or self.file_df is None:
            self.openfilename = current_file
            self.file_df = pd.read_table(current_file, encoding="utf-8", index_col='url', names=self.colnames, sep='\t', on_bad_lines='warn')
            use_stockfish_score(self.file_df)
            if self.include_centipawns:
                yval = (self.file_df.result.values, self.file_df.centipawns.values)
            else:
                yval = self.file_df.result.values

            self.seq = FenSequence(self.file_df, yval, self.batch_size, self.mirror)
        return self.seq[idx - lineno]

class FenSequence(keras.utils.Sequence):
    empty = np.zeros(64*64*10, dtype=np.ubyte)
    num_classes = 3

    def __init__(self, df, y, batch_size, random_mirroring=False, **kwargs):
        super().__init__(**kwargs)
        self.random_mirroring = random_mirroring
        self.p = 0
        self.f = 0
        self.boards = [self.read_fen(fen) for fen in df.fen.values]
        self.y = y
        self.batch_size = batch_size

    def on_epoch_end(self):
        if len(self.y) == 2:
            data = list(zip(self.y[0], self.y[1], self.boards))
            random.shuffle(data)
            a, b, self.boards = zip(*data)
            self.y = (a, b)
        else:
            data = list(zip(self.y, self.boards))
            random.shuffle(data)
            self.y, self.boards = zip(*data)

    def read_fen(self, fen):
        """Return side-to-move-king, s-t-m-board-sequence, opp-king, opp-board, white-to-move, invert result"""
        b = chessdecode.Board()
        b.read_fen(fen)
        w_full = np.packbits(np.concatenate([b.piece_boards[piece] for piece in 'PNBRQpnbrq']))
        b_full = np.packbits(np.concatenate([mirror(b.piece_boards[piece]) for piece in 'pnbrqPNBRQ']))
        w_king_idx = np.where(b.piece_boards['K'] == 1)[0][0]
        b_king_idx = np.where(b.piece_boards['k'] == 1)[0][0]

        mirror_times = 0
        invert_result = False
        if b.color_to_play == chessdecode.Board.BLACK:
            mirror_times += 1
        if self.random_mirroring:
            self.p += 1
            flip = random.random() > 0.5
            mirror_times += flip
            if flip:
                self.f += 1

        if mirror_times % 2 == 0:
            return w_king_idx, w_full, mirror_square(b_king_idx), b_full, False
        else:
            return mirror_square(b_king_idx), b_full, w_king_idx, w_full, True

    def __len__(self):
        return math.ceil(len(self.boards) / self.batch_size)

    def hydrate(self, a):
        w_king_idx, w_full, b_king_idx, b_full, white_to_play = a
        input_bits = []
        for king_idx, present_board in ((w_king_idx, w_full), (b_king_idx, b_full)):
            board = self.empty.copy()
            board[king_idx * 64 * 10:(king_idx + 1) * 64 * 10] = np.unpackbits(present_board)
            input_bits.append(board)
        input_bits.append(white_to_play)
        return input_bits

    def __getitem__(self, idx):
        if idx > len(self):
            raise IndexError()
        batch_x = self.boards[idx * self.batch_size:(idx + 1) * self.batch_size]
        if len(self.y) == 2:
            batch_y = [self.y[i][idx * self.batch_size:(idx + 1) * self.batch_size] for i in range(2)]
        else:
            batch_y = self.y[idx * self.batch_size:(idx + 1) * self.batch_size]

        parts = [self.hydrate(b) for b in batch_x]
        invert_result = np.array([-1 if part[2] else 1 for part in parts])
        updated_x = np.array([part[0] for part in parts]), np.array([part[1] for part in parts])

        if len(batch_y) == 2:
            y_value = (keras.utils.to_categorical(batch_y[0] * invert_result + 1, self.num_classes), batch_y[1])
        else:
            y_value = keras.utils.to_categorical(batch_y * invert_result + 1, self.num_classes)
        return updated_x, y_value

input_shape = (2, 64*64*10)
BATCH_SIZE = 256

def fiteval_model(model, x_train, x_test_data, epochs, batch_size, include_centipawns):

    history = model.fit(x_train, validation_data=x_test_data, batch_size=batch_size, epochs=epochs)
    x_test = np.concatenate([x_test_data[i][0] for i in range(min(30, len(x_test_data)))], axis=1)
    if include_centipawns:
        y_test = np.concatenate([x_test_data[i][1][0] for i in range(min(30, len(x_test_data)))])
    else:
        y_test = np.concatenate([x_test_data[i][1] for i in range(min(30, len(x_test_data)))])
    y_output = model.predict((x_test[0], x_test[1]))
    if include_centipawns:
        y_pred = y_output[0][:,2]
    else:
        y_pred = y_output[:,2]
    y_actual = y_test[:,2] - 0.5

    cf_matrix = confusion_matrix(y_actual > 0, y_pred >= 0.5)
    sns.heatmap(cf_matrix/np.sum(cf_matrix), annot=True,
                fmt='.2%', cmap='Blues')
    plt.show()

    fp, tp, _ = roc_curve(y_actual > 0, y_pred)
    plt.plot(fp,tp)
    plt.show()

    print(roc_auc_score(y_actual > 0, y_pred))
    print(model)
    return history

def train_model_big(model, df_train_glob, df_test, include_centipawns=False, epochs=6, batch_size=BATCH_SIZE, mirror=False):

    if include_centipawns:
        result_test = (df_test.result.values, df_test.centipawns.values)

    else:
        result_test = df_test.result.values

    x_train_sparse = PositionSequence(df_train_glob, batch_size, mirror)
    x_test_sparse = FenSequence(df_test, result_test, batch_size, mirror)
    history = fiteval_model(model, x_train_sparse, x_test_sparse, epochs=epochs, batch_size=batch_size, include_centipawns=include_centipawns)
    return history

def train_model(model, df_train, df_test, include_centipawns=False, epochs=6, batch_size=BATCH_SIZE, mirror=False):
    print("available size", df_train.shape)

    if include_centipawns:
        result_train = (df_train.result.values, df_train.centipawns.values)
        result_test = (df_test.result.values, df_test.centipawns.values)

    else:
        result_train = df_train.result.values
        result_test = df_test.result.values

    x_train_sparse = FenSequence(df_train, result_train, batch_size, mirror)
    x_test_sparse = FenSequence(df_test, result_test, batch_size, mirror)
    fiteval_model(model, x_train_sparse, x_test_sparse, epochs=epochs, batch_size=batch_size, include_centipawns=include_centipawns)
    return model

def relu_sat(x):
    return keras.activations.relu(x, max_value=1)


def make_nnue_model_mirror(input_shape, num_classes=3, include_centipawns=False):
    inputs = []
    hidden = []
    l1hidden = layers.Dense(256, activation=relu_sat)
    for color in range(2):
        x = keras.Input(shape=input_shape[1:], name='side_{}'.format(color))
        inputs.append(x)
        hidden.append(l1hidden(x))
    x = layers.concatenate(hidden)
    x = layers.Dense(32, activation=relu_sat)(x)
    x = layers.Dense(32, activation=relu_sat)(x)
    result = layers.Dense(num_classes, activation="softmax", name="result")(x)
    if include_centipawns:
        centipawns = layers.Dense(1, name="centipawns")(x)
        outputs = [result, centipawns]
    else:
        outputs = result

    model = keras.Model(inputs=inputs, outputs=outputs, name="lobsternet_nnue")
    if include_centipawns:
        model.compile(
            loss=["categorical_crossentropy", "mean_squared_error"],
            loss_weights=[1, .05],
            optimizer="adam",
            metrics=["categorical_accuracy", "mean_squared_error"])
    else:
        model.compile(loss=["categorical_crossentropy"], optimizer="adam", metrics=["categorical_accuracy"])
    return model
