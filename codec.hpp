#pragma once
#include <cstdint>
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
