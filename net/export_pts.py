import sys
import numpy as np
import keras
from fentrain import relu_sat
import pgntrain

UNITY = 64

def preamble(model):
    model_psqt = print_weights('int16_t', model.get_layer('pts'), flatten=True)
    return f"""
#include <psqt.h>

const int16_t psqt_weights[] = {{{model_psqt}}};
"""


def int_min(bias_type):
    return -128 if bias_type == 'int8_t' else -32768

def int_max(bias_type):
    return 127 if bias_type == 'int8_t' else 32767

def print_bias(bias_type, layer):
    val = layer.bias.numpy()
    return str(int(np.rint(val * UNITY)))

def print_biases(bias_type, layer):
    return f"{{{', '.join(str(max(int_min(bias_type), min(int_max(bias_type), int(np.rint(x * UNITY))))) for x in layer.bias.numpy())}}}"

def print_weights(bias_type, layer, flatten=False):
    if flatten:
        return ', '.join([', '.join(str(int(max(int_min(bias_type), min(int_max(bias_type), np.rint(x * UNITY))))) for x in row)
                          for row in layer.kernel.numpy()])
    else:
        return ', '.join(['{' + ', '.join(str(int(max(int_min(bias_type), min(int_max(bias_type), np.rint(x * UNITY))))) for x in row) + '}'
                          for row in layer.kernel.numpy()])


if __name__ == '__main__':
    nnue_model = pgntrain.NNUEModel()
    content = nnue_model.initialize_pts()
    model = keras.saving.load_model(sys.argv[1], custom_objects=dict(relu_sat=relu_sat, fixed_initializer=lambda shape, dtype: content.astype(dtype)))
    print(preamble(model))
