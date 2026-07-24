#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "vocab.hpp"
#include "codec.hpp"
#include "router.hpp"

namespace py = pybind11;
namespace fs = std::filesystem;

static VocabData g_vd;
static TransitionMatrix g_matrix;
static bool g_initialized = false;

// Helper RAII class to temporarily suppress std::cerr console messages
struct SilenceStderr {
    std::streambuf* old_buf;
    std::stringstream null_buf;
    SilenceStderr() : old_buf(std::cerr.rdbuf(null_buf.rdbuf())) {}
    ~SilenceStderr() { std::cerr.rdbuf(old_buf); }
};

static void py_init(const std::string& vocab_path = "", const std::string& router_path = "", bool quiet = true) {
    if (quiet) {
        SilenceStderr silence; // Redirect std::cerr during asset loading
        g_vd = load_and_partition_wordlist(vocab_path);
        g_matrix = load_router(router_path, g_vd.expert_count);
    } else {
        g_vd = load_and_partition_wordlist(vocab_path);
        g_matrix = load_router(router_path, g_vd.expert_count);
    }
    g_initialized = true;
}

static void ensure_initialized() {
    if (!g_initialized) {
        py_init("", "", true); // Default quiet auto-initialization
    }
}

static py::bytes py_compress(const std::string& text) {
    ensure_initialized();
    auto packed = compress_adaptive_moe(text, g_vd.vocab_to_idx, g_matrix, g_vd.expert_count, g_vd.expert_size);
    return py::bytes(reinterpret_cast<const char*>(packed.data()), packed.size());
}

static std::string py_decompress(const std::string& packed_bytes) {
    ensure_initialized();
    std::vector<uint8_t> vec(packed_bytes.begin(), packed_bytes.end());
    return decompress_adaptive_moe(vec, g_vd.vocab, g_vd.vocab_to_idx, g_matrix, g_vd.expert_count, g_vd.expert_size);
}

static void py_train(const std::string& corpus_or_path, const std::string& output_filepath = "router_stateless_v4.json", bool quiet = false) {
    ensure_initialized();

    std::string corpus_text;
    if (fs::exists(corpus_or_path)) {
        std::ifstream fin(corpus_or_path, std::ios::binary);
        if (!fin.is_open()) {
            throw std::runtime_error("Cannot open corpus file: " + corpus_or_path);
        }
        corpus_text = std::string((std::istreambuf_iterator<char>(fin)),
                                   std::istreambuf_iterator<char>());
    } else {
        corpus_text = corpus_or_path;
    }

    if (quiet) {
        SilenceStderr silence;
        g_matrix = train_router_on_corpus(corpus_text, g_vd.vocab_to_idx, g_vd.expert_count, g_vd.expert_size, output_filepath);
    } else {
        g_matrix = train_router_on_corpus(corpus_text, g_vd.vocab_to_idx, g_vd.expert_count, g_vd.expert_size, output_filepath);
    }
}

PYBIND11_MODULE(moezip, m) {
    m.doc() = "moezip Python extension module";

    m.def("init", &py_init, 
          py::arg("vocab_path") = "", 
          py::arg("router_path") = "", 
          py::arg("quiet") = true,
          "Initializes or re-loads moezip engine with optional custom asset paths.");

    m.def("compress", &py_compress, py::arg("text"), "Compresses a string into moezip bytes.");
    m.def("decompress", &py_decompress, py::arg("packed_bytes"), "Decompresses moezip bytes back into a string.");

    m.def("train", &py_train, 
          py::arg("corpus_or_path"), 
          py::arg("output_filepath") = "router_stateless_v4.json", 
          py::arg("quiet") = false,
          "Trains the router state matrix on a text string or corpus file.");
}
