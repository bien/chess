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

def mirror(board):
    return np.concatenate([board[(8*(7-i)):(8*(7-i))+8] for i in range(8)])

def mirror_square(sq):
    return sq % 8 + 8 * (7 - (sq // 8))

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
        batch_y = self.y[idx * self.batch_size:(idx + 1) * self.batch_size]

        parts = [self.hydrate(b) for b in batch_x]
        invert_result = np.array([-1 if part[2] else 1 for part in parts])
        updated_x = np.array([part[0] for part in parts]), np.array([part[1] for part in parts])

        y_value = keras.utils.to_categorical(batch_y * invert_result + 1, self.num_classes)

        return updated_x, y_value

input_shape = (2, 64*64*10)
BATCH_SIZE = 256

def fiteval_model(model, x_train, x_test_data, epochs, batch_size):
    
    model.fit(x_train, validation_data=x_test_data, batch_size=batch_size, epochs=epochs)
    x_test = np.concatenate([x_test_data[i][0] for i in range(min(30, len(x_test_data)))], axis=1)
    y_test = np.concatenate([x_test_data[i][1] for i in range(min(30, len(x_test_data)))])
    y_output = model.predict((x_test[0], x_test[1]))
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

def train_model(model, df_train, df_test, epochs=6):
    df_sel = df_train[df_train.fen.str.len() > 5]
    print("available size", df_sel.shape)
    
    result_train = df_sel.result
    result_test = df_test.result
        
    x_train_sparse = FenSequence(df_train, result_train.values, BATCH_SIZE, True)
    x_test_sparse = FenSequence(df_test, result_test.values, BATCH_SIZE, True)
    fiteval_model(model, x_train_sparse, x_test_sparse, epochs=epochs, batch_size=BATCH_SIZE)
    return model

def relu_sat(x):
    return keras.activations.relu(x, max_value=1)


def make_nnue_model_mirror(input_shape, num_classes=3):
    inputs = []
    hidden = []
    l1hidden = layers.Dense(256, activation=relu_sat)
    for color in range(2):
        x = keras.Input(shape=input_shape[1:])
        inputs.append(x)
        hidden.append(l1hidden(x))
    x = layers.concatenate(hidden)
    x = layers.Dense(32, activation=relu_sat)(x)
    x = layers.Dense(32, activation=relu_sat)(x)
    outputs = layers.Dense(num_classes, activation="softmax")(x)

    return keras.Model(inputs=inputs, outputs=outputs, name="lobsternet_nnue")
