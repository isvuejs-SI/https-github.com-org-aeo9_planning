#pragma once

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace drone_pc {

struct Json {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type = NUL;
    bool b = false;
    double num = 0.0;
    bool is_int = false;
    int64_t inum = 0;
    std::string str;
    std::vector<Json> arr;
    std::map<std::string, Json> obj;

    const Json& at(const std::string& key) const { return obj.at(key); }
    bool has(const std::string& key) const { return obj.find(key) != obj.end(); }
    int64_t integer() const { return is_int ? inum : static_cast<int64_t>(num); }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : text_(text) {}

    Json parse() {
        skip();
        Json value = parse_value();
        skip();
        if (index_ != text_.size()) {
            throw std::runtime_error("trailing json");
        }
        return value;
    }

private:
    const std::string& text_;
    size_t index_ = 0;

    void skip() {
        while (index_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[index_]))) {
            ++index_;
        }
    }

    char peek() const { return index_ < text_.size() ? text_[index_] : '\0'; }

    char get() { return index_ < text_.size() ? text_[index_++] : '\0'; }

    Json parse_value() {
        skip();
        const char ch = peek();
        if (ch == '{') {
            return parse_object();
        }
        if (ch == '[') {
            return parse_array();
        }
        if (ch == '"') {
            Json value;
            value.type = Json::STR;
            value.str = parse_string();
            return value;
        }
        if (ch == 't' || ch == 'f') {
            return parse_bool();
        }
        if (ch == 'n') {
            return parse_null();
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
            return parse_number();
        }
        throw std::runtime_error("invalid json");
    }

    Json parse_object() {
        get();
        Json value;
        value.type = Json::OBJ;
        skip();
        if (peek() == '}') {
            get();
            return value;
        }
        while (true) {
            skip();
            if (peek() != '"') {
                throw std::runtime_error("expected key");
            }
            const std::string key = parse_string();
            skip();
            if (get() != ':') {
                throw std::runtime_error("expected colon");
            }
            value.obj.emplace(key, parse_value());
            skip();
            const char ch = get();
            if (ch == '}') {
                break;
            }
            if (ch != ',') {
                throw std::runtime_error("expected comma");
            }
        }
        return value;
    }

    Json parse_array() {
        get();
        Json value;
        value.type = Json::ARR;
        skip();
        if (peek() == ']') {
            get();
            return value;
        }
        while (true) {
            value.arr.push_back(parse_value());
            skip();
            const char ch = get();
            if (ch == ']') {
                break;
            }
            if (ch != ',') {
                throw std::runtime_error("expected comma");
            }
        }
        return value;
    }

    std::string parse_string() {
        if (get() != '"') {
            throw std::runtime_error("expected string");
        }
        std::string out;
        while (index_ < text_.size()) {
            const char ch = get();
            if (ch == '"') {
                return out;
            }
            if (ch == '\\') {
                const char esc = get();
                if (esc == '"' || esc == '\\' || esc == '/') {
                    out.push_back(esc);
                } else if (esc == 'b') {
                    out.push_back('\b');
                } else if (esc == 'f') {
                    out.push_back('\f');
                } else if (esc == 'n') {
                    out.push_back('\n');
                } else if (esc == 'r') {
                    out.push_back('\r');
                } else if (esc == 't') {
                    out.push_back('\t');
                } else {
                    throw std::runtime_error("bad escape");
                }
            } else {
                out.push_back(ch);
            }
        }
        throw std::runtime_error("unterminated string");
    }

    Json parse_bool() {
        Json value;
        value.type = Json::BOOL;
        if (text_.compare(index_, 4, "true") == 0) {
            index_ += 4;
            value.b = true;
            return value;
        }
        if (text_.compare(index_, 5, "false") == 0) {
            index_ += 5;
            value.b = false;
            return value;
        }
        throw std::runtime_error("bad bool");
    }

    Json parse_null() {
        if (text_.compare(index_, 4, "null") != 0) {
            throw std::runtime_error("bad null");
        }
        index_ += 4;
        Json value;
        value.type = Json::NUL;
        return value;
    }

    Json parse_number() {
        const size_t start = index_;
        if (peek() == '-') {
            get();
        }
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            get();
        }
        bool integral = true;
        if (peek() == '.') {
            integral = false;
            get();
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                get();
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            integral = false;
            get();
            if (peek() == '+' || peek() == '-') {
                get();
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                get();
            }
        }
        const std::string token = text_.substr(start, index_ - start);
        Json value;
        value.type = Json::NUM;
        value.num = std::strtod(token.c_str(), nullptr);
        if (integral) {
            value.is_int = true;
            value.inum = std::stoll(token);
        }
        return value;
    }
};

inline Json parse_json(const std::string& text) { return JsonParser(text).parse(); }

}  // namespace drone_pc
