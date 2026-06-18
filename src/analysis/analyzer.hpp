// Aggregates a user's submission history into per-topic proficiency stats.
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "model/models.hpp"

namespace cfr {

class Analyzer {
public:
    // Builds all aggregates from the given submissions. Pass a filtered subset
    // (e.g. submissions before a cutoff) to analyse a historical window.
    explicit Analyzer(const std::vector<Submission>& submissions);

    // Per-tag proficiency, keyed by canonical Codeforces tag.
    const std::map<std::string, TopicStat>& topics() const { return topics_; }

    // Distinct problems with at least one accepted submission.
    const std::set<ProblemId>& solved() const { return solved_; }
    bool isSolved(const ProblemId& id) const { return solved_.count(id) > 0; }
    int totalSolved() const { return static_cast<int>(solved_.size()); }
    int totalAttempted() const { return static_cast<int>(attempted_.size()); }

    // Verdict histogram per tag, e.g. verdicts().at("dp")["WRONG_ANSWER"].
    const std::map<std::string, std::map<std::string, int>>& verdicts() const {
        return verdicts_;
    }

    // Success rate for a single tag (0 if no attempts recorded).
    double successRate(const std::string& tag) const;

    // Tag frequencies among the most recently solved `n` problems. Used by the
    // recommender to favour variety over the user's recent practice.
    std::map<std::string, int> recentSolvedTagFreq(int n) const;

    // Unix time of the most recent submission (0 if none).
    long long lastActivityTime() const { return last_activity_; }

private:
    std::map<std::string, TopicStat> topics_;
    std::map<std::string, std::map<std::string, int>> verdicts_;
    std::set<ProblemId> solved_;
    std::set<ProblemId> attempted_;
    // (solve_time, tags) per solved problem, sorted newest-first.
    std::vector<std::pair<long long, std::vector<std::string>>> solved_events_;
    long long last_activity_ = 0;
};

}  // namespace cfr
