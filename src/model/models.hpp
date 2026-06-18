// Core domain types shared across the API client, database, and recommender.
#pragma once

#include <string>
#include <vector>

namespace cfr {

// A problem's stable identity on Codeforces is its contest id plus its index
// within that contest, e.g. contest 1203, index "D".
struct ProblemId {
    long long contest_id = 0;
    std::string index;

    // Human-facing key, e.g. "1203D".
    std::string key() const { return std::to_string(contest_id) + index; }

    bool operator<(const ProblemId& o) const {
        if (contest_id != o.contest_id) return contest_id < o.contest_id;
        return index < o.index;
    }
    bool operator==(const ProblemId& o) const {
        return contest_id == o.contest_id && index == o.index;
    }
};

struct Problem {
    ProblemId id;
    std::string name;
    int rating = 0;                  // 0 == unrated / unknown
    std::vector<std::string> tags;   // canonical Codeforces tags
    long long solved_count = 0;      // how many users solved it (popularity)
};

struct Submission {
    long long id = 0;
    ProblemId problem;
    long long creation_time = 0;     // unix seconds
    std::string verdict;             // "OK", "WRONG_ANSWER", "TIME_LIMIT_EXCEEDED", ...
    int problem_rating = 0;          // rating snapshot carried by the submission
    std::vector<std::string> tags;   // tags snapshot carried by the submission
};

struct UserInfo {
    std::string handle;
    int rating = 0;            // current rating, 0 if unrated
    int max_rating = 0;
    std::string rank;
};

// Aggregated practice statistics for a single topic (Codeforces tag).
struct TopicStat {
    std::string tag;
    int solved = 0;        // distinct problems with at least one accepted submission
    int attempted = 0;     // distinct problems attempted
    int submissions = 0;   // total submissions touching this tag

    double success_rate() const {
        return attempted == 0 ? 0.0 : static_cast<double>(solved) / attempted;
    }
};

}  // namespace cfr
