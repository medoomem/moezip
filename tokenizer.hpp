#pragma once
#include <string>
#include <vector>
#include <unordered_map>

using Token = std::pair<std::string, std::string>;

std::pair<std::string, std::vector<Token>> raw_tokenize(const std::string& text);
std::pair<std::string, std::vector<Token>> tokenize(const std::string& text, const std::unordered_map<std::string, int>& vocab_to_idx);
std::string detokenize(const std::string& leading_ws, const std::vector<Token>& tokens);
int get_casing(const std::string& w);
int get_actual_expert(const std::string& w, const std::unordered_map<std::string, int>& vocab_to_idx, int expert_size);
std::string to_lower(const std::string& s);
std::string to_upper(const std::string& s);
std::string capitalize(const std::string& s);
std::string to_title(const std::string& s);
bool is_all_nonword(const std::string& s);
