#include "codec.hpp"
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
