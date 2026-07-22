# moezip 🗜️

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)](https://isocpp.org/)

A learning-augmented, high-efficiency text compressor written in C++20. `moezip` combines a Mixture of Experts (MoE) predictive state router, Asymmetric Numeral Systems (rANS) arithmetic coding, and LZ-style back-referencing to compress natural language text.

---

## 📍 Table of Contents
* [Performance & Benchmarks](#performance--benchmarks)
* [Compilation & Building](#compilation--building)
  * [Prerequisites](#prerequisites)
  * [Step 1: Generate Embedded Assets](#step-1-generate-embedded-c-assets)
  * [Step 2: Choose Compilation Path](#step-2-select-your-compilation-path)
    * [Option 1: Standard Build (Zero Python Dependencies)](#-option-1-standard-build-zero-python-dependencies)
    * [Option 2: PyBind11 Build (Native `import moezip` Extension)](#-option-2-pybind11-build-native-import-moezip-extension)
* [Embedded Assets & Portability](#embedded-assets--portability)
* [Usage](#usage)
  * [1. Command Line Interface (CLI)](#1-command-line-interface-cli)
  * [2. Native Python Extension Module (`import moezip`)](#2-native-python-extension-module-import-moezip)
  * [3. Shared Library Integration (`ctypes`)](#3-shared-library-integration-ctypes--moezipdll--libmoezipso)
  * [4. Piping & Subprocess Fallback (`stdin` / `stdout`)](#4-piping--subprocess-fallback-stdin--stdout)
* [Contributing](#contributing)
* [License](#license)

---

## Performance & Benchmarks

In real-world benchmarks on everyday text snippets (4 KB – 15 KB), `moezip` consistently outperforms industry-standard algorithms like Facebook's **Zstandard (Zstd)** on text blocks, reducing inputs to **36%–42% of their original size**:

* **5.4 KB Text Clip:** Compresses to **2,286 bytes** (42.3%) vs. Zstd's 2,595 bytes (48.0%).
* **4.8 KB Text Clip:** Compresses to **2,072 bytes** (42.6%) vs. Zstd's 2,499 bytes (51.4%).
* **16 KB Document:** Compresses down to **5.7 KB**.

When loaded as an in-memory shared library (`moezip.dll` / `libmoezip.so`) or native Python extension (`moezip.pyd` / `moezip.so`), the pre-trained vocabulary remains resident in RAM, executing full compression passes in **< 3 milliseconds**!

---

## Compilation & Building

### Prerequisites
* A C++20 compatible compiler (MSVC on Windows, GCC 10+, or Clang 11+)
* CMake (3.16 or higher)
* Python 3.x
* `pybind11` (Optional, required only for native `import moezip` extension)

---

### Step 1: Generate Embedded C++ Assets
Run `make.py` first. This reads `words_final.txt` and `router_stateless_v4.json`, chunking them into `embedded_assets.hpp` to bypass compiler string literal limits:

**Windows CMD:**
```cmd
python make.py
```

**Linux / macOS:**
```bash
python3 make.py
```

---

### Step 2: Select Your Compilation Path

Choose one of the two compilation workflows depending on your requirements:

---

### 🔹 Option 1: Standard Build (Zero Python Dependencies)
*Use this option if you want the standalone CLI executable (`moezip`) and the C-API Shared Library (`moezip.dll` / `libmoezip.so`) for fast Python `ctypes` integration with zero external python dependencies.*

**Windows (MSVC):**
```cmd
cmake -B build
cmake --build build --config Release
```

**Linux / macOS (GCC / Clang):**
```bash
cmake -B build_linux -DCMAKE_BUILD_TYPE=Release
cmake --build build_linux --parallel
```

**Generated Binaries:**
* **Windows (`build/Release/`):** `moezip.exe` (CLI) and `moezip.dll` (Shared Library)
* **Linux/macOS (`build_linux/`):** `moezip` (CLI) and `libmoezip.so` (Shared Library)

---

### 🔹 Option 2: PyBind11 Build (Native `import moezip` Extension)
*Use this option if you want to compile a native C++ Python extension module (`moezip.pyd` on Windows / `moezip.so` on Linux) for direct `import moezip` support.*

**1. Install PyBind11:**
```bash
# Windows
pip install pybind11

# Linux (Debian/Ubuntu)
sudo apt install python3-pybind11
```

**2. Configure CMake:**

*Windows CMD (Automated Path Detection):*
```cmd
for /f "delims=" %i in ('python -c "import pybind11; print(pybind11.get_cmake_dir())"') do for /f "delims=" %j in ('python -c "import sys; print(sys.executable)"') do cmake -B build -Dpybind11_DIR="%i" -DPYTHON_EXECUTABLE="%j" -DPython_EXECUTABLE="%j"
```

*Linux / macOS:*
```bash
cmake -B build_linux -DCMAKE_BUILD_TYPE=Release
```

**3. Compile Release Binaries:**
```bash
# Windows
cmake --build build --config Release

# Linux / macOS
cmake --build build_linux --parallel
```

**Generated Extension Module:**
* **Windows:** `build/Release/moezip.pyd`
* **Linux / macOS:** `build_linux/moezip.so`

---

## Embedded Assets & Portability

`moezip` is **100% portable and self-contained**. 

During `make.py`, your vocabulary (`words_final.txt`) and transition matrix (`router_stateless_v4.json`) are chunked into C++ string literals (`embedded_assets.hpp`) and compiled directly into the binary and libraries.

* **Zero File Dependencies:** You do **NOT** need `words_final.txt` or `router_stateless_v4.json` on disk to run `moezip`. You can distribute the compiled binaries to any folder or machine.
* **Optional Disk Override:** If you wish to test a new vocabulary or domain-trained matrix without recompiling, place a `words_final.txt` or `router_stateless_v4.json` file in the execution directory. `moezip` will detect disk files and override its embedded defaults automatically.

---

## Usage

### 1. Command Line Interface (CLI)

*Note: On Windows, use `build\Release\moezip.exe`. On Linux/macOS, use `./moezip`.*

**Compress a text file:**
```bash
moezip compress input.txt -o archive.moe
```

**Decompress an archive:**
```bash
moezip decompress archive.moe -o restored.txt
```

**Decompress directly from a Hex String:**
```bash
moezip hexdec 880A090A20202020... -o restored.txt
```

**Train a custom domain router matrix:**
Optimize the compressor for specialized fields (e.g., medical, legal, or code) by feeding it a text corpus. This generates a new `router_stateless_v4.json` file. Re-run `make.py` and recompile to embed the new weights:
```bash
moezip train --corpus path/to/dataset.txt
```

---

### 2. Native Python Extension Module (`import moezip`)

Copy `moezip.pyd` (Windows) or `moezip.so` (Linux/macOS) into your Python project directory:

```python
import moezip

# Compress directly in RAM (automatic embedded asset loading)
compressed_bytes = moezip.compress("Hello world, compress this text!")

# Decompress back to string
original_text = moezip.decompress(compressed_bytes)

print(f"Original  : '{original_text}'")
print(f"Compressed: {compressed_bytes.hex().upper()}")
```

---

### 3. Shared Library Integration (`ctypes` + `moezip.dll` / `libmoezip.so`)

If built with Option 1, use Python's built-in `ctypes` to interface directly with the shared library in memory:

```python
import os
import sys
import ctypes

# Auto-select shared library based on OS
lib_name = "moezip.dll" if sys.platform == "win32" else "libmoezip.so"
lib_path = os.path.abspath(os.path.join("build/Release" if sys.platform == "win32" else "build_linux", lib_name))

moezip = ctypes.CDLL(lib_path)

# Configure C-Function signatures
moezip.init_engine.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
moezip.compress_text.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_int]
moezip.compress_text.restype = ctypes.c_int
moezip.decompress_bytes.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
moezip.decompress_bytes.restype = ctypes.c_int

# Pre-load embedded vocabulary in RAM at app startup (passing empty string loads from binary)
moezip.init_engine(b"", b"")

# In-Memory Compression Function (< 3ms speed)
def compress(text: str) -> bytes:
    raw_bytes = text.encode('utf-8')
    buf_size = len(raw_bytes) * 2 + 2048
    out_buf = (ctypes.c_uint8 * buf_size)()
    
    comp_len = moezip.compress_text(raw_bytes, out_buf, buf_size)
    if comp_len <= 0: raise RuntimeError("Compression error")
    return bytes(out_buf[:comp_len])
```

---

### 4. Piping & Subprocess Fallback (`stdin` / `stdout`)

`moezip` supports standard I/O pipes. On Windows, standard streams are set to binary mode to prevent OS newline translation corruption (`0x0A` to `0x0D 0x0A`).

Diagnostic logging is directed to `stderr`, leaving `stdout` clean for binary data transfer.

**CLI Pipe Example:**
```bash
(echo | set /p="Hello there, compress this string.") | moezip compress - -o - > archive.moe
```

---

## Contributing

PRs are welcome. If you are looking for areas to contribute, consider:

* **Zero-Copy Tokenization:** Refactoring `std::string` manipulations in `tokenizer.cpp` to use `std::string_view` to eliminate temporary allocations during encoding.
* **LZ Cache Optimization:** Replacing `std::unordered_map<std::string, std::deque<int>>` in `codec.cpp` with a flat integer hash-chain array (similar to zlib) for faster LZ match lookups on large inputs.
* **SIMD / AVX2 Acceleration:** Vectorizing the rANS probability search in `ans.cpp`.

## License

This project is licensed under the MIT License - see the `LICENSE` file for details.