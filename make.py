import os
import json

def chunk_string(content, chunk_size=16000):
    """Slices a string into chunks to stay safely below MSVC's 65,535 character limit per literal."""
    return [content[i:i+chunk_size] for i in range(0, len(content), chunk_size)]

def get_chunked_cpp_array(name, content, delimiter):
    chunks = chunk_string(content)
    cpp_code = f"inline const char* const {name}_CHUNKS[] = {{\n"
    for chunk in chunks:
        cpp_code += f'    R"{delimiter}({chunk}){delimiter}",\n'
    cpp_code += f"}};\n"
    cpp_code += f"inline const size_t {name}_CHUNKS_COUNT = {len(chunks)};\n"
    return cpp_code

# 1. Read and process words_final.txt
vocab_content = ""
if os.path.exists("words_final.txt"):
    with open("words_final.txt", "r", encoding="utf-8") as f:
        vocab_content = f.read()
    print("Found 'words_final.txt'. Chunking and embedding it...")
else:
    vocab_content = "hello\\nthere\\n"
    print("[INFO] words_final.txt not found. Using default fallback vocabulary.")

# 2. Read and process router_stateless_v4.json
router_content = ""
if os.path.exists("router_stateless_v4.json"):
    with open("router_stateless_v4.json", "r", encoding="utf-8") as f:
        router_content = f.read()
    print("Found 'router_stateless_v4.json'. Chunking and embedding it...")
else:
    matrix_1s = [[1]*32 for _ in range(32)]
    router_content = json.dumps(matrix_1s)
    print("[INFO] router_stateless_v4.json not found. Using default fallback router matrix.")

# Generate chunked C++ array strings
vocab_array_cpp = get_chunked_cpp_array("EMBEDDED_VOCAB", vocab_content, "EMBED_VOCAB")
router_array_cpp = get_chunked_cpp_array("EMBEDDED_ROUTER", router_content, "EMBED_ROUTER")

# Files map to generate
files = {
    "embedded_assets.hpp": f"""#pragma once
#include <cstddef>

{vocab_array_cpp}

{router_array_cpp}
""",

    "vocab.hpp": """#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct VocabData {
    std::vector<std::string> vocab;
    std::unordered_map<std::string, int> vocab_to_idx;
    int expert_count;
    int expert_size;
};

VocabData load_and_partition_wordlist(const std::string& filepath = "words_final.txt");
""",

    "vocab.cpp": """#include "vocab.hpp"
#include "embedded_assets.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_set>

static std::vector<std::string> COMMON_AFFIXES = {
    "ing","ed","ly","tion","ment","ness","able","ible","ous","ist",
    "ism","ity","ers","est","pre","pro","dis","mis","un","re"
};

static std::vector<std::string> COMMON_PHRASES = {
    "of the","in the","to the","on the","and the","for the",
    "to be","it is","that the","with the","at the","by the",
    "from the","this is","will be","he is","she is",
    "shah rukh","rukh khan","jackie chan","new york","united states",
    "one of","out of","as a","in a","for a","to a","with a",
    "of his","in his","to his","and his","for his",
    "he has","has been","have been"
};

static std::vector<std::string> COMMON_WORDS = {
    "the","be","to","of","and","a","in","that","have","i","it","for",
    "not","on","with","he","as","you","do","at","this","but","his","by",
    "from","they","we","say","her","she","or","an","will","my","one",
    "all","would","there","their","what"
};

static std::string to_lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::tolower(c); });
    return r;
}

VocabData load_and_partition_wordlist(const std::string& filepath) {
    std::vector<std::string> raw_words;
    std::unordered_set<std::string> seen;
    std::string line;

    std::ifstream fin(filepath);
    if (!fin.is_open()) {
        std::cerr << "'" << filepath << "' not found on disk. Loading embedded vocabulary chunks from binary...\\n";
        std::stringstream ssin;
        for (size_t i = 0; i < EMBEDDED_VOCAB_CHUNKS_COUNT; ++i) {
            ssin << EMBEDDED_VOCAB_CHUNKS[i];
        }
        while (std::getline(ssin, line)) {
            while (!line.empty() && (line.back()=='\\r'||line.back()=='\\n'||line.back()==' '))
                line.pop_back();
            while (!line.empty() && (line.front()==' '))
                line.erase(line.begin());
            if (line.empty()) continue;
            std::string lw = to_lower(line);
            if (!seen.count(lw)) {
                seen.insert(lw);
                raw_words.push_back(lw);
            }
        }
    } else {
        while (std::getline(fin, line)) {
            while (!line.empty() && (line.back()=='\\r'||line.back()=='\\n'||line.back()==' '))
                line.pop_back();
            while (!line.empty() && (line.front()==' '))
                line.erase(line.begin());
            if (line.empty()) continue;
            std::string lw = to_lower(line);
            if (!seen.count(lw)) {
                seen.insert(lw);
                raw_words.push_back(lw);
            }
        }
        fin.close();
    }

    // Append affixes and phrases not already present
    for (const auto& a : COMMON_AFFIXES) {
        if (!seen.count(a)) { seen.insert(a); raw_words.push_back(a); }
    }
    for (const auto& p : COMMON_PHRASES) {
        if (!seen.count(p)) { seen.insert(p); raw_words.push_back(p); }
    }

    int total_words = (int)raw_words.size();
    std::cerr << "Loaded " << total_words << " unique tokens (including N-Grams).\\n";

    std::unordered_set<std::string> priority_set;
    for (const auto& p : COMMON_PHRASES) priority_set.insert(p);
    for (const auto& w : COMMON_WORDS)   priority_set.insert(w);
    for (const auto& a : COMMON_AFFIXES) priority_set.insert(a);

    std::vector<std::string> sorted_vocab;
    for (const auto& p : COMMON_PHRASES) {
        if (seen.count(p)) sorted_vocab.push_back(p);
    }
    for (const auto& w : COMMON_WORDS) {
        if (seen.count(w) && priority_set.count(w)) sorted_vocab.push_back(w);
    }
    for (const auto& a : COMMON_AFFIXES) {
        if (seen.count(a)) sorted_vocab.push_back(a);
    }
    // Deduplicate priority list
    std::unordered_set<std::string> already;
    {
        std::vector<std::string> deduped;
        for (const auto& w : sorted_vocab) {
            if (!already.count(w)) { already.insert(w); deduped.push_back(w); }
        }
        sorted_vocab = deduped;
    }
    // Then the rest
    for (const auto& w : raw_words) {
        if (!already.count(w)) {
            already.insert(w);
            sorted_vocab.push_back(w);
        }
    }

    const int expert_count = 32;
    int expert_size = (total_words + expert_count - 1) / expert_count;

    std::cerr << "Experts: " << expert_count
              << " | Capacity/Exp: " << expert_size << "\\n";

    int padded_size = expert_count * expert_size;
    std::vector<std::string> vocab_padded(padded_size);
    for (int i = 0; i < (int)sorted_vocab.size(); ++i)
        vocab_padded[i] = sorted_vocab[i];
    for (int i = (int)sorted_vocab.size(); i < padded_size; ++i)
        vocab_padded[i] = "padding" + std::to_string(i);

    std::unordered_map<std::string, int> word_to_global_idx;
    word_to_global_idx.reserve(padded_size);
    for (int i = 0; i < padded_size; ++i)
        word_to_global_idx[vocab_padded[i]] = i;

    VocabData vd;
    vd.vocab = std::move(vocab_padded);
    vd.vocab_to_idx = std::move(word_to_global_idx);
    vd.expert_count = expert_count;
    vd.expert_size = expert_size;
    return vd;
}
""",

    "tokenizer.hpp": """#pragma once
#include <string>
#include <vector>
#include <unordered_map>

using Token = std::pair<std::string, std::string>;

std::pair<std::string, std::vector<Token>> raw_tokenize(const std::string& text);
std::pair<std::string, std::vector<Token>> tokenize(const std::string& text, const std::unordered_map<std::string, int>& vocab_to_idx);
std::string detokenize(const std::string& leading_ws, const std::vector<Token>& tokens);
int get_casing(const std::string& w);
int get_actual_expert(const std::string& w, const std::unordered_map<std::string, int>& vocab_to_idx, int expert_size);
std::string to_lower(const std::string& s);
std::string to_upper(const std::string& s);
std::string capitalize(const std::string& s);
std::string to_title(const std::string& s);
bool is_all_nonword(const std::string& s);
""",

    "tokenizer.cpp": """#include "tokenizer.hpp"
#include <regex>
#include <algorithm>
#include <cctype>

std::string to_lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::tolower(c); });
    return r;
}

std::string to_upper(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::toupper(c); });
    return r;
}

std::string capitalize(const std::string& s) {
    if (s.empty()) return s;
    std::string r = to_lower(s);
    r[0] = (char)::toupper((unsigned char)r[0]);
    return r;
}

std::string to_title(const std::string& s) {
    std::string r = s;
    bool cap_next = true;
    for (char& c : r) {
        if (std::isspace((unsigned char)c)) { cap_next = true; }
        else if (cap_next) { c = (char)::toupper((unsigned char)c); cap_next = false; }
        else { c = (char)::tolower((unsigned char)c); }
    }
    return r;
}

// ... [Remainder of tokenizer.cpp remains unchanged] ...

bool is_all_nonword(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (std::isalnum((unsigned char)c) || c == '_') return false;
    }
    return true;
}

std::pair<std::string, std::vector<Token>> raw_tokenize(const std::string& text) {
    std::string leading_ws;
    std::string rest = text;

    size_t i = 0;
    while (i < rest.size() && std::isspace((unsigned char)rest[i])) ++i;
    leading_ws = rest.substr(0, i);
    rest = rest.substr(i);

    std::vector<Token> tokens;
    size_t pos = 0;
    while (pos < rest.size()) {
        std::string word, spacing;

        if (std::isalnum((unsigned char)rest[pos]) || rest[pos] == '_') {
            size_t start = pos;
            while (pos < rest.size() && (std::isalnum((unsigned char)rest[pos]) || rest[pos] == '_'))
                ++pos;
            word = rest.substr(start, pos - start);
        } else if (!std::isspace((unsigned char)rest[pos])) {
            size_t start = pos;
            while (pos < rest.size() && !std::isalnum((unsigned char)rest[pos]) && rest[pos] != '_' && !std::isspace((unsigned char)rest[pos]))
                ++pos;
            word = rest.substr(start, pos - start);
        } else {
            ++pos;
            continue;
        }

        size_t sp_start = pos;
        while (pos < rest.size() && std::isspace((unsigned char)rest[pos]))
            ++pos;
        spacing = rest.substr(sp_start, pos - sp_start);

        tokens.push_back({word, spacing});
    }
    return {leading_ws, tokens};
}

std::pair<std::string, std::vector<Token>> tokenize(const std::string& text, const std::unordered_map<std::string, int>& vocab_to_idx) {
    auto [leading_ws, raw_tokens] = raw_tokenize(text);

    std::vector<Token> phrased_tokens;
    int n = (int)raw_tokens.size();
    int idx = 0;
    while (idx < n) {
        bool matched = false;
        for (int length : {3, 2}) {
            if (idx + length <= n) {
                bool valid_phrase = true;
                for (int k = 0; k < length - 1; ++k) {
                    if (raw_tokens[idx + k].second != " ") {
                        valid_phrase = false;
                        break;
                    }
                }
                if (valid_phrase) {
                    std::string phrase_str;
                    for (int k = 0; k < length; ++k) {
                        if (k > 0) phrase_str += " ";
                        phrase_str += raw_tokens[idx + k].first;
                    }
                    std::string phrase_lower = to_lower(phrase_str);
                    if (vocab_to_idx.count(phrase_lower)) {
                        std::string trailing_sp = raw_tokens[idx + length - 1].second;
                        phrased_tokens.push_back({phrase_str, trailing_sp});
                        idx += length;
                        matched = true;
                        break;
                    }
                }
            }
        }
        if (!matched) {
            phrased_tokens.push_back(raw_tokens[idx]);
            ++idx;
        }
    }

    std::vector<Token> refined_tokens;
    for (auto& [w, sp] : phrased_tokens) {
        std::string w_lower = to_lower(w);

        bool skip_split = vocab_to_idx.count(w_lower) > 0
                       || w.size() < 4
                       || is_all_nonword(w)
                       || w.find(' ') != std::string::npos;

        if (skip_split) {
            refined_tokens.push_back({w, sp});
            continue;
        }

        std::vector<std::string> chunks;
        size_t pos2 = 0;
        while (pos2 < w_lower.size()) {
            int best_len = 0;
            for (int len = (int)(w_lower.size() - pos2); len > 2; --len) {
                if (vocab_to_idx.count(w_lower.substr(pos2, len))) {
                    best_len = len;
                    break;
                }
            }
            if (best_len > 0) {
                chunks.push_back(w.substr(pos2, best_len));
                pos2 += best_len;
            } else {
                chunks.push_back(w.substr(pos2, 1));
                pos2 += 1;
            }
        }

        std::vector<std::string> merged;
        std::string temp;
        for (const auto& c : chunks) {
            bool is_short_oov = c.size() < 3 && !vocab_to_idx.count(to_lower(c));
            if (is_short_oov) {
                temp += c;
            } else {
                if (!temp.empty()) { merged.push_back(temp); temp = ""; }
                merged.push_back(c);
            }
        }
        if (!temp.empty()) merged.push_back(temp);

        for (size_t k = 0; k < merged.size(); ++k) {
            std::string chunk_sp = (k < merged.size() - 1) ? "" : sp;
            refined_tokens.push_back({merged[k], chunk_sp});
        }
    }

    return {leading_ws, refined_tokens};
}

std::string detokenize(const std::string& leading_ws, const std::vector<Token>& tokens) {
    std::string result = leading_ws;
    for (const auto& [tok, sp] : tokens) {
        result += tok;
        result += sp;
    }
    return result;
}

int get_casing(const std::string& w) {
    if (w == to_lower(w))       return 0;
    if (w == capitalize(w))     return 1;
    if (w == to_upper(w))       return 2;
    if (w == to_title(w))       return 3;
    return -1;
}

int get_actual_expert(const std::string& w, const std::unordered_map<std::string, int>& vocab_to_idx, int expert_size) {
    std::string w_lower = to_lower(w);
    int casing = get_casing(w);
    auto it = vocab_to_idx.find(w_lower);
    if (it != vocab_to_idx.end() && casing != -1) {
        return it->second / expert_size;
    }
    return 31;
}
""",

    "ans.hpp": """#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>

struct AdaptiveModel {
    std::vector<int> freqs;
    std::vector<int> cumul;
    int total;

    explicit AdaptiveModel(int size);
    explicit AdaptiveModel(std::vector<int> init_freqs);

    void get_stats(int sym, int& out_cum, int& out_freq, int& out_total) const;
    void update(int sym);
    int find_symbol(int slot, int& out_cum, int& out_freq) const;
};

struct ANSAction {
    enum Type { ADAPTIVE, UNIFORM } type;
    int cum_low = 0, freq = 0, total_val = 0;
    int val = 0, bits = 0;
};

struct ANSStream {
    std::vector<ANSAction> actions;
    void write_adaptive(AdaptiveModel& model, int sym);
    void write_uniform(int val, int bits);
    std::vector<uint8_t> finalize();
};

struct ANSDecoder {
    uint64_t x;
    std::vector<uint8_t> payload_data;
    size_t ptr;

    explicit ANSDecoder(const std::vector<uint8_t>& payload);
    uint32_t read_word();
    int  read_adaptive(AdaptiveModel& model);
    int  read_uniform(int bits);
};

void get_number_parts(int val, int& out_bits, int& out_rem_bits, int& out_rem_val);
void write_number(ANSStream& stream, int val, AdaptiveModel& bits_model);
int  read_number(ANSDecoder& stream, AdaptiveModel& bits_model);
std::vector<uint8_t> encode_varint(uint64_t val);
uint64_t decode_varint(const std::vector<uint8_t>& data, size_t& ptr);
""",

    "ans.cpp": """#include "ans.hpp"
#include <algorithm>
#include <stdexcept>

// ─── AdaptiveModel ────────────────────────────────────────────────────────────

AdaptiveModel::AdaptiveModel(int size) {
    freqs.assign(size, 1);
    total = size;
    cumul.resize(size + 1);
    for (int i = 0; i <= size; ++i) cumul[i] = i;
}

AdaptiveModel::AdaptiveModel(std::vector<int> init_freqs)
    : freqs(std::move(init_freqs)) {
    total = 0;
    for (int f : freqs) total += f;
    cumul.resize(freqs.size() + 1);
    cumul[0] = 0;
    for (size_t i = 0; i < freqs.size(); ++i)
        cumul[i + 1] = cumul[i] + freqs[i];
}

void AdaptiveModel::get_stats(int sym, int& out_cum, int& out_freq, int& out_total) const {
    out_cum   = cumul[sym];
    out_freq  = freqs[sym];
    out_total = total;
}

void AdaptiveModel::update(int sym) {
    freqs[sym]++;
    total++;
    for (int i = sym + 1; i < (int)cumul.size(); ++i)
        cumul[i]++;

    if (total >= 16383) {
        int cum = 0;
        cumul[0] = 0;
        for (size_t i = 0; i < freqs.size(); ++i) {
            freqs[i] = (freqs[i] >> 1) | 1;
            cum += freqs[i];
            cumul[i + 1] = cum;
        }
        total = cum;
    }
}

int AdaptiveModel::find_symbol(int slot, int& out_cum, int& out_freq) const {
    int lo = 0, hi = (int)freqs.size() - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (cumul[mid + 1] <= slot) lo = mid + 1;
        else hi = mid;
    }
    if (cumul[lo] <= slot && slot < cumul[lo + 1]) {
        out_cum  = cumul[lo];
        out_freq = freqs[lo];
        return lo;
    }
    throw std::runtime_error("ANS find_symbol: slot out of bounds");
}

// ─── ANSStream ────────────────────────────────────────────────────────────────

void ANSStream::write_adaptive(AdaptiveModel& model, int sym) {
    ANSAction a;
    a.type = ANSAction::ADAPTIVE;
    model.get_stats(sym, a.cum_low, a.freq, a.total_val);
    actions.push_back(a);
    model.update(sym);
}

void ANSStream::write_uniform(int val, int bits) {
    if (bits <= 0) return;
    ANSAction a;
    a.type = ANSAction::UNIFORM;
    a.val  = val;
    a.bits = bits;
    actions.push_back(a);
}

// ─── Zero-Overhead 32-Bit rANS ────────────────────────────────────────────────

std::vector<uint8_t> ANSStream::finalize() {
    uint64_t X_MAX = 1ULL << 63;
    uint64_t x = 1; // Start at 1 (Zero dummy bits) like BigInt
    std::vector<uint8_t> out;

    for (int k = (int)actions.size() - 1; k >= 0; --k) {
        const auto& a = actions[k];
        if (a.type == ANSAction::ADAPTIVE) {
            uint64_t x_max = (X_MAX / a.total_val) * a.freq;
            while (x >= x_max) {
                out.push_back((uint8_t)(x & 0xFF));
                out.push_back((uint8_t)((x >> 8) & 0xFF));
                out.push_back((uint8_t)((x >> 16) & 0xFF));
                out.push_back((uint8_t)((x >> 24) & 0xFF));
                x >>= 32;
            }
            x = (x / a.freq) * a.total_val + (x % a.freq) + a.cum_low;
        } else {
            uint64_t x_max = X_MAX >> a.bits;
            while (x >= x_max) {
                out.push_back((uint8_t)(x & 0xFF));
                out.push_back((uint8_t)((x >> 8) & 0xFF));
                out.push_back((uint8_t)((x >> 16) & 0xFF));
                out.push_back((uint8_t)((x >> 24) & 0xFF));
                x >>= 32;
            }
            x = (x << a.bits) | (uint64_t)a.val;
        }
    }

    // Exact variable length flush (removes 4 bytes of fixed padding)
    uint8_t x_bytes = 0;
    uint64_t temp = x;
    if (temp == 0) x_bytes = 1;
    while (temp > 0) { x_bytes++; temp >>= 8; }
    
    for (int i = 0; i < x_bytes; ++i) {
        out.push_back((uint8_t)(x & 0xFF));
        x >>= 8;
    }
    
    out.push_back(x_bytes); // 1-byte length prefix to sync the hardware streams

    std::reverse(out.begin(), out.end());
    return out;
}

ANSDecoder::ANSDecoder(const std::vector<uint8_t>& payload) {
    payload_data = payload;
    ptr = 0;
    x = 0;
    
    // Read precise variable state
    if (ptr < payload_data.size()) {
        uint8_t x_bytes = payload_data[ptr++];
        for (int i = 0; i < x_bytes; ++i) {
            x = (x << 8) | (ptr < payload_data.size() ? payload_data[ptr++] : 0);
        }
    } else {
        x = 1; 
    }
}

uint32_t ANSDecoder::read_word() {
    uint32_t w = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t byte = 0;
        if (ptr < payload_data.size()) {
            byte = payload_data[ptr++];
        }
        w = (w << 8) | byte;
    }
    return w;
}

int ANSDecoder::read_adaptive(AdaptiveModel& model) {
    uint32_t slot = x % model.total;
    int cum_low, freq;
    int sym = model.find_symbol(slot, cum_low, freq);

    x = (x / model.total) * freq + slot - cum_low;
    model.update(sym);

    uint64_t L = 1ULL << 31;
    while (x < L) {
        if (ptr >= payload_data.size()) break; // Safe break for zero-overhead EOF
        x = (x << 32) | read_word();
    }
    return sym;
}

int ANSDecoder::read_uniform(int bits) {
    if (bits == 0) return 0;
    uint32_t val = x & ((1ULL << bits) - 1);
    x >>= bits;

    uint64_t L = 1ULL << 31;
    while (x < L) {
        if (ptr >= payload_data.size()) break; // Safe break for zero-overhead EOF
        x = (x << 32) | read_word();
    }
    return val;
}

// ─── Number Encoding Helpers ──────────────────────────────────────────────────

void get_number_parts(int val, int& out_bits, int& out_rem_bits, int& out_rem_val) {
    if (val == 0) { out_bits = 0; out_rem_bits = 0; out_rem_val = 0; return; }
    int bits = 0, v = val;
    while (v) { ++bits; v >>= 1; }
    out_bits     = bits;
    out_rem_bits = bits - 1;
    out_rem_val  = val & ((1 << (bits - 1)) - 1);
}

void write_number(ANSStream& stream, int val, AdaptiveModel& bits_model) {
    int bits, rem_bits, rem_val;
    get_number_parts(val, bits, rem_bits, rem_val);
    stream.write_adaptive(bits_model, bits);
    stream.write_uniform(rem_val, rem_bits);
}

int read_number(ANSDecoder& stream, AdaptiveModel& bits_model) {
    int bits = stream.read_adaptive(bits_model);
    if (bits == 0) return 0;
    return (1 << (bits - 1)) | stream.read_uniform(bits - 1);
}

std::vector<uint8_t> encode_varint(uint64_t val) {
    std::vector<uint8_t> out;
    while (val >= 0x80) { out.push_back((uint8_t)((val & 0x7F) | 0x80)); val >>= 7; }
    out.push_back((uint8_t)val);
    return out;
}

uint64_t decode_varint(const std::vector<uint8_t>& data, size_t& ptr) {
    uint64_t val = 0; int shift = 0;
    while (true) {
        if (ptr >= data.size()) return val; // Safety bound
        uint8_t b = data[ptr++];
        val |= ((uint64_t)(b & 0x7F)) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return val;
}
""",

    "router.hpp": """#pragma once
#include <vector>
#include <string>
#include <unordered_map>

using TransitionMatrix = std::vector<std::vector<int>>;

int  predict_next_expert(int prev_expert, const TransitionMatrix& matrix);
void update_transition_matrix(int prev_expert, int actual_expert, TransitionMatrix& matrix);

TransitionMatrix train_router_on_corpus(
    const std::string& corpus_text,
    const std::unordered_map<std::string, int>& vocab_to_idx,
    int expert_count, int expert_size,
    const std::string& output_filepath = "router_state.json");

TransitionMatrix load_router(const std::string& filepath, int expert_count);
void save_router(const TransitionMatrix& matrix, const std::string& filepath);
""",

    "router.cpp": """#include "router.hpp"
#include "tokenizer.hpp"
#include "embedded_assets.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

static std::string matrix_to_json(const TransitionMatrix& m) {
    std::ostringstream oss;
    oss << "[";
    for (size_t r = 0; r < m.size(); ++r) {
        if (r > 0) oss << ",";
        oss << "[";
        for (size_t c = 0; c < m[r].size(); ++c) {
            if (c > 0) oss << ",";
            oss << m[r][c];
        }
        oss << "]";
    }
    oss << "]";
    return oss.str();
}

static TransitionMatrix json_to_matrix(const std::string& json) {
    TransitionMatrix result;
    std::vector<int> current_row;
    bool in_inner = false;
    size_t i = 0;
    while (i < json.size()) {
        char c = json[i];
        if (c == '[') {
            if (in_inner) {
                current_row.clear();
            } else {
                in_inner = (i > 0 && json[i-1] != 0);
            }
            in_inner = true;
            ++i;
        } else if (c == ']') {
            if (!current_row.empty()) {
                result.push_back(current_row);
                current_row.clear();
                in_inner = false;
            }
            ++i;
        } else if (std::isdigit((unsigned char)c) || (c == '-' && i + 1 < json.size() && std::isdigit((unsigned char)json[i+1]))) {
            size_t end = i;
            if (c == '-') ++end;
            while (end < json.size() && std::isdigit((unsigned char)json[end])) ++end;
            current_row.push_back(std::stoi(json.substr(i, end - i)));
            i = end;
        } else {
            ++i;
        }
    }
    return result;
}

int predict_next_expert(int prev_expert, const TransitionMatrix& matrix) {
    const auto& row = matrix[prev_expert];
    int max_val = *std::max_element(row.begin(), row.end());
    if (row[prev_expert] == max_val) return prev_expert;
    for (int i = 0; i < (int)row.size(); ++i)
        if (row[i] == max_val) return i;
    return prev_expert;
}

void update_transition_matrix(int prev_expert, int actual_expert, TransitionMatrix& matrix) {
    auto& row = matrix[prev_expert];
    row[actual_expert]++;
    int s = 0;
    for (int v : row) s += v;
    if (s > 128) {
        for (int& v : row) v = std::max(1, v / 2);
    }
}

TransitionMatrix train_router_on_corpus(
    const std::string& corpus_text,
    const std::unordered_map<std::string, int>& vocab_to_idx,
    int expert_count, int expert_size,
    const std::string& output_filepath) {

    std::cout << "--- ROUTER TRAINING PHASE STARTED ---\\n";
    TransitionMatrix matrix(expert_count, std::vector<int>(expert_count, 1));
    int prev_expert = 0;

    auto [lws, tokens] = tokenize(corpus_text, vocab_to_idx);
    for (const auto& [w, _] : tokens) {
        int actual = get_actual_expert(w, vocab_to_idx, expert_size);
        update_transition_matrix(prev_expert, actual, matrix);
        prev_expert = actual;
    }

    save_router(matrix, output_filepath);
    std::cout << "Training completed. Saved pre-trained matrix to '"
              << output_filepath << "'.\\n\\n";
    return matrix;
}

void save_router(const TransitionMatrix& matrix, const std::string& filepath) {
    std::ofstream fout(filepath);
    if (!fout.is_open())
        throw std::runtime_error("Cannot write router state to: " + filepath);
    fout << matrix_to_json(matrix);
}

TransitionMatrix load_router(const std::string& filepath, int expert_count) {
    std::ifstream fin(filepath);
    std::string content;
    if (!fin.is_open()) {
        std::cerr << "[WARN] Router state '" << filepath << "' not found on disk. Loading embedded router state from memory.\\n";
        std::ostringstream oss;
        for (size_t i = 0; i < EMBEDDED_ROUTER_CHUNKS_COUNT; ++i) {
            oss << EMBEDDED_ROUTER_CHUNKS[i];
        }
        content = oss.str();
    } else {
        content = std::string((std::istreambuf_iterator<char>(fin)),
                               std::istreambuf_iterator<char>());
        fin.close();
    }
    return json_to_matrix(content);
}
""",

    "codec.hpp": """#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "router.hpp"

std::vector<uint8_t> compress_adaptive_moe(
    const std::string& text,
    const std::unordered_map<std::string, int>& vocab_to_idx,
    const TransitionMatrix& initial_matrix,
    int expert_count, int expert_size,
    bool verbose = false);

std::string decompress_adaptive_moe(
    const std::vector<uint8_t>& packed_bytes,
    const std::vector<std::string>& vocab,
    const std::unordered_map<std::string, int>& vocab_to_idx,
    const TransitionMatrix& initial_matrix,
    int expert_count, int expert_size,
    bool verbose = false);
""",

    "codec.cpp": """#include "codec.hpp"
#include "tokenizer.hpp"
#include "ans.hpp"
#include "router.hpp"
#include <unordered_map>
#include <stdexcept>
#include <cstring>
#include <optional>
#include <deque>

struct MatchResult { int dist; int length; };

using LZCache = std::unordered_map<std::string, std::deque<int>>;

static std::optional<MatchResult>
find_best_token_match(const std::vector<Token>& tokens, int current_idx,
                      LZCache& lz_cache,
                      int max_window = 2000000,
                      int max_chain = 128,
                      int good_enough_len = 32) {

    const std::string& token_str = tokens[current_idx].first;
    int best_len = 0, best_dist = 0;
    int start_search = std::max(0, current_idx - max_window);

    auto it = lz_cache.find(token_str);
    if (it != lz_cache.end()) {
        auto& positions = it->second;

        while (!positions.empty() && positions.front() < start_search)
            positions.pop_front();

        int examined = 0;
        int max_possible = (int)tokens.size() - current_idx;
        for (auto rit = positions.rbegin();
             rit != positions.rend() && examined < max_chain;
             ++rit, ++examined) {
            int j = *rit;
            int length = 0;
            while (length < max_possible &&
                   j + length < current_idx &&
                   tokens[j + length] == tokens[current_idx + length])
                ++length;
            if (length > best_len) {
                best_len  = length;
                best_dist = current_idx - j;
                if (best_len >= good_enough_len) break;
            }
        }
    }

    auto& chain = lz_cache[token_str];
    chain.push_back(current_idx);
    if ((int)chain.size() > max_chain)
        chain.pop_front();

    if (best_len >= 1)
        return MatchResult{best_dist, best_len};
    return std::nullopt;
}

static void write_spacing(ANSStream& stream, const std::string& sp,
                          AdaptiveModel& sp_model, AdaptiveModel& len_model,
                          AdaptiveModel& char_model) {
    if (sp == " ")       { stream.write_adaptive(sp_model, 0); }
    else if (sp.empty()) { stream.write_adaptive(sp_model, 1); }
    else {
        stream.write_adaptive(sp_model, 2);
        const auto* bytes = reinterpret_cast<const uint8_t*>(sp.data());
        int len = (int)sp.size();
        write_number(stream, len, len_model);
        for (int i = 0; i < len; ++i)
            stream.write_adaptive(char_model, bytes[i]);
    }
}

static std::string read_spacing(ANSDecoder& stream,
                                AdaptiveModel& sp_model, AdaptiveModel& len_model,
                                AdaptiveModel& char_model) {
    int sp_tag = stream.read_adaptive(sp_model);
    if (sp_tag == 0) return " ";
    if (sp_tag == 1) return "";
    int length = read_number(stream, len_model);
    std::vector<uint8_t> bts(length);
    for (int i = 0; i < length; ++i)
        bts[i] = (uint8_t)stream.read_adaptive(char_model);
    return std::string(reinterpret_cast<const char*>(bts.data()), length);
}

std::vector<uint8_t> compress_adaptive_moe(
    const std::string& text,
    const std::unordered_map<std::string, int>& vocab_to_idx,
    const TransitionMatrix& initial_matrix,
    int expert_count, int expert_size,
    bool /*verbose*/) {

    auto [leading_ws, tokens] = tokenize(text, vocab_to_idx);
    if (tokens.empty() && leading_ws.empty()) return {};

    AdaptiveModel tag_model({8, 1, 1, 1});
    AdaptiveModel casing_model({8, 1, 1, 1});
    AdaptiveModel expert_model(expert_count);
    AdaptiveModel spacing_model({8, 2, 1});
    AdaptiveModel char_model(256);

    AdaptiveModel local_idx_bits({1,1,2,4,8,16,32,64,128,64,32,16,8,4,2,1});
    AdaptiveModel dist_bits(32);
    AdaptiveModel len_bits(32);
    AdaptiveModel char_len_bits({1,2,4,8,16,32,64,32,16,8,4,2,1,1,1,1});

    ANSStream stream;
    TransitionMatrix transition_matrix = initial_matrix;
    int prev_expert = 0;
    int i = 0;

    LZCache lz_cache; // Declared locally for thread safety and auto-destruction

    while (i < (int)tokens.size()) {
        auto match = find_best_token_match(tokens, i, lz_cache);
        if (match) {
            int dist = match->dist, length = match->length;
            stream.write_adaptive(tag_model, 3);
            write_number(stream, dist, dist_bits);
            write_number(stream, length, len_bits);

            for (int k = 0; k < length; ++k) {
                const auto& [w_curr, _] = tokens[i + k];
                int ae = get_actual_expert(w_curr, vocab_to_idx, expert_size);
                update_transition_matrix(prev_expert, ae, transition_matrix);
                prev_expert = ae;
            }
            i += length;
            continue;
        }

        const auto& [w, spacing] = tokens[i];
        std::string w_lower = to_lower(w);
        bool has_casing = (to_lower(w) != to_upper(w));
        int casing = get_casing(w);
        int predicted_expert = predict_next_expert(prev_expert, transition_matrix);

        auto vit = vocab_to_idx.find(w_lower);
        if (vit != vocab_to_idx.end() && casing != -1) {
            int global_idx   = vit->second;
            int actual_expert = global_idx / expert_size;
            int local_idx    = global_idx % expert_size;

            if (actual_expert == predicted_expert) {
                stream.write_adaptive(tag_model, 0);
                write_number(stream, local_idx, local_idx_bits);
                if (has_casing) stream.write_adaptive(casing_model, casing);
            } else {
                stream.write_adaptive(tag_model, 1);
                stream.write_adaptive(expert_model, actual_expert);
                write_number(stream, local_idx, local_idx_bits);
                if (has_casing) stream.write_adaptive(casing_model, casing);
            }

            update_transition_matrix(prev_expert, actual_expert, transition_matrix);
            prev_expert = actual_expert;
            write_spacing(stream, spacing, spacing_model, char_len_bits, char_model);
            ++i;
            continue;
        }

        stream.write_adaptive(tag_model, 2);
        const auto* wb = reinterpret_cast<const uint8_t*>(w.data());
        write_number(stream, (int)w.size(), char_len_bits);
        for (size_t b = 0; b < w.size(); ++b)
            stream.write_adaptive(char_model, wb[b]);

        update_transition_matrix(prev_expert, 31, transition_matrix);
        prev_expert = 31;
        write_spacing(stream, spacing, spacing_model, char_len_bits, char_model);
        ++i;
    }

    std::vector<uint8_t> ans_payload = stream.finalize();

    auto lead_bytes_v = std::vector<uint8_t>(leading_ws.begin(), leading_ws.end());
    auto hdr1 = encode_varint((uint64_t)tokens.size());
    auto hdr2 = encode_varint((uint64_t)lead_bytes_v.size());

    std::vector<uint8_t> result;
    result.insert(result.end(), hdr1.begin(), hdr1.end());
    result.insert(result.end(), hdr2.begin(), hdr2.end());
    result.insert(result.end(), lead_bytes_v.begin(), lead_bytes_v.end());
    result.insert(result.end(), ans_payload.begin(), ans_payload.end());
    return result;
}

std::string decompress_adaptive_moe(
    const std::vector<uint8_t>& packed_bytes,
    const std::vector<std::string>& vocab,
    const std::unordered_map<std::string, int>& vocab_to_idx,
    const TransitionMatrix& initial_matrix,
    int expert_count, int expert_size,
    bool /*verbose*/) {

    if (packed_bytes.empty()) return "";

    size_t ptr = 0;
    int word_count = (int)decode_varint(packed_bytes, ptr);
    int lead_len   = (int)decode_varint(packed_bytes, ptr);
    std::string leading_ws(packed_bytes.begin() + ptr,
                           packed_bytes.begin() + ptr + lead_len);
    ptr += lead_len;

    std::vector<uint8_t> ans_payload(packed_bytes.begin() + ptr,
                                     packed_bytes.end());
    ANSDecoder stream(ans_payload);

    AdaptiveModel tag_model({8, 1, 1, 1});
    AdaptiveModel casing_model({8, 1, 1, 1});
    AdaptiveModel expert_model(expert_count);
    AdaptiveModel spacing_model({8, 2, 1});
    AdaptiveModel char_model(256);

    AdaptiveModel local_idx_bits({1,1,2,4,8,16,32,64,128,64,32,16,8,4,2,1});
    // Must mirror compress_adaptive_moe exactly -- same model sizes in the
    // same order -- or the adaptive state will desync and decoding breaks.
    AdaptiveModel dist_bits(32);
    AdaptiveModel len_bits(32);
    AdaptiveModel char_len_bits({1,2,4,8,16,32,64,32,16,8,4,2,1,1,1,1});

    TransitionMatrix transition_matrix = initial_matrix;
    int prev_expert = 0;
    std::vector<Token> decoded_tokens;
    decoded_tokens.reserve(word_count);

    while ((int)decoded_tokens.size() < word_count) {
        int predicted_expert = predict_next_expert(prev_expert, transition_matrix);
        int tag = stream.read_adaptive(tag_model);

        if (tag == 0) {
            int local_idx  = read_number(stream, local_idx_bits);
            int global_idx = predicted_expert * expert_size + local_idx;
            std::string word = vocab[global_idx];

            bool has_casing = (to_lower(word) != to_upper(word));
            if (has_casing) {
                int casing = stream.read_adaptive(casing_model);
                if      (casing == 1) word = capitalize(word);
                else if (casing == 2) word = to_upper(word);
                else if (casing == 3) word = to_title(word);
            }

            std::string spacing = read_spacing(stream, spacing_model,
                                               char_len_bits, char_model);
            update_transition_matrix(prev_expert, predicted_expert, transition_matrix);
            prev_expert = predicted_expert;
            decoded_tokens.push_back({word, spacing});

        } else if (tag == 1) {
            int actual_expert = stream.read_adaptive(expert_model);
            int local_idx     = read_number(stream, local_idx_bits);
            int global_idx    = actual_expert * expert_size + local_idx;
            std::string word  = vocab[global_idx];

            bool has_casing = (to_lower(word) != to_upper(word));
            if (has_casing) {
                int casing = stream.read_adaptive(casing_model);
                if      (casing == 1) word = capitalize(word);
                else if (casing == 2) word = to_upper(word);
                else if (casing == 3) word = to_title(word);
            }

            std::string spacing = read_spacing(stream, spacing_model,
                                               char_len_bits, char_model);
            update_transition_matrix(prev_expert, actual_expert, transition_matrix);
            prev_expert = actual_expert;
            decoded_tokens.push_back({word, spacing});

        } else if (tag == 2) {
            int length = read_number(stream, char_len_bits);
            std::vector<uint8_t> bts(length);
            for (int b = 0; b < length; ++b)
                bts[b] = (uint8_t)stream.read_adaptive(char_model);
            std::string word(reinterpret_cast<const char*>(bts.data()), length);
            std::string spacing = read_spacing(stream, spacing_model,
                                               char_len_bits, char_model);
            update_transition_matrix(prev_expert, 31, transition_matrix);
            prev_expert = 31;
            decoded_tokens.push_back({word, spacing});

        } else if (tag == 3) {
            int dist   = read_number(stream, dist_bits);
            int length = read_number(stream, len_bits);
            int start_idx = (int)decoded_tokens.size() - dist;

            for (int k = 0; k < length; ++k) {
                const Token& copied = decoded_tokens[start_idx + k];
                int ae = get_actual_expert(copied.first, vocab_to_idx, expert_size);
                update_transition_matrix(prev_expert, ae, transition_matrix);
                prev_expert = ae;
                decoded_tokens.push_back(copied);
            }
        }
    }

    return detokenize(leading_ws, decoded_tokens);
}
""",

    "main.cpp": """#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <algorithm>
#include <cstring>
#include <cstdio>  // For std::fwrite

#include "vocab.hpp"
#include "tokenizer.hpp"
#include "ans.hpp"
#include "router.hpp"
#include "codec.hpp"

namespace fs = std::filesystem;

static std::string get_exe_dir() {
    return fs::path(fs::absolute(fs::path("."))).string();
}

static std::string resolve(const std::string& filename, bool write_mode = false) {
    std::string ext_dir = get_exe_dir();
    if (write_mode) return (fs::path(ext_dir) / filename).string();

    std::string ext_path = (fs::path(ext_dir) / filename).string();
    if (fs::exists(ext_path)) return ext_path;

    return ext_path;
}

static std::string bytes_to_hex(const std::vector<uint8_t>& data) {
    static const char hex_chars[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(data.size() * 2);
    for (uint8_t b : data) {
        out += hex_chars[b >> 4];
        out += hex_chars[b & 0xF];
    }
    return out;
}

static std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::string cleaned;
    for (char c : hex) {
        if (std::isxdigit((unsigned char)c)) cleaned += c;
    }
    if (cleaned.size() % 2 != 0)
        throw std::invalid_argument("Odd-length hex string");
    std::vector<uint8_t> out(cleaned.size() / 2);
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = (uint8_t)std::stoi(cleaned.substr(i * 2, 2), nullptr, 16);
    }
    return out;
}

static bool looks_like_hex(const std::vector<uint8_t>& raw) {
    for (uint8_t b : raw) {
        if (!std::isxdigit((unsigned char)b) && b != ' ' && b != '\\n' && b != '\\r' && b != '\\t')
            return false;
    }
    return true;
}

static std::vector<uint8_t> read_binary_file(const std::string& path) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(fin)),
                                 std::istreambuf_iterator<char>());
}

static std::string read_text_file(const std::string& path) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    return std::string((std::istreambuf_iterator<char>(fin)),
                        std::istreambuf_iterator<char>());
}

struct Args {
    std::string command;
    std::string input;
    std::string output;
    std::string hex_data;
    std::string corpus;
    std::string vocab   = "words_final.txt";
    std::string router  = "router_stateless_v4.json";
    bool quiet = false;
};

static void print_help(const char* prog) {
    std::cout <<
"Usage: " << prog << " COMMAND [options]\\n\\n"
"Commands:\\n"
"  compress   (c)   Compress a text file or string\\n"
"  decompress (d)   Decompress a .moe binary file\\n"
"  hexdec     (hd)  Decompress from a hex string on the command line\\n"
"  train      (t)   Train the router matrix on a text corpus\\n\\n"
"Options (all commands):\\n"
"  --vocab FILE     Vocabulary file (default: words_final.txt)\\n"
"  --router FILE    Router JSON state (default: router_stateless_v4.json)\\n"
"  -q, --quiet      Suppress informational output\\n\\n"
"compress / decompress:\\n"
"  INPUT            File path or '-' for stdin\\n"
"  -o, --output     Output path or '-' for stdout\\n\\n"
"hexdec:\\n"
"  HEX              Hex-encoded compressed data\\n"
"  -o, --output     Output path or '-' for stdout (default: stdout)\\n\\n"
"train:\\n"
"  --corpus FILE    Training corpus text file\\n\\n"
"Examples:\\n"
"  moezip compress myfile.txt -o myfile.moe\\n"
"  moezip decompress myfile.moe -o myfile.txt\\n"
"  moezip decompress myfile.moe -o -\\n"
"  moezip hexdec AABBCC... -o result.txt\\n"
"  moezip train --corpus corpus.txt\\n";
}

static Args parse_args(int argc, char* argv[]) {
    Args args;
    if (argc < 2) { print_help(argv[0]); std::exit(0); }

    std::string cmd = argv[1];
    if (cmd == "-h" || cmd == "--help") { print_help(argv[0]); std::exit(0); }

    if (cmd == "c")  cmd = "compress";
    if (cmd == "d")  cmd = "decompress";
    if (cmd == "hd") cmd = "hexdec";
    if (cmd == "t")  cmd = "train";
    args.command = cmd;

    int positional_count = 0;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--vocab"  && i+1 < argc) { args.vocab  = argv[++i]; }
        else if (a == "--router" && i+1 < argc) { args.router = argv[++i]; }
        else if ((a == "-o" || a == "--output") && i+1 < argc) { args.output = argv[++i]; }
        else if (a == "--corpus" && i+1 < argc) { args.corpus = argv[++i]; }
        else if (a == "-q" || a == "--quiet") { args.quiet = true; }
        else if (a[0] != '-' || a == "-") {
            if (cmd == "hexdec") {
                if (positional_count == 0) args.hex_data = a;
            } else {
                if (positional_count == 0) args.input = a;
            }
            ++positional_count;
        } else {
            std::cerr << "[WARN] Unknown option: " << a << "\\n";
        }
    }

    if (args.output.empty()) {
        if (cmd == "hexdec")     args.output = "-";
        else if (cmd == "compress")   args.output = "";
        else if (cmd == "decompress") args.output = "";
    }

    return args;
}

static void cmd_train(const Args& args) {
    auto vd = load_and_partition_wordlist(resolve(args.vocab));
    if (!fs::exists(args.corpus)) {
        std::cerr << "[ERROR] Corpus file not found: " << args.corpus << "\\n";
        std::exit(1);
    }
    std::string corpus_text = read_text_file(args.corpus);
    std::string out_matrix  = resolve(args.router, true);
    train_router_on_corpus(corpus_text, vd.vocab_to_idx,
                           vd.expert_count, vd.expert_size, out_matrix);
    std::cout << "Router matrix saved to: " << out_matrix << "\\n";
}

static void cmd_compress(const Args& args) {
    auto vd = load_and_partition_wordlist(resolve(args.vocab));
    auto matrix = load_router(resolve(args.router), vd.expert_count);

    std::string text;
    if (args.input == "-") {
        text = std::string((std::istreambuf_iterator<char>(std::cin)),
                            std::istreambuf_iterator<char>());
    } else if (fs::exists(args.input)) {
        text = read_text_file(args.input);
    } else {
        text = args.input;
    }

    auto packed = compress_adaptive_moe(text, vd.vocab_to_idx, matrix,
                                        vd.expert_count, vd.expert_size);

    size_t orig_bytes = text.size();
    size_t comp_bytes = packed.size();
    double ratio = orig_bytes ? (double)comp_bytes / orig_bytes * 100.0 : 0.0;

    if (args.output == "-") {
        std::fwrite(packed.data(), 1, packed.size(), stdout);
    } else {
        std::string out_path = args.output;
        if (out_path.empty()) {
            out_path = (args.input != "-") ? (args.input + ".moe") : "output.moe";
        }
        std::ofstream fout(out_path, std::ios::binary);
        if (!fout.is_open()) {
            std::cerr << "[ERROR] Cannot write to: " << out_path << "\\n";
            std::exit(1);
        }
        fout.write(reinterpret_cast<const char*>(packed.data()), packed.size());
        if (!args.quiet) {
            std::cout << "Compressed  : " << args.input << "\\n";
            std::cout << "Output      : " << out_path  << "\\n";
        }
    }

    if (!args.quiet) {
        std::cerr << "Original    : " << orig_bytes << " bytes\\n";
        std::cerr << "Compressed  : " << comp_bytes << " bytes\\n";
        std::cerr << "Ratio       : " << ratio      << "%\\n";
    }
}

static void cmd_decompress(const Args& args) {
    auto vd = load_and_partition_wordlist(resolve(args.vocab));
    auto matrix = load_router(resolve(args.router), vd.expert_count);

    std::vector<uint8_t> raw;
    if (args.input == "-") {
        raw = std::vector<uint8_t>((std::istreambuf_iterator<char>(std::cin)),
                                    std::istreambuf_iterator<char>());
    } else {
        if (!fs::exists(args.input)) {
            std::cerr << "[ERROR] Input file not found: " << args.input << "\\n";
            std::exit(1);
        }
        raw = read_binary_file(args.input);
    }

    std::vector<uint8_t> packed;
    if (looks_like_hex(raw)) {
        std::string hex_str(raw.begin(), raw.end());
        packed = hex_to_bytes(hex_str);
    } else {
        packed = raw;
    }

    std::string text = decompress_adaptive_moe(packed, vd.vocab, vd.vocab_to_idx,
                                               matrix, vd.expert_count, vd.expert_size);

    if (args.output == "-") {
        std::fwrite(text.data(), 1, text.size(), stdout);
    } else {
        std::string out_path = args.output;
        if (out_path.empty()) {
            if (args.input != "-" && args.input.size() >= 4 &&
                args.input.substr(args.input.size()-4) == ".moe")
                out_path = args.input.substr(0, args.input.size()-4) + ".txt";
            else
                out_path = "output.txt";
        }
        std::ofstream fout(out_path, std::ios::binary);
        if (!fout.is_open()) {
            std::cerr << "[ERROR] Cannot write to: " << out_path << "\\n";
            std::exit(1);
        }
        fout << text;
        if (!args.quiet) {
            std::cout << "Decompressed: " << args.input << "\\n";
            std::cout << "Output      : " << out_path  << "\\n";
        }
    }
}

static void cmd_hexdec(const Args& args) {
    auto vd = load_and_partition_wordlist(resolve(args.vocab));
    auto matrix = load_router(resolve(args.router), vd.expert_count);

    auto packed = hex_to_bytes(args.hex_data);
    std::string text = decompress_adaptive_moe(packed, vd.vocab, vd.vocab_to_idx,
                                               matrix, vd.expert_count, vd.expert_size);

    if (args.output.empty() || args.output == "-") {
        std::fwrite(text.data(), 1, text.size(), stdout);
    } else {
        std::ofstream fout(args.output);
        fout << text;
        if (!args.quiet)
            std::cout << "Output written to: " << args.output << "\\n";
    }
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    try {
        Args args = parse_args(argc, argv);

        if      (args.command == "compress")   cmd_compress(args);
        else if (args.command == "decompress") cmd_decompress(args);
        else if (args.command == "hexdec")     cmd_hexdec(args);
        else if (args.command == "train")      cmd_train(args);
        else { print_help(argv[0]); return 1; }

    } catch (const std::exception& ex) {
        std::cerr << "[FATAL] " << ex.what() << "\\n";
        return 1;
    }
    return 0;
}
""",

    "CMakeLists.txt": """cmake_minimum_required(VERSION 3.16)
project(moezip CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/EHsc /O2 /std:c++20)
else()
  add_compile_options(
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-march=native>
  )
endif()

add_executable(moezip
    main.cpp
    vocab.cpp
    tokenizer.cpp
    ans.cpp
    router.cpp
    codec.cpp
)

target_include_directories(moezip PRIVATE ${CMAKE_SOURCE_DIR})
"""
}

# Write files out
print("Generating C++ files in active directory...")
for filename, content in files.items():
    with open(filename, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Created: {filename}")

print("\\nSuccessfully updated project files. You can now compile a completely self-contained C++ executable.")