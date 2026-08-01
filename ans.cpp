#include "ans.hpp"
#include <algorithm>
#include <stdexcept>

AdaptiveModel::AdaptiveModel(int size) {
    n = size;
    freqs.assign(size, 1);
    bit.assign(n + 1, 0);
    total = size;
    for (int i = 0; i < n; ++i) _bit_add(i, 1);
}

AdaptiveModel::AdaptiveModel(std::vector<int> init_freqs) {
    freqs = std::move(init_freqs);
    n = freqs.size();
    bit.assign(n + 1, 0);
    total = 0;
    for (int i = 0; i < n; ++i) {
        total += freqs[i];
        _bit_add(i, freqs[i]);
    }
}

void AdaptiveModel::_bit_add(int idx, int val) {
    idx += 1;
    while (idx <= n) {
        bit[idx] += val;
        idx += idx & (-idx);
    }
}

int AdaptiveModel::_bit_sum(int idx) const {
    int res = 0;
    while (idx > 0) {
        res += bit[idx];
        idx -= idx & (-idx);
    }
    return res;
}

void AdaptiveModel::get_stats(int sym, int& out_cum, int& out_freq, int& out_total) const {
    out_cum = _bit_sum(sym);
    out_freq = freqs[sym];
    out_total = total;
}

void AdaptiveModel::update(int sym) {
    freqs[sym]++;
    total++;
    _bit_add(sym, 1);

    if (total >= 16383) {
        total = 0;
        std::fill(bit.begin(), bit.end(), 0);
        for (int i = 0; i < n; ++i) {
            freqs[i] = (freqs[i] >> 1) | 1;
            total += freqs[i];
            _bit_add(i, freqs[i]);
        }
    }
}

int AdaptiveModel::find_symbol(int slot, int& out_cum, int& out_freq) const {
    int idx = 0;
    int bit_mask = 1;
    while (bit_mask * 2 <= n) bit_mask *= 2;
    
    int curr_sum = 0;
    while (bit_mask > 0) {
        int next_idx = idx + bit_mask;
        if (next_idx <= n && curr_sum + bit[next_idx] <= slot) {
            idx = next_idx;
            curr_sum += bit[next_idx];
        }
        bit_mask /= 2;
    }
    
    if (idx < n) {
        out_cum = curr_sum;
        out_freq = freqs[idx];
        return idx;
    }
    throw std::runtime_error("ANS find_symbol: slot out of bounds");
}

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

std::vector<uint8_t> ANSStream::finalize() {
    uint64_t X_MAX = 1ULL << 63;
    uint64_t x = 1; 
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

    uint8_t x_bytes = 0;
    uint64_t temp = x;
    if (temp == 0) x_bytes = 1;
    while (temp > 0) { x_bytes++; temp >>= 8; }
    
    for (int i = 0; i < x_bytes; ++i) {
        out.push_back((uint8_t)(x & 0xFF));
        x >>= 8;
    }
    out.push_back(x_bytes); 

    std::reverse(out.begin(), out.end());
    return out;
}

// ZERO-COPY ANS DECODER IMPLEMENTATION
ANSDecoder::ANSDecoder(const std::vector<uint8_t>& payload) {
    payload_ref = &payload;
    ptr = 0;
    x = 0;
    
    if (ptr < payload_ref->size()) {
        uint8_t x_bytes = (*payload_ref)[ptr++];
        for (int i = 0; i < x_bytes; ++i) {
            x = (x << 8) | (ptr < payload_ref->size() ? (*payload_ref)[ptr++] : 0);
        }
    } else {
        x = 1; 
    }
}

uint32_t ANSDecoder::read_word() {
    uint32_t w = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t byte = 0;
        if (ptr < payload_ref->size()) byte = (*payload_ref)[ptr++];
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
        if (ptr >= payload_ref->size()) break; 
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
        if (ptr >= payload_ref->size()) break;
        x = (x << 32) | read_word();
    }
    return val;
}

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
        if (ptr >= data.size()) return val; 
        uint8_t b = data[ptr++];
        val |= ((uint64_t)(b & 0x7F)) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return val;
}
