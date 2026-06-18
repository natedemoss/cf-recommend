// Scores and ranks unsolved problems for a user based on their weak topics,
// rating fit, problem popularity, and variety.
#pragma once

#include <string>
#include <vector>

#include "analysis/analyzer.hpp"
#include "model/models.hpp"

namespace cfr {

// A topic is considered "weak" when its success rate is below this threshold.
constexpr double kWeakThreshold = 0.70;

// Minimum attempts before a topic's success rate is treated as reliable. Below
// this, one unlucky problem shouldn't brand a whole topic as a weakness.
constexpr int kMinReliableAttempts = 3;

struct RecOptions {
    int count = 10;
    std::string topic;            // canonical tag filter; empty = any topic
    int min_rating = 0;           // inclusive lower bound; 0 = no bound
    int max_rating = 0;           // inclusive upper bound; 0 = no bound
    bool weak_topics_only = false;
};

struct Recommendation {
    Problem problem;
    double score = 0.0;
    std::string primary_tag;      // weak/driving tag for this recommendation
    double primary_weakness = 0;  // 1 - success_rate of primary_tag
    bool is_weak_area = false;    // primary_tag is a known weak topic
    int effective_rating = 0;     // difficulty as it likely feels to this user
    std::string difficulty_label; // "Easy" / "Medium" / "Hard" (for the user)
    int est_minutes_low = 0;
    int est_minutes_high = 0;
    std::string reason;
};

class Recommender {
public:
    Recommender(const std::vector<Problem>& problems,
                const Analyzer& analyzer,
                int user_rating);

    std::vector<Recommendation> recommend(const RecOptions& opts) const;

    // Topics sorted from weakest to strongest, restricted to topics the user
    // has actually attempted at least `min_attempts` times.
    std::vector<TopicStat> rankedWeakTopics(int min_attempts = 3) const;

    // A short, heuristic insight about why a topic is weak (verdict patterns).
    std::string topicInsight(const std::string& tag) const;

    // Count of unsolved, rated problems available for a tag.
    int availableProblems(const std::string& tag) const;

    // The user's adaptive "solving level": derived from the ratings of problems
    // they actually solve, blended with their Codeforces rating. This is what
    // recommendations and difficulty labels are centred on.
    int solveLevel() const { return solve_level_; }

private:
    const std::vector<Problem>& problems_;
    const Analyzer& analyzer_;
    int cf_rating_;          // raw Codeforces rating (0 if unrated)
    int solve_level_ = 0;    // adaptive practice level
    long long max_solved_count_ = 1;
    std::map<std::string, int> recent_tag_freq_;  // tags in recent solves

    double weakness(const std::string& tag) const;
    Recommendation scoreProblem(const Problem& p) const;
    int computeSolveLevel() const;
};

}  // namespace cfr
