// Helpers for mapping between user-friendly topic names and Codeforces tags.
#pragma once

#include <string>

namespace cfr {

// Resolves a user-supplied topic name (e.g. "binsearch", "graph", "DP") to the
// canonical Codeforces tag (e.g. "binary search", "graphs", "dp"). Unknown
// inputs are returned lowercased and trimmed so they still match raw tags.
std::string canonicalTag(const std::string& input);

// Pretty display name for a canonical tag, e.g. "dp" -> "DP",
// "binary search" -> "Binary Search".
std::string displayTag(const std::string& tag);

}  // namespace cfr
