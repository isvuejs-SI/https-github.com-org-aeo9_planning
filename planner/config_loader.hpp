#pragma once
// Minimal YAML subset: nested maps, integers, and strings. No external library.
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Tree {
    std::map<std::string, Tree> child;
    std::map<std::string, std::string> scalar;
};

inline void parse_yaml(const std::string& text, Tree& root) {
    struct Frame {
        int ind;
        Tree* node;
    };
    std::vector<Frame> st;
    st.push_back({-1, &root});
    std::size_t i = 0;
    while (i < text.size()) {
        std::size_t e = text.find('\n', i);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(i, e - i);
        i = e < text.size() ? e + 1 : text.size();
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        std::size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        while (!line.empty() && (line[line.size() - 1] == ' ' || line[line.size() - 1] == '\t'))
            line.erase(line.size() - 1);
        if (line.find_first_not_of(' ') == std::string::npos) continue;
        int ind = 0;
        while (ind < (int)line.size() && line[ind] == ' ') ind++;
        std::string body = line.substr(ind);
        std::size_t col = body.find(':');
        if (col == std::string::npos) throw std::runtime_error("bad yaml");
        std::string key = body.substr(0, col);
        std::string val = col + 1 < body.size() ? body.substr(col + 1) : "";
        std::size_t a = 0;
        while (a < val.size() && val[a] == ' ') a++;
        val = val.substr(a);
        while (st.size() > 1 && ind <= st.back().ind) st.pop_back();
        Tree* parent = st.back().node;
        if (val.empty()) {
            parent->child[key] = Tree();
            st.push_back({ind, &parent->child[key]});
        } else {
            parent->scalar[key] = val;
        }
    }
}

inline Tree load_file(const std::string& path) {
    std::ifstream in(path.c_str());
    if (!in) throw std::runtime_error(path);
    std::stringstream buf;
    buf << in.rdbuf();
    Tree root;
    parse_yaml(buf.str(), root);
    return root;
}

inline const std::string& scalar_at(const Tree& root, const std::string& path) {
    const Tree* cur = &root;
    std::size_t start = 0;
    while (true) {
        std::size_t dot = path.find('.', start);
        std::string part = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (dot == std::string::npos) {
            std::map<std::string, std::string>::const_iterator it = cur->scalar.find(part);
            if (it == cur->scalar.end()) throw std::runtime_error(path);
            return it->second;
        }
        std::map<std::string, Tree>::const_iterator it = cur->child.find(part);
        if (it == cur->child.end()) throw std::runtime_error(path);
        cur = &it->second;
        start = dot + 1;
    }
}

inline int get_int(const Tree& root, const std::string& path) {
    return std::stoi(scalar_at(root, path));
}

inline std::string get_str(const Tree& root, const std::string& path) {
    return scalar_at(root, path);
}
