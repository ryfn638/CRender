<h1 align="center"> CRender </h1>

A customised 3D rendering library designed for quick 3D renders without the setup pipeline required by OpenGL. Optimised for easy Python integration — in essence a 3-dimensional Pygame with more efficient shape drawing methods. Note that this does not open a window, you will have to use an extra library to do this, this is to allow more control over visuals, this will only output the frame data.

> **Note:** This package is currently in early development and may not be stable.

## Installation
```
pip install crender
```

## Usage

To display outputs, you will need to use an external library such as opencv like so
```python
import numpy as np
import cv2

buf = engine.get_framebuffer()
frame = np.frombuffer(buf, dtype=np.uint8).reshape(height, width, 4)
cv2.imshow("Renderer", frame)
```

To have the display auto update, simply set `autoupdate` to true and define the desired framerate:
```python
from crender import Engine

engine = Engine()
engine.init(autoupdate=True, fps=60)
```

For manual updates, set `autoupdate` to false and call `update()` yourself:
```python
engine = Engine()
engine.init(autoupdate=False, fps=60)  # fps is negligible when autoupdate is False

while True:
    engine.update()
```

## Adding Shapes

Load a shape from an OBJ file and position it in the scene:
```python
from crender import Engine, create_point

engine = Engine()
sofa = engine.add_shape("sofa.obj", create_point(0, 0, 0), width=1, height=1)
sofa.move_shape(10, 0, 0)
```

## Camera

Update the camera position and angle:
```python
from crender import create_point

position = create_point(1, 1, 1)
engine.update_camera(position, angleX=0, angleY=0, angleZ=0)
```

## Preview
<img alt="Preview" src="">

## License
Apache 2.0 — See LICENSE for more details