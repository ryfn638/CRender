# Building from source

CRender currently builds on **Windows only**, with an **NVIDIA GPU** and the CUDA toolkit.

## Requirements

- Python 3.12+ with `pybind11` and `setuptools`
- CUDA Toolkit. `setup.py` reads `CUDA_PATH` and defaults to `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.0`.
- MSVC (Visual Studio Build Tools), which `nvcc` and setuptools use to compile

## Build

```bash
pip install pybind11 setuptools
python setup.py build_ext --inplace
```

`setup.py` does two things:

1. It runs `nvcc` on `src/render.cu` to produce `src/render.obj`, with `-O3` and `/MD`.
2. It builds the `crender` pybind11 extension from `bindings.cpp` and the `src/*.cpp` files, links `render.obj` and `cudart`.

To build a wheel instead:

```bash
python setup.py bdist_wheel
```

## Visual Studio

`Crender.sln` / `Crender.vcxproj` build the C++ side for debugging in Visual Studio. The commented-out `main` at the bottom of `bindings.cpp` is a C++ test harness that renders 60 frames and shows them with OpenCV.

## Troubleshooting

- **Wrong or black output on the GPU path:** update your NVIDIA driver first. An outdated driver caused incorrect GPU output before (fixed in `e65099d`).
- **`cudaMallocHost failed` in the console:** the engine falls back to normal `malloc` for the framebuffer. Rendering still works, but copies back from the GPU are slower.
