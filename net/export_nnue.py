import sys
import numpy as np
import keras
from fentrain import relu_sat

model = keras.saving.load_model(sys.argv[1], custom_objects=dict(relu_sat=relu_sat))

print("#include <stdint.h>")
print("#ifdef __cplusplus")
print('extern "C" {')
print("#endif")

# remapping for centipawn-aware version
centipawn_layer_names = {
    'dense_3': 'dense',
    'dense_4': 'dense_1',
    'dense_5': 'dense_2',
    'result':  'dense_3',
    'wdl': 'dense_3',
}

layer_names = [layer.name for layer in model.layers]
def layer_rewrite(layer_name):
    if layer_name in centipawn_layer_names:
        return centipawn_layer_names[layer_name]
    elif layer_name in centipawn_layer_names.values():
        return layer_name
    else:
        return None

for layer in model.layers:
    layer_name = layer_rewrite(layer.name)
    if layer_name is None:
        continue
    if layer_name == 'dense':
        bias_type = 'int8_t'
    else:
        bias_type = 'int16_t'
    print(f"const {bias_type} {layer_name}_bias[] = {{{', '.join(str(max(-128 if bias_type == 'int8_t' else -32768, min(127 if bias_type == 'int8_t' else 32767, int(np.rint(x * 64))))) for x in layer.bias.numpy())}}};")
    print(f"""const int8_t {layer_name}_weights[{layer.kernel.numpy().shape[0]}][{layer.kernel.numpy().shape[1]}] = {{""")
    print("  " + ', '.join(['{' + ', '.join(str(int(max(-128, min(127, np.rint(x * 64))))) for x in row) + '}' for row in layer.kernel.numpy()]))
    print("};")

print("#ifdef __cplusplus")
print('}')
print("#endif")
