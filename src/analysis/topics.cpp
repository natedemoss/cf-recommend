#include "analysis/topics.hpp"

#include <algorithm>
#include <cctype>
#include <map>

namespace cfr {

namespace {
std::string toLowerTrim(const std::string& in) {
    std::string s;
    s.reserve(in.size());
    for (char c : in) s.push_back(static_cast<char>(std::tolower((unsigned char)c)));
    auto not_space = [](char c) { return !std::isspace((unsigned char)c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

// Friendly aliases -> canonical Codeforces tags. Several common spellings and
// shorthands map onto each tag so filters like `--topic binsearch` just work.
const std::map<std::string, std::string>& aliasMap() {
    static const std::map<std::string, std::string> m = {
        {"dp", "dp"},
        {"dynamic programming", "dp"},
        {"greedy", "greedy"},
        {"math", "math"},
        {"maths", "math"},
        {"graph", "graphs"},
        {"graphs", "graphs"},
        {"strings", "strings"},
        {"string", "strings"},
        {"binsearch", "binary search"},
        {"binary-search", "binary search"},
        {"binary search", "binary search"},
        {"ds", "data structures"},
        {"data-structures", "data structures"},
        {"data structures", "data structures"},
        {"number-theory", "number theory"},
        {"number theory", "number theory"},
        {"nt", "number theory"},
        {"two-pointers", "two pointers"},
        {"two pointers", "two pointers"},
        {"dsu", "dsu"},
        {"geometry", "geometry"},
        {"bitmask", "bitmasks"},
        {"bitmasks", "bitmasks"},
        {"constructive", "constructive algorithms"},
        {"constructive algorithms", "constructive algorithms"},
        {"brute-force", "brute force"},
        {"bruteforce", "brute force"},
        {"brute force", "brute force"},
        {"implementation", "implementation"},
        {"combinatorics", "combinatorics"},
        {"trees", "trees"},
        {"tree", "trees"},
        {"sorting", "sortings"},
        {"sortings", "sortings"},
        {"shortest-paths", "shortest paths"},
        {"shortest paths", "shortest paths"},
        {"dfs", "dfs and similar"},
        {"flows", "flows"},
        {"hashing", "hashing"},
        {"games", "games"},
        {"probabilities", "probabilities"},
    };
    return m;
}
}  // namespace

std::string canonicalTag(const std::string& input) {
    std::string key = toLowerTrim(input);
    auto it = aliasMap().find(key);
    return it == aliasMap().end() ? key : it->second;
}

std::string displayTag(const std::string& tag) {
    // A few tags read better fully upper-cased.
    static const std::map<std::string, std::string> special = {
        {"dp", "DP"},
        {"dsu", "DSU"},
        {"fft", "FFT"},
    };
    auto it = special.find(tag);
    if (it != special.end()) return it->second;

    std::string out = tag;
    bool start_of_word = true;
    for (char& c : out) {
        if (start_of_word && std::isalpha((unsigned char)c)) {
            c = static_cast<char>(std::toupper((unsigned char)c));
            start_of_word = false;
        } else if (c == ' ' || c == '-') {
            start_of_word = true;
        }
    }
    return out;
}

}  // namespace cfr
