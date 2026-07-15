# moezip 🗜️

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)

A learning-augmented text compressor written in C++20. `moezip` combines a Mixture of Experts (MoE) predictive router, Asymmetric Numeral Systems (rANS) arithmetic coding, and LZ-style back-referencing to compress natural language text. 

Unlike standard dictionary-less compression algorithms that suffer from a "warm-up penalty" on short inputs, `moezip` embeds a pre-trained Markov state router directly into the compiled binary. This enables instant prediction of text patterns with zero metadata overhead.

## Performance & Target Use Cases

In standard benchmarks, `moezip` consistently reduces English text to **36%–51% of its original size** (a ~2.5x to 2.7x compression ratio). 
* A 57-byte string compresses down to 21 bytes.
* A 16 KB document compresses down to 5.7 KB.

Because it relies on a pre-trained vocabulary and transition matrix, it is highly specialized for text. It excels in:
* **Short-String Compression:** Database rows, JSON payloads, or network packets where standard LZ/Huffman implementations fail due to header overhead.
* **Domain-Specific NLP Datasets:** Archiving massive text corpora in specialized fields (e.g., legal, medical, or financial) by pre-training the router on a representative sampleSample.
* **Log Files & Telemetry:** Handling highly repetitive boilerplate through its hybrid LZ-matching fallback.

## Getting Started

### Prerequisites
* A C++20 compatible compiler (MSVC, GCC, or Clang)
* CMake (3.16 or higher)
* Python 3.x (to generate the embedded assets header)

### Build Instructions

First, run the included Python script. This parses your vocabulary and router state and chunks them into `embedded_assets.hpp` to bypass compiler literal limits:

```bash
python make.py
```

Then configure and compile the binary with CMake:

```bash
# Configure the build files
cmake .

# Compile the executable (Windows/MSVC)
cmake --build . --config Release
```

Your compiled binary will be located in `Release\moezip.exe` (Windows) or the root directory (Unix).

## Usage

*Note: On Windows CMD, run the executable as `Release\moezip.exe`. On Unix, use `./moezip`.*

**Compress a file:**
```bash
Release\moezip.exe compress input.txt -o archive.moe
```

**Decompress a file:**
```bash
Release\moezip.exe decompress archive.moe -o restored.txt
```

**Train a new router matrix:**
If you want to optimize the compressor for a specific domain, feed it a large text corpus. This generates a new `router_stateless_v4.json` file. You will need to re-run `python make.py` and recompile to embed the new matrix into the binary.
```bash
Release\moezip.exe train --corpus path/to/dataset.txt
```

## Piping and Subprocesses (stdin / stdout)

`moezip` supports standard I/O pipes. On Windows, standard streams are put into binary mode to prevent OS corruption (such as translating `0x0A` to `0x0D 0x0A`), enabling pure binary piping.

Diagnostic logging and warning messages are sent to `stderr`, leaving `stdout` completely clean for binary payload transfer.

**Pipe Example:**
```bash
(echo | set /p="Hello there, compress this string.")| moezip.exe compress - -o - > archive.moe
```

**Python Subprocess Example:**
You can wrap the compiled executable directly in Python to compress strings in memory without touching the disk and without any hexadecimal size overhead.

```python
import subprocess

# Windows path to your compiled executable
EXE_PATH = r"Release\moezip.exe"

# The target text to compress
text_data = "Biography of Shah Rukh Khan Often referred to in the media as a Baadshah of Bollywood."
original_bytes = text_data.encode('utf-8')
original_size = len(original_bytes)

print(f"Original Text: '{text_data}'")
print(f"Original Size: {original_size} bytes\n")


# ==========================================
# 1. COMPRESSION (In-Memory Pipe)
# ==========================================
# We pass '-' for both input and output to tell moezip to use stdin/stdout.
# 'text=False' ensures Python treats stdin and stdout as raw binary bytes.
compress_proc = subprocess.run(
    [EXE_PATH, "compress", "-", "-o", "-", "-q"],
    input=original_bytes,  # Send raw UTF-8 bytes directly to stdin
    capture_output=True,   # Capture stdout (data) and stderr (errors/warnings)
    text=False
)

if compress_proc.returncode != 0:
    print(f"Compression Failed: {compress_proc.stderr.decode('utf-8')}")
    exit(1)

# Capture the exact, uninflated compressed binary bytes from stdout
compressed_bytes = compress_proc.stdout
compressed_size = len(compressed_bytes)

print(f"--- Compression Results ---")
print(f"Compressed Binary: {compressed_bytes.hex().upper()[:30]}... (truncated)")
print(f"Compressed Size  : {compressed_size} bytes")
print(f"True Ratio       : {((compressed_size / original_size) * 100):.1f}%\n")


# ==========================================
# 2. DECOMPRESSION (In-Memory Pipe)
# ==========================================
# We pipe the exact compressed binary bytes back into stdin.
decompress_proc = subprocess.run(
    [EXE_PATH, "decompress", "-", "-o", "-", "-q"],
    input=compressed_bytes,  # Send raw compressed bytes directly to stdin
    capture_output=True,     # Capture stdout (restored data)
    text=False
)

if decompress_proc.returncode != 0:
    print(f"Decompression Failed: {decompress_proc.stderr.decode('utf-8')}")
    exit(1)

# Decode the clean stdout bytes back to a UTF-8 string
restored_text = decompress_proc.stdout.decode('utf-8')

print(f"--- Decompression Results ---")
print(f"Restored Text: '{restored_text}'")
print(f"Lossless Verification: {text_data == restored_text}")
```

## Contributing

PRs are welcome. `moezip` is currently a functional prototype, and there is significant room for C++ specific performance optimizations. If you are looking for an area to contribute, please consider:

* **Memory Allocation Profiling:** The `LZCache` currently uses `std::unordered_map<std::string, std::deque<int>>`, which triggers heavy heap allocations on large files. Replacing this with a flat integer hash-chain array (similar to zlib) will speed up the LZ matching phase.
* **Zero-Copy Tokenization:** Refactoring the `std::string` manipulations in `tokenizer.cpp` to use `std::string_view` to reduce temporary allocations during the encoder loop.
* **rANS Renormalization Tuning:** Experimenting with the 32-bit rANS renormalization thresholds to maximize CPU throughput. 

## License

This project is licensed under the MIT License - see the `LICENSE` file for details.
