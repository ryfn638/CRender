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
    ],
    include_dirs=["include", "src"],
)

setup(name="crender", ext_modules=[ext])