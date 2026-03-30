import os
import subprocess
from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension
import pybind11
import sysconfig
CUDA_PATH = os.environ.get("CUDA_PATH", r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.0")

nvcc = os.path.join(CUDA_PATH, "bin", "nvcc.exe")
python_include = sysconfig.get_paths()["include"]
cuda_include = os.path.join(CUDA_PATH, "include")

subprocess.run([
    nvcc,
    "-c", "src/render.cu",
    "-o", "src/render.obj",
    "-I", "include",
    "-I", pybind11.get_include(),
    "-I", python_include,
    "-I", cuda_include,
    "-Xcompiler", "/MD",
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
    include_dirs=[
        "include",
        "src",
        pybind11.get_include(),
        python_include,  # required for Python.h
        cuda_include,
    ],
    extra_objects=["src/render.obj"],
    library_dirs=[os.path.join(CUDA_PATH, "lib", "x64")],
    libraries=["cudart"],
)

setup(
    name="crender",
    version="1.0.0",
    ext_modules=[ext],
)