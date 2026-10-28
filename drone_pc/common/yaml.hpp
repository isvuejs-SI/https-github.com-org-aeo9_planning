#pragma once

#include <cctype>
#include <cstdint>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace drone_pc {

struct Yaml {
    enum Type { NUL, BOOL, INT, FLOAT, STR, SEQ, MAP } type = NUL;
    bool b = false;
    int64_t i = 0;
    double f = 0.0;
    std::string s;
    std::vector<Yaml> seq;
    std::map<std::string, Yaml> dict;

    const Yaml& key(const std::string& name) const {
        const auto found = dict.find(name);
        if (found == dict.end()) {
            throw std::runtime_error("missing key " + name);
        }
        return found->second;
    }

    double number() const {
        if (type == INT) {
            return static_cast<double>(i);
        }
        if (type == FLOAT) {
            return f;
        }
        throw std::runtime_error("expected number");
    }

    int64_t integer() const {
        if (type == INT) {
            return i;
        }
        if (type == FLOAT) {
            return static_cast<int64_t>(f);
        }
        throw std::runtime_error("expected integer");
    }

    const std::string& text() const {
        if (type != STR) {
            throw std::runtime_error("expected string");
        }
        return s;
    }

    bool boolean() const {
        if (type != BOOL) {
            throw std::runtime_error("expected bool");
        }
        return b;
    }
};

inline std::string rtrim_copy(std::string value) {
    while (!value.empty() && (value.back() == ' ' || value.back() == '\r' || value.back() == '\t')) {
        value.pop_back();
    }
    return value;
}

inline std::string strip_comment(const std::string& line) {
    bool in_single = false;
    bool in_double = false;
    for (size_t index = 0; index < line.size(); ++index) {
        const char ch = line[index];
        if (ch == '\'' && !in_double) {
            in_single = !in_single;
        } else if (ch == '"' && !in_single) {
            in_double = !in_double;
        } else if (ch == '#' && !in_single && !in_double) {
            return line.substr(0, index);
        }
    }
    return line;
}

inline Yaml parse_scalar(const std::string& raw) {
    const std::string token = [&]() {
        size_t begin = 0;
        while (begin < raw.size() && raw[begin] == ' ') {
            ++begin;
        }
        size_t end = raw.size();
        while (end > begin && raw[end - 1] == ' ') {
            --end;
        }
        return raw.substr(begin, end - begin);
    }();
    Yaml value;
    if (token.size() >= 2 && (token.front() == '"' || token.front() == '\'') && token.front() == token.back()) {
        value.type = Yaml::STR;
        value.s = token.substr(1, token.size() - 2);
        return value;
    }
    std::string lower = token;
    for (char& ch : lower) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (lower == "true") {
        value.type = Yaml::BOOL;
        value.b = true;
        return value;
    }
    if (lower == "false") {
        value.type = Yaml::BOOL;
        value.b = false;
        return value;
    }
    if (lower == "null" || lower == "~") {
        value.type = Yaml::NUL;
        return value;
    }
    try {
        if (token.find('.') != std::string::npos || token.find('e') != std::string::npos ||
            token.find('E') != std::string::npos) {
            size_t used = 0;
            const double number = std::stod(token, &used);
            if (used == token.size()) {
                value.type = Yaml::FLOAT;
                value.f = number;
                return value;
            }
        } else {
            size_t used = 0;
            const long long number = std::stoll(token, &used, 10);
            if (used == token.size()) {
                value.type = Yaml::INT;
                value.i = number;
                return value;
            }
        }
    } catch (const std::exception&) {
    }
    value.type = Yaml::STR;
    value.s = token;
    return value;
}

inline std::pair<std::string, std::string> split_key(const std::string& text) {
    const auto colon = text.find(':');
    if (colon == std::string::npos) {
        throw std::runtime_error("expected key: value");
    }
    std::string key = text.substr(0, colon);
    std::string rest = text.substr(colon + 1);
    while (!key.empty() && key.back() == ' ') {
        key.pop_back();
    }
    size_t start = 0;
    while (start < rest.size() && rest[start] == ' ') {
        ++start;
    }
    return {key, rest.substr(start)};
}

inline std::vector<std::pair<int, std::string>> prepare_yaml(const std::string& text) {
    std::vector<std::pair<int, std::string>> lines;
    std::istringstream input(text);
    std::string raw;
    while (std::getline(input, raw)) {
        std::string cleaned = rtrim_copy(strip_comment(raw));
        if (cleaned.find_first_not_of(' ') == std::string::npos) {
            continue;
        }
        int indent = 0;
        while (indent < static_cast<int>(cleaned.size()) && cleaned[indent] == ' ') {
            ++indent;
        }
        if (cleaned.find('\t') != std::string::npos && cleaned.find('\t') <= static_cast<size_t>(indent)) {
            throw std::runtime_error("tabs are not allowed in limits yaml");
        }
        std::string body = cleaned.substr(indent);
        lines.emplace_back(indent, body);
    }
    return lines;
}

inline Yaml parse_block(const std::vector<std::pair<int, std::string>>& lines, size_t& index);

inline Yaml parse_map(const std::vector<std::pair<int, std::string>>& lines, size_t& index, int indent) {
    Yaml value;
    value.type = Yaml::MAP;
    const size_t total = lines.size();
    while (index < total && lines[index].first == indent && lines[index].second.compare(0, 2, "- ") != 0) {
        const auto parts = split_key(lines[index].second);
        ++index;
        if (parts.second.empty()) {
            Yaml child;
            if (index < total && lines[index].first > indent) {
                child = parse_block(lines, index);
            } else {
                child.type = Yaml::MAP;
            }
            value.dict.emplace(parts.first, std::move(child));
        } else {
            value.dict.emplace(parts.first, parse_scalar(parts.second));
        }
    }
    return value;
}

inline Yaml parse_seq(const std::vector<std::pair<int, std::string>>& lines, size_t& index, int indent) {
    Yaml value;
    value.type = Yaml::SEQ;
    const size_t total = lines.size();
    while (index < total && lines[index].first == indent && lines[index].second.compare(0, 2, "- ") == 0) {
        std::string content = lines[index].second.substr(2);
        size_t start = 0;
        while (start < content.size() && content[start] == ' ') {
            ++start;
        }
        content = content.substr(start);
        ++index;
        if (content.empty()) {
            Yaml child;
            if (index < total && lines[index].first > indent) {
                child = parse_block(lines, index);
            } else {
                child.type = Yaml::NUL;
            }
            value.seq.push_back(std::move(child));
            continue;
        }
        const bool keyed = content.find(':') != std::string::npos && content[0] != '\'' && content[0] != '"' &&
                           content[0] != '[';
        if (keyed) {
            Yaml item;
            item.type = Yaml::MAP;
            const auto parts = split_key(content);
            if (parts.second.empty()) {
                Yaml child;
                if (index < total && lines[index].first > indent) {
                    child = parse_block(lines, index);
                } else {
                    child.type = Yaml::MAP;
                }
                item.dict.emplace(parts.first, std::move(child));
            } else {
                item.dict.emplace(parts.first, parse_scalar(parts.second));
            }
            int item_indent = -1;
            while (index < total && lines[index].second.compare(0, 2, "- ") != 0) {
                if (item_indent < 0) {
                    if (lines[index].first <= indent) {
                        break;
                    }
                    item_indent = lines[index].first;
                }
                if (lines[index].first != item_indent) {
                    break;
                }
                const auto more = split_key(lines[index].second);
                ++index;
                if (more.second.empty()) {
                    Yaml child;
                    if (index < total && lines[index].first > item_indent) {
                        child = parse_block(lines, index);
                    } else {
                        child.type = Yaml::MAP;
                    }
                    item.dict.emplace(more.first, std::move(child));
                } else {
                    item.dict.emplace(more.first, parse_scalar(more.second));
                }
            }
            value.seq.push_back(std::move(item));
            continue;
        }
        value.seq.push_back(parse_scalar(content));
    }
    return value;
}

inline Yaml parse_block(const std::vector<std::pair<int, std::string>>& lines, size_t& index) {
    if (index >= lines.size()) {
        Yaml empty;
        empty.type = Yaml::MAP;
        return empty;
    }
    if (lines[index].second.compare(0, 2, "- ") == 0) {
        return parse_seq(lines, index, lines[index].first);
    }
    return parse_map(lines, index, lines[index].first);
}

inline Yaml parse_yaml(const std::string& text) {
    const auto lines = prepare_yaml(text);
    if (lines.empty()) {
        Yaml empty;
        empty.type = Yaml::NUL;
        return empty;
    }
    size_t index = 0;
    Yaml value = parse_block(lines, index);
    if (index != lines.size()) {
        throw std::runtime_error("trailing yaml near " + lines[index].second);
    }
    return value;
}

}  // namespace drone_pc
