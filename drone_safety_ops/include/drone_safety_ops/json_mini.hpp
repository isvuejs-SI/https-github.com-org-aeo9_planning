#pragma once

#include <cctype>
#include <map>
#include <stdexcept>
#include <string>

namespace drone_safety {

struct Json {
  std::map<std::string, std::string> values;
  std::map<std::string, std::string> types;  // string, number, bool, null

  bool has(const std::string& key) const { return values.count(key) != 0; }

  double num(const std::string& key) const { return std::stod(values.at(key)); }

  bool boolean(const std::string& key) const { return values.at(key) == "true"; }

  const std::string& str(const std::string& key) const { return values.at(key); }
};

inline void skip_ws(const std::string& s, size_t& i) {
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

inline std::string parse_string(const std::string& s, size_t& i) {
  if (i >= s.size() || s[i] != '"') throw std::runtime_error("json string");
  ++i;
  std::string out;
  while (i < s.size()) {
    char c = s[i++];
    if (c == '"') return out;
    if (c == '\\' && i < s.size()) {
      char n = s[i++];
      if (n == 'n') out.push_back('\n');
      else out.push_back(n);
      continue;
    }
    out.push_back(c);
  }
  throw std::runtime_error("unterminated json string");
}

inline Json parse_json(const std::string& s) {
  size_t i = 0;
  skip_ws(s, i);
  if (i >= s.size() || s[i] != '{') throw std::runtime_error("json object");
  ++i;
  Json j;
  skip_ws(s, i);
  if (i < s.size() && s[i] == '}') return j;
  while (i < s.size()) {
    skip_ws(s, i);
    std::string key = parse_string(s, i);
    skip_ws(s, i);
    if (i >= s.size() || s[i] != ':') throw std::runtime_error("json colon");
    ++i;
    skip_ws(s, i);
    if (i >= s.size()) throw std::runtime_error("json value");
    if (s[i] == '"') {
      j.values[key] = parse_string(s, i);
      j.types[key] = "string";
    } else if (s.compare(i, 4, "true") == 0) {
      j.values[key] = "true";
      j.types[key] = "bool";
      i += 4;
    } else if (s.compare(i, 5, "false") == 0) {
      j.values[key] = "false";
      j.types[key] = "bool";
      i += 5;
    } else if (s.compare(i, 4, "null") == 0) {
      j.values[key] = "null";
      j.types[key] = "null";
      i += 4;
    } else {
      size_t start = i;
      while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '-' || s[i] == '+' ||
                              s[i] == '.' || s[i] == 'e' || s[i] == 'E'))
        ++i;
      j.values[key] = s.substr(start, i - start);
      j.types[key] = "number";
    }
    skip_ws(s, i);
    if (i < s.size() && s[i] == ',') {
      ++i;
      continue;
    }
    if (i < s.size() && s[i] == '}') return j;
    throw std::runtime_error("json trailer");
  }
  throw std::runtime_error("json end");
}

}  // namespace drone_safety
