import sys
import numpy as np
import keras
from fentrain import relu_sat

model = keras.saving.load_model(sys.argv[1], custom_objects=dict(relu_sat=relu_sat))

print("#include <stdint.h>")
for layer in model.layers:
    if layer.name.startswith("dense"):
        print(f"const int16_t {layer.name}_bias[] = {{{', '.join(str(int(np.rint(x * 64))) for x in layer.bias.numpy())}}};")
        print(f"""const int8_t {layer.name}_weights[{layer.kernel.numpy().shape[0]}][{layer.kernel.numpy().shape[1]}] = {{""")
        print("  " + ', '.join([', '.join(str(int(np.rint(x * 64))) for x in row) for row in layer.kernel.numpy()]))
        print("};")
