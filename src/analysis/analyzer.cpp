#include "analysis/analyzer.hpp"

#include <algorithm>

namespace cfr {

namespace {
// Collapses a problem's submissions into "was it ever solved" plus its tags.
struct ProblemAgg {
    bool solved = false;
    long long solve_time = 0;  // earliest accepted time
    std::vector<std::string> tags;
};
}  // namespace

Analyzer::Analyzer(const std::vector<Submission>& submissions) {
    // First pass: aggregate per distinct problem so proficiency is measured in
    // problems (not raw submissions), and collect a per-tag verdict histogram.
    std::map<ProblemId, ProblemAgg> per_problem;
    for (const auto& sub : submissions) {
        // Skip submissions without a final verdict (still judging) so they don't
        // count as failed attempts.
        if (sub.verdict.empty() || sub.verdict == "TESTING") continue;

        ProblemAgg& agg = per_problem[sub.problem];
        if (agg.tags.empty() && !sub.tags.empty()) agg.tags = sub.tags;
        if (sub.verdict == "OK") {
            if (!agg.solved || sub.creation_time < agg.solve_time)
                agg.solve_time = sub.creation_time;
            agg.solved = true;
        }
        last_activity_ = std::max(last_activity_, sub.creation_time);

        for (const auto& tag : sub.tags) {
            ++topics_[tag].submissions;
            ++verdicts_[tag][sub.verdict];
        }
    }

    // Second pass: turn per-problem results into per-tag solved/attempted counts.
    for (const auto& [pid, agg] : per_problem) {
        attempted_.insert(pid);
        if (agg.solved) {
            solved_.insert(pid);
            solved_events_.emplace_back(agg.solve_time, agg.tags);
        }
        for (const auto& tag : agg.tags) {
            TopicStat& ts = topics_[tag];
            ts.tag = tag;
            ++ts.attempted;
            if (agg.solved) ++ts.solved;
        }
    }

    std::sort(solved_events_.begin(), solved_events_.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
}

std::map<std::string, int> Analyzer::recentSolvedTagFreq(int n) const {
    std::map<std::string, int> freq;
    int taken = 0;
    for (const auto& [time, tags] : solved_events_) {
        if (taken++ >= n) break;
        for (const auto& tag : tags) ++freq[tag];
    }
    return freq;
}

double Analyzer::successRate(const std::string& tag) const {
    auto it = topics_.find(tag);
    return it == topics_.end() ? 0.0 : it->second.success_rate();
}

}  // namespace cfr
