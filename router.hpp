#pragma once
#include <vector>
#include <string>
#include <unordered_map>

// 3D Matrix: [prev2][prev1][actual]
using TransitionMatrix = std::vector<std::vector<std::vector<int>>>;

int  predict_next_expert(int prev2, int prev1, const TransitionMatrix& matrix);
void update_transition_matrix(int prev2, int prev1, int actual_expert, TransitionMatrix& matrix);

TransitionMatrix train_router_on_corpus(
    const std::string& corpus_text,
    const std::unordered_map<std::string, int>& vocab_to_idx,
    int expert_count, int expert_size,
    const std::string& output_filepath = "router_stateless_v5_2nd_order.json");

TransitionMatrix load_router(const std::string& filepath, int expert_count);
void save_router(const TransitionMatrix& matrix, const std::string& filepath);
