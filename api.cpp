#include "vocab.hpp"
#include "codec.hpp"
#include "router.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

static VocabData g_vd;
static TransitionMatrix g_matrix;
static bool g_initialized = false;

extern "C" {

    EXPORT void init_engine(const char* vocab_path, const char* router_path) {
        if (!g_initialized) {
            std::string v_path = (vocab_path && vocab_path[0] != '\0') ? vocab_path : "words_final.txt";
            std::string r_path = (router_path && router_path[0] != '\0') ? router_path : "router_stateless_v4.json";
            
            g_vd = load_and_partition_wordlist(v_path);
            g_matrix = load_router(r_path, g_vd.expert_count);
            g_initialized = true;
        }
    }

    EXPORT int compress_text(const char* text, uint8_t* out_buf, int max_out_len) {
        if (!g_initialized) init_engine(nullptr, nullptr);

        std::string input_str(text);
        auto packed = compress_adaptive_moe(input_str, g_vd.vocab_to_idx, g_matrix, g_vd.expert_count, g_vd.expert_size);

        if ((int)packed.size() > max_out_len) return -1;
        std::memcpy(out_buf, packed.data(), packed.size());
        return (int)packed.size();
    }

    EXPORT int decompress_bytes(const uint8_t* in_buf, int in_len, char* out_buf, int max_out_len) {
        if (!g_initialized) init_engine(nullptr, nullptr);

        std::vector<uint8_t> packed(in_buf, in_buf + in_len);
        std::string text = decompress_adaptive_moe(packed, g_vd.vocab, g_vd.vocab_to_idx, g_matrix, g_vd.expert_count, g_vd.expert_size);

        if ((int)text.size() >= max_out_len) return -1;
        std::memcpy(out_buf, text.c_str(), text.size() + 1);
        out_buf[text.size()] = '\0';
        return (int)text.size();
    }
}
