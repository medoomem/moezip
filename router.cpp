#include "router.hpp"
#include "tokenizer.hpp"
#include "embedded_assets.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cmath>

static std::string matrix_to_json(const TransitionMatrix& m) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < m.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "[";
        for (size_t j = 0; j < m[i].size(); ++j) {
            if (j > 0) oss << ",";
            oss << "[";
            for (size_t k = 0; k < m[i][j].size(); ++k) {
                if (k > 0) oss << ",";
                oss << m[i][j][k];
            }
            oss << "]";
        }
        oss << "]";
    }
    oss << "]";
    return oss.str();
}

static TransitionMatrix json_to_matrix(const std::string& json) {
    TransitionMatrix result;
    std::vector<std::vector<int>> current_2d;
    std::vector<int> current_row;
    int depth = 0;
    size_t i = 0;
    while (i < json.size()) {
        char c = json[i];
        if (c == '[') {
            depth++;
            if (depth == 3) current_row.clear();
            ++i;
        } else if (c == ']') {
            if (depth == 3) {
                current_2d.push_back(current_row);
                current_row.clear();
            } else if (depth == 2) {
                result.push_back(current_2d);
                current_2d.clear();
            }
            depth--;
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

int predict_next_expert(int prev2, int prev1, const TransitionMatrix& matrix) {
    const auto& row = matrix[prev2][prev1];
    int max_val = *std::max_element(row.begin(), row.end());
    if (row[prev1] == max_val) return prev1;
    for (int i = 0; i < (int)row.size(); ++i)
        if (row[i] == max_val) return i;
    return prev1;
}

void update_transition_matrix(int prev2, int prev1, int actual_expert, TransitionMatrix& matrix) {
    auto& row = matrix[prev2][prev1];
    row[actual_expert]++;
    int s = 0;
    for (int v : row) s += v;
    if (s > 1024) {
        for (int& v : row) v = std::max(1, (int)(v * 0.9));
    }
}

TransitionMatrix train_router_on_corpus(
    const std::string& corpus_text,
    const std::unordered_map<std::string, int>& vocab_to_idx,
    int expert_count, int expert_size,
    const std::string& output_filepath) {

    std::cout << "--- ROUTER TRAINING PHASE STARTED (2nd-Order Markov) ---\n";
    TransitionMatrix matrix(expert_count, std::vector<std::vector<int>>(expert_count, std::vector<int>(expert_count, 1)));
    int prev2_expert = 0;
    int prev1_expert = 0;

    auto [lws, tokens] = tokenize(corpus_text, vocab_to_idx);
    for (const auto& [w, _] : tokens) {
        int actual = get_actual_expert(w, vocab_to_idx, expert_size);
        update_transition_matrix(prev2_expert, prev1_expert, actual, matrix);
        prev2_expert = prev1_expert;
        prev1_expert = actual;
    }

    save_router(matrix, output_filepath);
    std::cout << "Training completed. Saved pre-trained matrix to '"
              << output_filepath << "'.\n\n";
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
        std::cerr << "[WARN] Router state '" << filepath << "' not found on disk. Loading embedded router state from memory.\n";
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
