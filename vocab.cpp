#include "vocab.hpp"
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
        std::cerr << "'" << filepath << "' not found on disk. Loading embedded vocabulary chunks from binary...\n";
        std::stringstream ssin;
        for (size_t i = 0; i < EMBEDDED_VOCAB_CHUNKS_COUNT; ++i) {
            ssin << EMBEDDED_VOCAB_CHUNKS[i];
        }
        while (std::getline(ssin, line)) {
            while (!line.empty() && (line.back()=='\r'||line.back()=='\n'||line.back()==' '))
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
            while (!line.empty() && (line.back()=='\r'||line.back()=='\n'||line.back()==' '))
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
    std::cerr << "Loaded " << total_words << " unique tokens (including N-Grams).\n";

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
              << " | Capacity/Exp: " << expert_size << "\n";

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
