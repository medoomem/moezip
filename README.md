# moezip

[![PyPI version](https://img.shields.io/pypi/v/moezip.svg)](https://pypi.org/project/moezip/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Android-lightgrey.svg)](https://isocpp.org/)

A learning-augmented, high-efficiency text compression engine written in C++20. `moezip` combines a Mixture of Experts (MoE) predictive state router, 32-bit Asymmetric Numeral Systems (rANS) arithmetic coding, and LZ back-referencing.

Standard dictionary-less compressors (like raw Zstandard or Gzip) incur frame-header inflation on short inputs, often expanding small text strings. `moezip` embeds pre-trained vocabulary tables and transition matrices directly into the compiled library or executable, enabling instant pattern prediction with zero metadata overhead.

---

## Installation

Install directly from [PyPI](https://pypi.org/project/moezip/):

```bash
pip install moezip
```

*Pre-compiled binary wheels are published for Windows (x64), Linux (x86_64 & ARM64/aarch64 for Android / Pydroid 3 / Termux), and macOS (Intel & Apple Silicon).*

---

## Documentation

Full technical documentation, build guides, and API specifications are hosted on the **[moezip GitHub Wiki](https://github.com/medoomem/moezip/wiki)**:

* **[Architecture & Design](https://github.com/medoomem/moezip/wiki/Architecture-and-Design):** Internal pipeline, tokenization, 32x32 MoE router, and `make.py` string-literal generator.
* **[Building & Compilation](https://github.com/medoomem/moezip/wiki/Building-and-Compilation):** CMake and PyPI build guides for Windows (MSVC), Linux (GCC/Clang), and GitHub Actions CI.
* **[CLI Usage & Training](https://github.com/medoomem/moezip/wiki/CLI-Usage-and-Training):** Command reference, stream piping, and training domain-specific matrices.
* **[API Reference](https://github.com/medoomem/moezip/wiki/API-Reference):** Documentation for Python (`import moezip`), C-API dynamic libraries, and native C++ headers.
* **[Performance & Benchmarks](https://github.com/medoomem/moezip/wiki/Performance-and-Benchmarks):** Detailed space savings and speed metrics across micro-strings (< 500 B) and medium documents.

---

## Key Highlights

* **Zero Warm-Up Penalty:** Compresses small text payloads and chat messages (< 500 bytes) without header bloat.
* **Self-Contained Portability:** Default vocabulary (`words_final.txt`) and router matrix (`router_stateless_v4.json`) compile directly into the binary. No external asset files are required at runtime.
* **First-Class Python Integration:** Native IDE autocomplete (`.pyi` type stubs) and quiet background initialization by default.
* **Multi-Target Compilation:** Single CMake or PyPI build produces standalone CLI executables, C-API dynamic libraries (`.dll` / `.so`), or native PyBind11 Python extension modules (`.pyd` / `.so`).
* **Domain Trainable:** Train state transition matrices directly in Python (`moezip.train()`) or CLI (`moezip train`) for specialized corpora (code, medical, legal).

---

## Quick Start

### Python Extension (`import moezip`)

```python
import moezip

# Compress and decompress strings in memory
compressed = moezip.compress("Hello world, compress this text!")
restored = moezip.decompress(compressed)

assert restored == "Hello world, compress this text!"

# Train router transition matrix on custom domain text
moezip.train("Sample domain text for matrix optimization...", output_filepath="custom_router.json")
```

### Command Line Interface

```bash
# Compress text file
moezip compress input.txt -o archive.moe

# Decompress back to original text
moezip decompress archive.moe -o restored.txt
```

---

## Performance Snapshot

Across 532 cumulative bytes of micro-string test samples:
* **`moezip`:** Reduced size to **247 bytes** (**53.6% space savings**).
* **Raw Zstandard (L3):** Reduced size to **456 bytes** (**14.3% space savings**).

For detailed test data and medium-clip benchmarks, see the **[Performance & Benchmarks Wiki](https://github.com/medoomem/moezip/wiki/Performance-and-Benchmarks)** page.

---

## Compiled Artifacts & Platform Matrix

| Platform | Architecture | Executable | Shared Library (C-API) | Python Module |
| :--- | :--- | :--- | :--- | :--- |
| **Windows** | x86_64 | `moezip.exe` | `moezip.dll` / `moezip.lib` | `moezip.pyd` |
| **Linux / Android** | x86_64, aarch64 (Pydroid 3 / Termux) | `moezip` | `libmoezip.so` | `moezip.so` |
| **macOS** | x86_64, arm64 (Apple Silicon) | `moezip` | `libmoezip.so` | `moezip.so` |

---

## License

This project is licensed under the [MIT License](https://github.com/medoomem/moezip/blob/main/LICENSE).