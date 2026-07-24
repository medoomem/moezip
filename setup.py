import os
import sys
from setuptools import setup, Extension

try:
    from pybind11.setup_helpers import PyBind11Extension, build_ext
    has_pybind11_helpers = True
except ImportError:
    has_pybind11_helpers = False
    from setuptools.command.build_ext import build_ext

if not os.path.exists("embedded_assets.hpp"):
    if os.path.exists("make.py"):
        print("embedded_assets.hpp not found. Running make.py...")
        os.system(f"{sys.executable} make.py")
    else:
        raise FileNotFoundError("Missing embedded_assets.hpp and make.py!")

extra_compile_args = []
if sys.platform == "win32":
    extra_compile_args = ["/std:c++20", "/O2", "/EHsc"]
else:
    extra_compile_args = ["-std=c++20", "-O3"]

sources = [
    "bindings.cpp",
    "vocab.cpp",
    "tokenizer.cpp",
    "ans.cpp",
    "router.cpp",
    "codec.cpp",
]

if has_pybind11_helpers:
    ext = PyBind11Extension(
        "moezip",
        sources=sources,
        cxx_std=20,
        extra_compile_args=extra_compile_args,
    )
else:
    import pybind11
    ext = Extension(
        "moezip",
        sources=sources,
        include_dirs=[pybind11.get_include()],
        language="c++",
        extra_compile_args=extra_compile_args,
    )

setup(
    name="moezip",
    version="1.0.10",
    author="medoomem",
    description="A learning-augmented text compression engine in C++20",
    long_description=open("README.md", encoding="utf-8").read() if os.path.exists("README.md") else "",
    long_description_content_type="text/markdown",
    url="https://github.com/medoomem/moezip",
    ext_modules=[ext],
    cmdclass={"build_ext": build_ext},
    package_data={"moezip": ["*.pyi"]},
    data_files=[("", ["moezip.pyi"])],  # Package .pyi stub alongside .pyd / .so
    python_requires=">=3.8",
    classifiers=[
        "Programming Language :: C++",
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Topic :: System :: Archiving :: Compression",
    ],
)