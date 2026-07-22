# moezip 🗜️

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)

A learning-augmented, high-efficiency text compressor written in C++20. `moezip` combines a Mixture of Experts (MoE) predictive state router, Asymmetric Numeral Systems (rANS) arithmetic coding, and LZ-style back-referencing to compress natural language text.

Unlike standard dictionary-less compression algorithms (like Gzip or raw Zstd) that suffer from a "warm-up penalty" on short inputs, `moezip` embeds a pre-trained Markov state router and vocabulary directly into the compiled binary or DLL. This enables instant prediction of text patterns with zero metadata overhead.

---

## Performance & Benchmarks

In real-world benchmarks on everyday text snippets (4 KB – 15 KB), `moezip` consistently outperforms industry-standard algorithms like Facebook's **Zstandard (Zstd)** on text blocks, reducing inputs to **36%–42% of their original size**:

* **5.4 KB Text Clip:** Compresses to **2,286 bytes** (42.3%) vs. Zstd's 2,595 bytes (48.0%).
* **4.8 KB Text Clip:** Compresses to **2,072 bytes** (42.6%) vs. Zstd's 2,499 bytes (51.4%).
* **16 KB Document:** Compresses down to **5.7 KB**.

When loaded as an in-memory shared library (`moezip.dll`) or native Python module (`moezip.pyd`), the pre-trained vocabulary remains resident in RAM, executing full compression passes in **< 3 milliseconds**!

---

## Compilation & Building

### Prerequisites
* A C++20 compatible compiler (MSVC on Windows, GCC 10+, or Clang 11+)
* CMake (3.16 or higher)
* Python 3.x

---

### Step 1: Generate Embedded C++ Assets
Run `make.py` first. This parses `words_final.txt` and `router_stateless_v4.json`, chunking them into `embedded_assets.hpp` to bypass compiler string literal limits:

```bash
python make.py
```

---

### Step 2: Select Your Compilation Path

Choose one of the two compilation workflows depending on your requirements:

---

### 🔹 Option 1: Standard Build (Zero Python Dependencies)
*Use this option if you want the standalone CLI tool (`moezip.exe`) and the C-API Shared Library (`moezip.dll`) for fast Python `ctypes` integration with zero external dependencies.*

**Windows (MSVC):**
```cmd
cmake -B build
cmake --build build --config Release
```

**Linux / macOS:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**Generated Binaries in `build/Release/`:**
* **`moezip.exe`** (Standalone CLI Executable)
* **`moezip.dll` / `libmoezip.so`** (Shared Library for C-API / `ctypes`)

---

### 🔹 Option 2: PyBind11 Build (Native `import moezip` Extension)
*Use this option if you want to compile a native C++ Python module (`moezip.pyd` / `moezip.so`) for direct `import moezip` support in Python.*

**1. Install PyBind11:**
```cmd
pip install pybind11
```

**2. Configure CMake with PyBind11 & Python Paths:**

*Automated Path Detection (Windows CMD):*
```cmd
for /f "delims=" %i in ('python -c "import pybind11; print(pybind11.get_cmake_dir())"') do for /f "delims=" %j in ('python -c "import sys; print(sys.executable)"') do cmake -B build -Dpybind11_DIR="%i" -DPYTHON_EXECUTABLE="%j" -DPython_EXECUTABLE="%j"
```

*Or Manual Explicit Path Passing:*
```cmd
cmake -B build -Dpybind11_DIR="C:\Users\<YourUser>\AppData\Local\Programs\Python\Python311\Lib\site-packages\pybind11\share\cmake\pybind11" -DPYTHON_EXECUTABLE="C:\Users\<YourUser>\AppData\Local\Programs\Python\Python311\python.exe"
```

**3. Compile Release Binaries:**
```cmd
cmake --build build --config Release
```

**Generated Binaries in `build/Release/`:**
* **`moezip.exe`** (Standalone CLI Executable)
* **`moezip.dll`** (C-API Shared Library)
* **`moezip.pyd`** (Native Python Extension Module for `import moezip`)

---

## Embedded Assets & Portability

`moezip` is **100% portable and self-contained**. 

During `python make.py`, your vocabulary (`words_final.txt`) and transition matrix (`router_stateless_v4.json`) are chunked into C++ string literals (`embedded_assets.hpp`) and compiled directly into `moezip.exe`, `moezip.dll`, and `moezip.pyd`.

* **Zero File Dependencies:** You do **NOT** need `words_final.txt` or `router_stateless_v4.json` on disk to run `moezip`. You can distribute the compiled binaries to any folder or machine.
* **Optional Disk Override:** If you wish to test a new vocabulary or domain-trained matrix without recompiling, place a `words_final.txt` or `router_stateless_v4.json` file in the execution directory. `moezip` will detect disk files and override its embedded defaults automatically.

---

## Usage

### 1. Command Line Interface (CLI)

*Note: On Windows CMD, run the executable as `build\Release\moezip.exe`. On Unix, use `./moezip`.*

**Compress a text file:**
```bash
build\Release\moezip.exe compress input.txt -o archive.moe
```

**Decompress an archive:**
```bash
build\Release\moezip.exe decompress archive.moe -o restored.txt
```

**Decompress directly from a Hex String:**
```bash
build\Release\moezip.exe hexdec 880A090A20202020... -o restored.txt
```

**Train a custom domain router matrix:**
Optimize the compressor for specialized fields (e.g., medical, legal, or code) by feeding it a text corpus. This generates a new `router_stateless_v4.json` file. Re-run `python make.py` and recompile to embed the new weights:
```bash
build\Release\moezip.exe train --corpus path/to/dataset.txt
```

---

### 2. Native Python Extension Module (`import moezip`)

If built with Option 2, copy `moezip.pyd` (Windows) or `moezip.so` (Unix) from `build/Release/` into your Python project directory:

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

### 3. Shared Library Integration (`ctypes` + `moezip.dll`)

If built with Option 1 (or Option 2), use Python's built-in `ctypes` to interface directly with `moezip.dll` in memory:

```python
import os
import ctypes

# Load DLL
dll_path = os.path.abspath("build/Release/moezip.dll")
moezip = ctypes.CDLL(dll_path)

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