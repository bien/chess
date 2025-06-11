import sys
import numpy as np
import keras
from fentrain import relu_sat

UNITY = 64

# remapping for centipawn-aware version
layer_name_remapping = {
    'dense_5': 'hidden_0',
}

def get_layer(model, canonical_name):
    all_layer_names = [layer.name for layer in model.layers]
    for alias, canonical in layer_name_remapping.items():
        if alias in all_layer_names and canonical == canonical_name:
            return model.get_layer(alias)

    return model.get_layer(canonical_name)

def get_dense_layer_names(model):
    dense_layer_names = []
    all_layer_names = [layer.name for layer in model.layers]
    for i in range(6, 8):
        name = f'dense_{i}'
        if name in all_layer_names:
            dense_layer_names.append(name)
        else:
            break
    for i in range(1, 3):
        name = f'hidden_{i}'
        if name in all_layer_names:
            dense_layer_names.append(name)
        else:
            break
    return dense_layer_names

def preamble(MODEL_DENSE_LAYERS, MODEL_HIDDEN_LAYER_WIDTH, MODEL_FIRST_LAYER_WIDTH):
    model_input_bias = print_biases('int8_t', get_layer(model, 'hidden_0'))
    model_psqt = print_weights('int16_t', get_layer(model, 'pts'), flatten=True)
    model_psqt_bias = print_bias('int16_t', get_layer(model, 'pts'))
    model_input_weights = print_weights('int8_t', get_layer(model, 'hidden_0'))

    dense_layer_names = get_dense_layer_names(model)
    model_dense_bias = ',\n'.join([print_biases('int16_t', get_layer(model, dense_layer)) for dense_layer in dense_layer_names])
    model_dense_1_weights = '{%s}' % print_weights('int8_t', get_layer(model, dense_layer_names[0]))
    model_dense_weights = ',\n'.join(['{%s}' % print_weights('int8_t', get_layer(model, dense_layer)) for dense_layer in dense_layer_names[1:]])

    model_cp_bias = print_bias('int16_t', get_layer(model, 'centipawns'))
    model_cp_weights = print_weights('int8_t', get_layer(model, 'centipawns'), flatten=True)
    return f"""
#include <stdint.h>
#include <nnueeval.hh>

extern const int MODEL_DENSE_LAYERS; // excluding cp output layer
extern const int MODEL_HIDDEN_LAYER_WIDTH;
extern const int MODEL_FIRST_LAYER_WIDTH;

nnue_model my_model = {{
    {model_input_bias},
    {{{model_psqt}}},
    {model_psqt_bias},
    {{{model_input_weights}}},

    // dense_bias
    {{{model_dense_bias}}},
    // dense_1_weights
    {model_dense_1_weights},
    // NB: omits first layer!
    {{{model_dense_weights}}},

    // cp_bias
    {model_cp_bias},
    {{{model_cp_weights}}},
}};

nnue_model *load_nnue_model() {{
    static_assert(MODEL_DENSE_LAYERS == {MODEL_DENSE_LAYERS});
    static_assert(MODEL_HIDDEN_LAYER_WIDTH == {MODEL_HIDDEN_LAYER_WIDTH});
    static_assert(MODEL_FIRST_LAYER_WIDTH == {MODEL_FIRST_LAYER_WIDTH});
    return &my_model;
}}
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
    model = keras.saving.load_model(sys.argv[1], custom_objects=dict(relu_sat=relu_sat))
    print(preamble(len(get_dense_layer_names(model)), get_layer(model, 'centipawns').kernel.numpy().shape[0],
                   get_layer(model, 'hidden_0').bias.numpy().shape[0] * 2))
