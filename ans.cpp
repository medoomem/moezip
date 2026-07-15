#include "ans.hpp"
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
