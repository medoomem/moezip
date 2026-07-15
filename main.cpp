#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <algorithm>
#include <cstring>

#include "vocab.hpp"
#include "tokenizer.hpp"
#include "ans.hpp"
#include "router.hpp"
#include "codec.hpp"

namespace fs = std::filesystem;

static std::string get_exe_dir() {
    return fs::path(fs::absolute(fs::path("."))).string();
}

static std::string resolve(const std::string& filename, bool write_mode = false) {
    std::string ext_dir = get_exe_dir();
    if (write_mode) return (fs::path(ext_dir) / filename).string();

    std::string ext_path = (fs::path(ext_dir) / filename).string();
    if (fs::exists(ext_path)) return ext_path;

    return ext_path;
}

static std::string bytes_to_hex(const std::vector<uint8_t>& data) {
    static const char hex_chars[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(data.size() * 2);
    for (uint8_t b : data) {
        out += hex_chars[b >> 4];
        out += hex_chars[b & 0xF];
    }
    return out;
}

static std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::string cleaned;
    for (char c : hex) {
        if (std::isxdigit((unsigned char)c)) cleaned += c;
    }
    if (cleaned.size() % 2 != 0)
        throw std::invalid_argument("Odd-length hex string");
    std::vector<uint8_t> out(cleaned.size() / 2);
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = (uint8_t)std::stoi(cleaned.substr(i * 2, 2), nullptr, 16);
    }
    return out;
}

static bool looks_like_hex(const std::vector<uint8_t>& raw) {
    for (uint8_t b : raw) {
        if (!std::isxdigit((unsigned char)b) && b != ' ' && b != '\n' && b != '\r' && b != '\t')
            return false;
    }
    return true;
}

static std::vector<uint8_t> read_binary_file(const std::string& path) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(fin)),
                                 std::istreambuf_iterator<char>());
}

static std::string read_text_file(const std::string& path) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    return std::string((std::istreambuf_iterator<char>(fin)),
                        std::istreambuf_iterator<char>());
}

struct Args {
    std::string command;
    std::string input;
    std::string output;
    std::string hex_data;
    std::string corpus;
    std::string vocab   = "words_final.txt";
    std::string router  = "router_stateless_v4.json";
    bool quiet = false;
};

static void print_help(const char* prog) {
    std::cout <<
"Usage: " << prog << " COMMAND [options]\n\n"
"Commands:\n"
"  compress   (c)   Compress a text file or string\n"
"  decompress (d)   Decompress a .moe binary file\n"
"  hexdec     (hd)  Decompress from a hex string on the command line\n"
"  train      (t)   Train the router matrix on a text corpus\n\n"
"Options (all commands):\n"
"  --vocab FILE     Vocabulary file (default: words_final.txt)\n"
"  --router FILE    Router JSON state (default: router_stateless_v4.json)\n"
"  -q, --quiet      Suppress informational output\n\n"
"compress / decompress:\n"
"  INPUT            File path or '-' for stdin\n"
"  -o, --output     Output path or '-' for stdout\n\n"
"hexdec:\n"
"  HEX              Hex-encoded compressed data\n"
"  -o, --output     Output path or '-' for stdout (default: stdout)\n\n"
"train:\n"
"  --corpus FILE    Training corpus text file\n\n"
"Examples:\n"
"  moezip compress myfile.txt -o myfile.moe\n"
"  moezip decompress myfile.moe -o myfile.txt\n"
"  moezip decompress myfile.moe -o -\n"
"  moezip hexdec AABBCC... -o result.txt\n"
"  moezip train --corpus corpus.txt\n";
}

static Args parse_args(int argc, char* argv[]) {
    Args args;
    if (argc < 2) { print_help(argv[0]); std::exit(0); }

    std::string cmd = argv[1];
    if (cmd == "-h" || cmd == "--help") { print_help(argv[0]); std::exit(0); }

    if (cmd == "c")  cmd = "compress";
    if (cmd == "d")  cmd = "decompress";
    if (cmd == "hd") cmd = "hexdec";
    if (cmd == "t")  cmd = "train";
    args.command = cmd;

    int positional_count = 0;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--vocab"  && i+1 < argc) { args.vocab  = argv[++i]; }
        else if (a == "--router" && i+1 < argc) { args.router = argv[++i]; }
        else if ((a == "-o" || a == "--output") && i+1 < argc) { args.output = argv[++i]; }
        else if (a == "--corpus" && i+1 < argc) { args.corpus = argv[++i]; }
        else if (a == "-q" || a == "--quiet") { args.quiet = true; }
        else if (a[0] != '-') {
            if (cmd == "hexdec") {
                if (positional_count == 0) args.hex_data = a;
            } else {
                if (positional_count == 0) args.input = a;
            }
            ++positional_count;
        } else {
            std::cerr << "[WARN] Unknown option: " << a << "\n";
        }
    }

    if (args.output.empty()) {
        if (cmd == "hexdec")     args.output = "-";
        else if (cmd == "compress")   args.output = "";
        else if (cmd == "decompress") args.output = "";
    }

    return args;
}

static void cmd_train(const Args& args) {
    auto vd = load_and_partition_wordlist(resolve(args.vocab));
    if (!fs::exists(args.corpus)) {
        std::cerr << "[ERROR] Corpus file not found: " << args.corpus << "\n";
        std::exit(1);
    }
    std::string corpus_text = read_text_file(args.corpus);
    std::string out_matrix  = resolve(args.router, true);
    train_router_on_corpus(corpus_text, vd.vocab_to_idx,
                           vd.expert_count, vd.expert_size, out_matrix);
    std::cout << "Router matrix saved to: " << out_matrix << "\n";
}

static void cmd_compress(const Args& args) {
    auto vd = load_and_partition_wordlist(resolve(args.vocab));
    auto matrix = load_router(resolve(args.router), vd.expert_count);

    std::string text;
    if (args.input == "-") {
        text = std::string((std::istreambuf_iterator<char>(std::cin)),
                            std::istreambuf_iterator<char>());
    } else if (fs::exists(args.input)) {
        text = read_text_file(args.input);
    } else {
        text = args.input;
    }

    auto packed = compress_adaptive_moe(text, vd.vocab_to_idx, matrix,
                                        vd.expert_count, vd.expert_size);

    size_t orig_bytes = text.size();
    size_t comp_bytes = packed.size();
    double ratio = orig_bytes ? (double)comp_bytes / orig_bytes * 100.0 : 0.0;

    if (args.output == "-") {
        std::cout << bytes_to_hex(packed) << "\n";
    } else {
        std::string out_path = args.output;
        if (out_path.empty()) {
            out_path = (args.input != "-") ? (args.input + ".moe") : "output.moe";
        }
        std::ofstream fout(out_path, std::ios::binary);
        if (!fout.is_open()) {
            std::cerr << "[ERROR] Cannot write to: " << out_path << "\n";
            std::exit(1);
        }
        fout.write(reinterpret_cast<const char*>(packed.data()), packed.size());
        if (!args.quiet) {
            std::cout << "Compressed  : " << args.input << "\n";
            std::cout << "Output      : " << out_path  << "\n";
        }
    }

    if (!args.quiet) {
        std::cout << "Original    : " << orig_bytes << " bytes\n";
        std::cout << "Compressed  : " << comp_bytes << " bytes\n";
        std::cout << "Ratio       : " << ratio      << "%\n";
    }
}

static void cmd_decompress(const Args& args) {
    auto vd = load_and_partition_wordlist(resolve(args.vocab));
    auto matrix = load_router(resolve(args.router), vd.expert_count);

    std::vector<uint8_t> raw;
    if (args.input == "-") {
        raw = std::vector<uint8_t>((std::istreambuf_iterator<char>(std::cin)),
                                    std::istreambuf_iterator<char>());
    } else {
        if (!fs::exists(args.input)) {
            std::cerr << "[ERROR] Input file not found: " << args.input << "\n";
            std::exit(1);
        }
        raw = read_binary_file(args.input);
    }

    std::vector<uint8_t> packed;
    if (looks_like_hex(raw)) {
        std::string hex_str(raw.begin(), raw.end());
        packed = hex_to_bytes(hex_str);
    } else {
        packed = raw;
    }

    std::string text = decompress_adaptive_moe(packed, vd.vocab, vd.vocab_to_idx,
                                               matrix, vd.expert_count, vd.expert_size);

    if (args.output == "-") {
        std::cout << text;
    } else {
        std::string out_path = args.output;
        if (out_path.empty()) {
            if (args.input != "-" && args.input.size() >= 4 &&
                args.input.substr(args.input.size()-4) == ".moe")
                out_path = args.input.substr(0, args.input.size()-4) + ".txt";
            else
                out_path = "output.txt";
        }
        std::ofstream fout(out_path, std::ios::binary);
        if (!fout.is_open()) {
            std::cerr << "[ERROR] Cannot write to: " << out_path << "\n";
            std::exit(1);
        }
        fout << text;
        if (!args.quiet) {
            std::cout << "Decompressed: " << args.input << "\n";
            std::cout << "Output      : " << out_path  << "\n";
        }
    }
}

static void cmd_hexdec(const Args& args) {
    auto vd = load_and_partition_wordlist(resolve(args.vocab));
    auto matrix = load_router(resolve(args.router), vd.expert_count);

    auto packed = hex_to_bytes(args.hex_data);
    std::string text = decompress_adaptive_moe(packed, vd.vocab, vd.vocab_to_idx,
                                               matrix, vd.expert_count, vd.expert_size);

    if (args.output.empty() || args.output == "-") {
        std::cout << text << "\n";
    } else {
        std::ofstream fout(args.output);
        fout << text;
        if (!args.quiet)
            std::cout << "Output written to: " << args.output << "\n";
    }
}

int main(int argc, char* argv[]) {
    try {
        Args args = parse_args(argc, argv);

        if      (args.command == "compress")   cmd_compress(args);
        else if (args.command == "decompress") cmd_decompress(args);
        else if (args.command == "hexdec")     cmd_hexdec(args);
        else if (args.command == "train")      cmd_train(args);
        else { print_help(argv[0]); return 1; }

    } catch (const std::exception& ex) {
        std::cerr << "[FATAL] " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
