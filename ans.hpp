#pragma once
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
