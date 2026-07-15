#pragma once
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
