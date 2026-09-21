# Architecture

CRender turns `.obj` / `.mtl` files into a framebuffer of pixels on an NVIDIA GPU. It does not open a window. The caller displays the framebuffer (for example with pygame or OpenCV).

## Components

| Area | Files | Responsibility |
|---|---|---|
| Engine | `include/engine.h`, `src/engine.cpp` | Public entry point. Owns the framebuffer, depth buffer, shapes, lights, camera and material library. |
| Geometry | `include/spatial.h`, `src/spatial.cpp` | `point_t`, `vertex_t`, `face_t`, `Shape`, `Camera`. OBJ loading, model/view/projection matrices (`build_mvp`). |
| Maths | `include/matrix.h`, `src/math.cpp` | Generic `matrix_t` (width × height of floats) and operations: multiply, add, subtract, scale, transpose, determinant. |
| Materials | `include/material.h`, `src/material.cpp` | Parses `.mtl` files (`newmtl`, `Ka`, `Kd`, `Ks`, `Ke`, `Ns`, `Ni`) into an `MTLLibrary`. |
| Lights | `include/light.h` | `light_t`: position, intensity, packed colour. |
| Memory | `include/arena.h`, `src/arena.cpp` | Arena allocator. Each shape lives in its own arena. |
| GPU renderer | `include/render.cuh`, `src/render.cu` | CUDA pipeline: `gpu_init`, `gpu_render`, `gpu_free`. |
| CPU renderer (legacy) | `include/buffer.h`, `src/buffer.cpp` | Original software rasteriser (`rasterize_face`). No longer called by `Engine::render`. |
| Python bindings | `bindings.cpp`, `setup.py` | pybind11 module `crender`. See [python-api.md](python-api.md). |

## Frame lifecycle

1. **`init(width, height)`** allocates a pinned host framebuffer (`uint32_t` per pixel) and a depth buffer.
2. **`load_material` / `addShape` / `create_light`** build the scene on the CPU. Each `addShape` parses the OBJ, creates an arena for the shape and appends it to `allShapes`.
3. **`start()` → `gpu_init`** copies all vertices, faces, lights and materials to device memory **once**, and allocates the device framebuffer, depth buffer and tile bins.
4. **`update()` → `render()` → `gpu_render`** runs each frame:
   1. **Clear:** zero the framebuffer, reset depth.
   2. **Vertex pass (`vertex_kernel`):** per shape, build the MVP matrix (90° FOV, near 0.1, far 1000) and transform every vertex to clip space.
   3. **Binning pass (`bin_kernel`):** each face is clipped and added to every 16×16 pixel tile its bounding box touches (max 1024 faces per tile).
   4. **Tile pass (`tile_kernel`):** one CUDA block per tile. Each thread is a pixel. It rasterises only that tile's faces, depth-tests in shared memory and shades with the lights and material colour.
   5. **Copy back:** the device framebuffer is copied to `engine->frame_buffer`.
5. **`get_framebuffer()`** returns those bytes to Python.

## Constraints to know about

- **GPU data is uploaded once.** Shapes added after `start()` are not on the GPU until `start()` runs again.
- **Transforms are applied through the MVP.** `moveShape` / `rotateShape` / `scaleShape` change the shape's model matrix inputs.
- **The pixel format is fixed.** One `uint32_t` per pixel, read as BGRA (see issue #5).
- **Transparency is ignored.** The MTL `d` value is parsed but not used by the renderer (see issue #6).
- **Hard-coded camera settings.** FOV, near and far planes are constants in `gpu_render`, not camera properties (see issue #12).
