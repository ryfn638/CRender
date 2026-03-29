from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension

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
        "src/window.cpp",
    ],
    include_dirs=["include", "src"],
)

setup(
    name="crender",
    version="0.1.0",
    description="A lightweight C++ 3D rendering library with Python bindings",
    long_description=open("README.md").read(),
    long_description_content_type="text/markdown",
    ext_modules=[ext],
    python_requires=">=3.8",
)