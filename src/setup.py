from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension

ext = Pybind11Extension(
    "crender",
    ["bindings.cpp"],
    include_dirs=["path/to/your/headers"],
)

setup(name="crender", ext_modules=[ext])
