from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension
import subprocess
import os

# compile the .cu file separately with nvcc
subprocess.run([
    "nvcc", "-c", "src/render.cu",
    "-o", "src/render.o",
    "-I", "include",
    "--compiler-options", "/MD",  # MSVC compat
    "-O3"
], check=True)

ext = Pybind11Extension(
    "crender",
    [
        "bindings.cpp",
        "src/engine.cpp",
        "src/spatial.cpp",
        "src/buffer.cpp",
        "src/math.cpp",
        "src/arena.cpp",
        "src/light.cpp",
        "src/material.cpp",
    ],
    include_dirs=["include", "src"],
    extra_objects=["src/render.o"],  # link compiled CUDA object
    extra_link_args=["cudart.lib"],  # link CUDA runtime
    library_dirs=[r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.0\lib\x64"],
)

setup(name="crender", ext_modules=[ext])