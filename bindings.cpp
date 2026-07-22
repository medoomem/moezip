#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "vocab.hpp"
#include "codec.hpp"
#include "router.hpp"

namespace py = pybind11;

static VocabData g_vd;
static TransitionMatrix g_matrix;
static bool g_initialized = false;

static void init_engine() {
    if (!g_initialized) {
        g_vd = load_and_partition_wordlist("");
        g_matrix = load_router("", g_vd.expert_count);
        g_initialized = true;
    }
}

static py::bytes py_compress(const std::string& text) {
    init_engine();
    auto packed = compress_adaptive_moe(text, g_vd.vocab_to_idx, g_matrix, g_vd.expert_count, g_vd.expert_size);
    return py::bytes(reinterpret_cast<const char*>(packed.data()), packed.size());
}

static std::string py_decompress(const std::string& packed_bytes) {
    init_engine();
    std::vector<uint8_t> vec(packed_bytes.begin(), packed_bytes.end());
    return decompress_adaptive_moe(vec, g_vd.vocab, g_vd.vocab_to_idx, g_matrix, g_vd.expert_count, g_vd.expert_size);
}

PYBIND11_MODULE(moezip, m) {
    m.doc() = "moezip Python extension module";
    m.def("compress", &py_compress, "Compresses a string into moezip bytes");
    m.def("decompress", &py_decompress, "Decompresses moezip bytes back into a string");
}
