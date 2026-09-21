# Python API

The `crender` Python module is built from `bindings.cpp` with pybind11. This page lists everything the module exposes.

> Method names follow the C++ names, so some are camelCase (`addShape`) and some are snake_case (`get_framebuffer`). See issue #7 for the naming clean-up.

## Module functions

### `create_point(x, y, z) -> point_t`
Creates a point in 3D space. Points are used for shape positions, the camera position and light positions.

```python
import crender
origin = crender.create_point(0, 0, 0)
```

`point_t` is opaque from Python: you can pass it around but you cannot read or change its coordinates.

## `Engine`

One `Engine` is one renderer with its own framebuffer, shapes, lights and camera.

| Method | Description |
|---|---|
| `Engine()` | Creates an engine. Call `init` before anything else. |
| `init(width, height)` | Allocates the framebuffer and depth buffer for a `width` × `height` screen. |
| `load_material(path)` | Loads a `.mtl` file into the engine's material library. Call **before** `addShape` so faces can be matched to materials. |
| `addShape(path, position, width, height) -> Shape` | Loads a shape from an `.obj` file at `position`. `width` and `height` are stored but currently unused. |
| `removeShape(shape)` | Removes a shape from the scene. |
| `create_light(position, intensity, colour)` | Adds a point light. `intensity` is a float from 0 to 1; `colour` is an `(r, g, b)` tuple of 0–255 values. |
| `updateCamera(position, angleX, angleY, angleZ)` | Moves the camera. Angles are in radians. |
| `start_sim()` | Uploads all shapes, lights and materials to the GPU. Call after the scene is set up. |
| `update()` | Renders one frame on the GPU. |
| `get_framebuffer() -> bytes` | Returns the last rendered frame as raw bytes (`width * height * 4`, BGRA). |

## `Shape`

Returned by `Engine.addShape`.

| Method | Description |
|---|---|
| `moveShape(dx, dy, dz)` | Moves the shape by an offset on each axis. |
| `rotateShape(angleX, angleY, angleZ)` | Rotates the shape by angles (radians) on each axis. |
| `scaleShape(scale)` | Scales the shape uniformly. |

## Typical frame loop

```python
import crender

engine = crender.Engine()
engine.init(800, 600)
engine.load_material("sofa.mtl")
sofa = engine.addShape("sofa.obj", crender.create_point(0, 0, 0), 1, 1)
engine.create_light(crender.create_point(0, 1, 3), 0.9, (255, 255, 255))
engine.start_sim()

while running:
    engine.updateCamera(crender.create_point(0, 1, 3), 0.45, 0, 0)
    engine.update()
    frame = engine.get_framebuffer()  # hand to pygame / OpenCV to display
```
