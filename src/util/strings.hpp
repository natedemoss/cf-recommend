// Small string helpers used across modules.
#pragma once

#include <string>
#include <vector>

namespace cfr {

inline std::string joinTags(const std::vector<std::string>& tags) {
    std::string out;
    for (std::size_t i = 0; i < tags.size(); ++i) {
        if (i) out += ';';
        out += tags[i];
    }
    return out;
}

inline std::vector<std::string> splitTags(const std::string& joined) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : joined) {
        if (c == ';') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

}  // namespace cfr
