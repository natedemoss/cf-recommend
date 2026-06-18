#include "analysis/recommender.hpp"

#include <algorithm>
#include <cmath>

#include "analysis/topics.hpp"

namespace cfr {

namespace {
// Score component weights (sum to 1.0).
constexpr double kWeightWeakness = 0.40;
constexpr double kWeightRatingFit = 0.30;
constexpr double kWeightFrequency = 0.20;
constexpr double kWeightVariety = 0.10;

constexpr int kDefaultRating = 1400;  // assumed level for unrated users
constexpr int kRecentWindow = 25;     // recent solves considered for variety
}  // namespace

Recommender::Recommender(const std::vector<Problem>& problems,
                         const Analyzer& analyzer, int user_rating)
    : problems_(problems), analyzer_(analyzer), cf_rating_(user_rating) {
    for (const auto& p : problems_)
        max_solved_count_ = std::max(max_solved_count_, p.solved_count);
    recent_tag_freq_ = analyzer_.recentSolvedTagFreq(kRecentWindow);
    solve_level_ = computeSolveLevel();
}

int Recommender::computeSolveLevel() const {
    // Collect the ratings of problems the user has actually solved.
    std::vector<int> solved_ratings;
    for (const auto& p : problems_)
        if (p.rating > 0 && analyzer_.isSolved(p.id))
            solved_ratings.push_back(p.rating);

    // Too little signal to adapt: fall back to the Codeforces rating.
    if (solved_ratings.size() < 12)
        return cf_rating_ > 0 ? cf_rating_ : kDefaultRating;

    // The 80th percentile of solved ratings reflects the level the user can
    // already reach; practising a little above it is the sweet spot.
    std::sort(solved_ratings.begin(), solved_ratings.end());
    std::size_t idx = static_cast<std::size_t>((solved_ratings.size() - 1) * 0.80);
    int p80 = solved_ratings[idx];

    // Blend with the Codeforces rating when available so a single lucky solve
    // doesn't overstate ability; otherwise trust the demonstrated level.
    if (cf_rating_ > 0)
        return static_cast<int>(std::lround(0.5 * cf_rating_ + 0.5 * p80));
    return p80;
}

double Recommender::weakness(const std::string& tag) const {
    const auto& topics = analyzer_.topics();
    auto it = topics.find(tag);
    // Unknown or thinly-sampled topics get a modest, fixed value: enough to
    // encourage exploration, but never enough to outrank a genuine weak area.
    if (it == topics.end() || it->second.attempted < kMinReliableAttempts) {
        return 0.30;
    }
    return 1.0 - it->second.success_rate();
}

Recommendation Recommender::scoreProblem(const Problem& p) const {
    Recommendation rec;
    rec.problem = p;

    // Pick the weakest tag to drive the recommendation's framing.
    double best_weakness = -1.0;
    for (const auto& tag : p.tags) {
        double w = weakness(tag);
        if (w > best_weakness) {
            best_weakness = w;
            rec.primary_tag = tag;
        }
    }
    // Untagged problems can't be reasoned about topically; treat them as mild,
    // neutral challenges rather than fake "weak areas".
    rec.primary_weakness = p.tags.empty() ? 0.15 : best_weakness;

    // --- score components ---
    double weak_score = rec.primary_weakness;

    double gap = static_cast<double>(p.rating - solve_level_);
    double rating_fit = std::exp(-(gap * gap) / (2.0 * 350.0 * 350.0));

    double freq_score =
        std::log(1.0 + p.solved_count) / std::log(1.0 + max_solved_count_);

    int overlap = 0;
    for (const auto& tag : p.tags)
        if (recent_tag_freq_.count(tag)) ++overlap;
    double variety = p.tags.empty()
                         ? 1.0
                         : 1.0 - static_cast<double>(overlap) / p.tags.size();

    rec.score = kWeightWeakness * weak_score + kWeightRatingFit * rating_fit +
                kWeightFrequency * freq_score + kWeightVariety * variety;

    // Effective difficulty: a weak topic makes a problem feel harder, so we add
    // up to +200 rating for the user's weakest involved tag.
    rec.effective_rating = p.rating + static_cast<int>(std::lround(rec.primary_weakness * 200));

    int diff = rec.effective_rating - solve_level_;
    if (diff <= -100) {
        rec.difficulty_label = "Easy";
        rec.est_minutes_low = 15;
        rec.est_minutes_high = 25;
    } else if (diff <= 150) {
        rec.difficulty_label = "Medium";
        rec.est_minutes_low = 25;
        rec.est_minutes_high = 40;
    } else {
        rec.difficulty_label = "Hard";
        rec.est_minutes_low = 40;
        rec.est_minutes_high = 60;
    }

    // Reason text.
    const auto& topics = analyzer_.topics();
    auto it = topics.find(rec.primary_tag);
    bool reliable = it != topics.end() && it->second.attempted >= kMinReliableAttempts;
    bool untried = it == topics.end() || it->second.attempted == 0;
    std::string tagName = displayTag(rec.primary_tag);
    if (p.tags.empty()) {
        rec.reason = "Untagged problem near your level — a fresh challenge.";
    } else if (reliable && it->second.success_rate() < kWeakThreshold) {
        rec.is_weak_area = true;
        int pct = static_cast<int>(std::lround(it->second.success_rate() * 100));
        rec.reason = "You're only " + std::to_string(pct) + "% proficient in " +
                     tagName + " — focused practice on a weak area.";
    } else if (untried) {
        rec.reason = "New topic for you (" + tagName +
                     ") — broadens your coverage.";
    } else {
        rec.reason = "Solid " + tagName + " problem near your level.";
    }
    return rec;
}

std::vector<Recommendation> Recommender::recommend(const RecOptions& opts) const {
    std::vector<Recommendation> recs;

    for (const auto& p : problems_) {
        if (p.rating == 0) continue;                 // need a rating to estimate
        if (analyzer_.isSolved(p.id)) continue;       // only recommend unsolved
        if (opts.min_rating && p.rating < opts.min_rating) continue;
        if (opts.max_rating && p.rating > opts.max_rating) continue;

        if (!opts.topic.empty()) {
            if (std::find(p.tags.begin(), p.tags.end(), opts.topic) == p.tags.end())
                continue;
        }

        if (opts.weak_topics_only) {
            bool touches_weak = false;
            for (const auto& tag : p.tags) {
                auto it = analyzer_.topics().find(tag);
                if (it != analyzer_.topics().end() &&
                    it->second.attempted >= kMinReliableAttempts &&
                    it->second.success_rate() < kWeakThreshold) {
                    touches_weak = true;
                    break;
                }
            }
            if (!touches_weak) continue;
        }

        recs.push_back(scoreProblem(p));
    }

    std::sort(recs.begin(), recs.end(),
              [](const Recommendation& a, const Recommendation& b) {
                  if (a.score != b.score) return a.score > b.score;
                  return a.problem.solved_count > b.problem.solved_count;
              });

    if (static_cast<int>(recs.size()) > opts.count) recs.resize(opts.count);
    return recs;
}

std::vector<TopicStat> Recommender::rankedWeakTopics(int min_attempts) const {
    std::vector<TopicStat> v;
    for (const auto& [tag, ts] : analyzer_.topics()) {
        if (ts.attempted >= min_attempts) v.push_back(ts);
    }
    std::sort(v.begin(), v.end(), [](const TopicStat& a, const TopicStat& b) {
        if (a.success_rate() != b.success_rate())
            return a.success_rate() < b.success_rate();
        return a.attempted > b.attempted;
    });
    return v;
}

std::string Recommender::topicInsight(const std::string& tag) const {
    auto vit = analyzer_.verdicts().find(tag);
    auto tit = analyzer_.topics().find(tag);
    if (vit == analyzer_.verdicts().end() || tit == analyzer_.topics().end())
        return "Not enough data yet — start solving to build a profile.";

    const auto& verds = vit->second;
    int total = 0;
    for (const auto& [v, c] : verds) total += c;
    if (total == 0) return "Not enough data yet.";

    auto frac = [&](const std::string& v) {
        auto it = verds.find(v);
        return it == verds.end() ? 0.0 : static_cast<double>(it->second) / total;
    };

    double wa = frac("WRONG_ANSWER");
    double tle = frac("TIME_LIMIT_EXCEEDED");

    if (tit->second.attempted < 5) {
        return "You've barely attempted this topic — build fundamentals first.";
    }
    if (wa >= 0.45) {
        return "Lots of wrong answers — slow down on edge cases and correctness.";
    }
    if (tle >= 0.20) {
        return "Frequent TLEs — focus on complexity and optimisation tricks.";
    }
    if (tit->second.success_rate() < 0.4) {
        return "Low solve rate — drop a rating band and rebuild confidence.";
    }
    return "Solid base, but room to grow — push into a slightly higher band.";
}

int Recommender::availableProblems(const std::string& tag) const {
    int n = 0;
    for (const auto& p : problems_) {
        if (p.rating == 0 || analyzer_.isSolved(p.id)) continue;
        if (std::find(p.tags.begin(), p.tags.end(), tag) != p.tags.end()) ++n;
    }
    return n;
}

}  // namespace cfr
