#include "tokenizer.hpp"
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
