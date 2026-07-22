# moezip 🗜️

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)

A learning-augmented, high-efficiency text compressor written in C++20. `moezip` combines a Mixture of Experts (MoE) predictive state router, Asymmetric Numeral Systems (rANS) arithmetic coding, and LZ-style back-referencing to compress natural language text.

Unlike standard dictionary-less compression algorithms (like Gzip or raw Zstd) that suffer from a "warm-up penalty" on short inputs, `moezip` embeds a pre-trained Markov state router and vocabulary directly into the compiled binary or DLL. This enables instant prediction of text patterns with zero metadata overhead.

## Performance & Benchmarks

In real-world benchmarks on everyday text snippets (4 KB – 15 KB), `moezip` consistently outperforms industry-standard algorithms like Facebook's **Zstandard (Zstd)** on text blocks, reducing inputs to **36%–42% of their original size**:

* **5.4 KB Text Clip:** Compresses to **2,286 bytes** (42.3%) vs. Zstd's 2,595 bytes (48.0%).
* **4.8 KB Text Clip:** Compresses to **2,072 bytes** (42.6%) vs. Zstd's 2,499 bytes (51.4%).
* **16 KB Document:** Compresses down to **5.7 KB**.

When loaded as an in-memory shared library (`moezip.dll`), the pre-trained vocabulary remains resident in RAM, executing full compression passes in **< 3 milliseconds**!

## Getting Started

### Prerequisites
* A C++20 compatible compiler (MSVC, GCC, or Clang)
* CMake (3.16 or higher)
* Python 3.x (to generate embedded asset headers)

### Build Instructions

First, run the included Python script. This parses your vocabulary and router state, chunking them into `embedded_assets.hpp` to bypass compiler string literal limits:

```bash
python make.py
```

Then configure and compile both the standalone CLI executable (`moezip.exe`) and the shared library (`moezip.dll` / `libmoezip.so`):

```bash
# Configure build directory
cmake -B build

# Compile Release binaries
cmake --build build --config Release
```

Your compiled files will be located in `build/Release/`:
* **`moezip.exe`** (Standalone Command Line Executable)
* **`moezip.dll` / `libmoezip.so`** (Shared Library for C-API / Python Bindings)

## Embedded Assets & Portability

`moezip` is **100% portable and self-contained**. 

During `python make.py`, your vocabulary (`words_final.txt`) and transition matrix (`router_stateless_v4.json`) are chunked into C++ string literals (`embedded_assets.hpp`) and compiled directly into `moezip.exe` and `moezip.dll`.

### Zero File Dependencies
You do **NOT** need `words_final.txt` or `router_stateless_v4.json` to run `moezip`. You can distribute `moezip.dll` or `moezip.exe` as a single standalone file to any folder or machine.

### Optional Disk Override
If you wish to test a new vocabulary or domain-trained matrix without recompiling the binary, simply place a `words_final.txt` or `router_stateless_v4.json` file in the execution directory. `moezip` will detect the disk files and override its embedded defaults. If no files are found on disk, it silently falls back to its embedded memory assets.

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

### 2. High-Speed In-Memory Python Integration (`ctypes` + `moezip.dll`)

For maximum performance in Python (e.g., real-time clipboard managers, database row archivers, or API middleware), load `moezip.dll` directly using Python's built-in `ctypes`. 

This pre-loads the 63,000-word vocabulary into RAM **once on startup**, allowing subsequent compression calls to execute in **~1–3 milliseconds** without subprocess spawning overhead.

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

# 1. Pre-load embedded vocabulary in RAM at app startup
# We pass empty strings b"" to load the assets directly from DLL memory!
moezip.init_engine(b"", b"")

# 2. In-Memory Compression Function (Sub-millisecond speed)
def compress(text: str) -> bytes:
    raw_bytes = text.encode('utf-8')
    buf_size = len(raw_bytes) * 2 + 2048
    out_buf = (ctypes.c_uint8 * buf_size)()
    
    comp_len = moezip.compress_text(raw_bytes, out_buf, buf_size)
    if comp_len <= 0: raise RuntimeError("Compression error")
    return bytes(out_buf[:comp_len])

# 3. In-Memory Decompression Function
def decompress(packed_bytes: bytes) -> str:
    in_array = (ctypes.c_uint8 * len(packed_bytes)).from_buffer_copy(packed_bytes)
    out_buf = ctypes.create_string_buffer(len(packed_bytes) * 10 + 4096)
    
    decomp_len = moezip.decompress_bytes(in_array, len(packed_bytes), out_buf, len(out_buf))
    if decomp_len <= 0: raise RuntimeError("Decompression error")
    return out_buf.value.decode('utf-8')

# Example Usage
original_text = "Here is a test string to compress in memory."
compressed_bytes = compress(original_text)
restored_text = decompress(compressed_bytes)

print(f"Original Size   : {len(original_text.encode('utf-8'))} bytes")
print(f"Compressed Size : {len(compressed_bytes)} bytes")
print(f"Lossless Match  : {original_text == restored_text}")
```

---

### 3. Piping & Subprocess Fallback (`stdin` / `stdout`)

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