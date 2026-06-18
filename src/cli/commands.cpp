#include "cli/commands.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include "analysis/analyzer.hpp"
#include "analysis/recommender.hpp"
#include "analysis/topics.hpp"
#include "api/codeforces_api.hpp"
#include "cli/output.hpp"
#include "db/database.hpp"

namespace cfr {

namespace {

// ---------------------------------------------------------------------------
// Small formatting helpers
// ---------------------------------------------------------------------------
std::string padRight(const std::string& s, std::size_t w) {
    return s.size() >= w ? s : s + std::string(w - s.size(), ' ');
}
std::string padLeft(const std::string& s, std::size_t w) {
    return s.size() >= w ? s : std::string(w - s.size(), ' ') + s;
}

std::string pct(double rate) {
    return std::to_string(static_cast<int>(std::lround(rate * 100))) + "%";
}

std::string relativeTime(long long when) {
    if (when == 0) return "never";
    long long now = static_cast<long long>(std::time(nullptr));
    long long d = now - when;
    if (d < 60) return "just now";
    if (d < 3600) return std::to_string(d / 60) + " min ago";
    if (d < 86400) return std::to_string(d / 3600) + " hr ago";
    return std::to_string(d / 86400) + " day(s) ago";
}

std::string problemUrl(const ProblemId& id) {
    return "https://codeforces.com/problemset/problem/" +
           std::to_string(id.contest_id) + "/" + id.index;
}

// Parses "1400-1600" or "1500" into [min, max]. Returns false on bad input.
bool parseRange(const std::string& s, int& lo, int& hi) {
    if (s.empty()) return false;
    auto dash = s.find('-');
    try {
        if (dash == std::string::npos) {
            lo = hi = std::stoi(s);
        } else {
            lo = std::stoi(s.substr(0, dash));
            hi = std::stoi(s.substr(dash + 1));
        }
    } catch (...) {
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Shared data access
// ---------------------------------------------------------------------------
struct SyncResult {
    int submissions = 0;
    int problems = 0;
};

SyncResult syncData(Database& db, const std::string& handle) {
    CodeforcesApi api;
    std::cout << out::dim("Fetching submissions for " + handle + "...") << "\n";
    auto subs = api.getUserSubmissions(handle);
    std::cout << out::dim("Fetching Codeforces problem set...") << "\n";
    auto probs = api.getProblemSet();

    db.replaceSubmissions(subs);
    db.replaceProblems(probs);
    db.setMeta("last_sync", std::to_string(std::time(nullptr)));

    return {static_cast<int>(subs.size()), static_cast<int>(probs.size())};
}

// Loads cached data, printing guidance and returning false if it's missing.
bool loadCache(const Database& db, std::vector<Submission>& subs,
               std::vector<Problem>& probs, std::string& handle) {
    handle = db.getMeta("handle");
    if (handle.empty()) {
        std::cerr << "No handle set. Run: "
                  << out::bold("cf login <handle>") << "\n";
        return false;
    }
    subs = db.loadSubmissions();
    probs = db.loadProblems();
    if (subs.empty()) {
        std::cerr << "No cached submissions. Run: "
                  << out::bold("cf sync") << "\n";
        return false;
    }
    long long last = std::atoll(db.getMeta("last_sync", "0").c_str());
    if (last && std::time(nullptr) - last > 86400) {
        std::cerr << out::dim("(cache last synced " + relativeTime(last) +
                              "; run `cf sync` to refresh)")
                  << "\n";
    }
    return true;
}

int userRating(const Database& db) {
    return std::atoi(db.getMeta("rating", "0").c_str());
}

// ---------------------------------------------------------------------------
// Topic breakdown rendering (shared by analyze)
// ---------------------------------------------------------------------------
std::string weaknessLabel(double rate) {
    if (rate < 0.40) return out::red("(very weak)");
    if (rate < 0.50) return out::yellow("(weak)");
    if (rate >= 0.80) return out::green("(strong)");
    return "";
}

}  // namespace

// ---------------------------------------------------------------------------
// Args
// ---------------------------------------------------------------------------
std::string Args::option(const std::string& name, const std::string& def) const {
    for (std::size_t i = 0; i < rest.size(); ++i) {
        const std::string& a = rest[i];
        if (a == name && i + 1 < rest.size()) return rest[i + 1];
        std::string prefix = name + "=";
        if (a.rfind(prefix, 0) == 0) return a.substr(prefix.size());
    }
    return def;
}

bool Args::has(const std::string& flag) const {
    return std::find(rest.begin(), rest.end(), flag) != rest.end();
}

std::string Args::positional(std::size_t i) const {
    std::size_t seen = 0;
    for (const auto& a : rest) {
        if (a.rfind("--", 0) == 0) continue;
        if (seen++ == i) return a;
    }
    return "";
}

// ---------------------------------------------------------------------------
// login
// ---------------------------------------------------------------------------
int runLogin(const Args& args) {
    std::string handle = args.positional(0);
    if (handle.empty()) {
        std::cerr << "Usage: cf login <handle>\n";
        return 2;
    }

    try {
        CodeforcesApi api;
        std::cout << out::dim("Verifying handle...") << "\n";
        UserInfo info = api.getUserInfo(handle);

        Database db(Database::defaultPath());
        db.setMeta("handle", info.handle);
        db.setMeta("rating", std::to_string(info.rating));
        db.setMeta("max_rating", std::to_string(info.max_rating));

        std::cout << out::green("Logged in as ") << out::bold(info.handle);
        if (info.rating) std::cout << "  (rating " << info.rating << ")";
        std::cout << "\n";

        if (!args.has("--no-sync")) {
            SyncResult r = syncData(db, info.handle);
            std::cout << out::green("Synced ") << r.submissions
                      << " submissions and " << r.problems << " problems.\n";
            std::cout << "Try: " << out::bold("cf analyze") << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << out::red("Login failed: ") << e.what() << "\n";
        return 1;
    }
}

// ---------------------------------------------------------------------------
// sync
// ---------------------------------------------------------------------------
int runSync(const Args&) {
    try {
        Database db(Database::defaultPath());
        std::string handle = db.getMeta("handle");
        if (handle.empty()) {
            std::cerr << "No handle set. Run: "
                      << out::bold("cf login <handle>") << "\n";
            return 2;
        }
        SyncResult r = syncData(db, handle);
        std::cout << out::green("Synced ") << r.submissions
                  << " submissions and " << r.problems << " problems.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << out::red("Sync failed: ") << e.what() << "\n";
        return 1;
    }
}

// ---------------------------------------------------------------------------
// analyze
// ---------------------------------------------------------------------------
int runAnalyze(const Args&) {
    try {
        Database db(Database::defaultPath());
        std::vector<Submission> subs;
        std::vector<Problem> probs;
        std::string handle;
        if (!loadCache(db, subs, probs, handle)) return 1;

        Analyzer analyzer(subs);
        Recommender rec(probs, analyzer, userRating(db));

        // Topics with enough signal, sorted strongest -> weakest.
        std::vector<TopicStat> rows;
        for (const auto& [tag, ts] : analyzer.topics())
            if (ts.attempted >= 3) rows.push_back(ts);
        std::sort(rows.begin(), rows.end(), [](const TopicStat& a, const TopicStat& b) {
            return a.success_rate() > b.success_rate();
        });

        std::cout << "\n" << out::bold(handle) << "  —  "
                  << analyzer.totalSolved() << " problems solved across "
                  << analyzer.totalAttempted() << " attempted\n";
        std::cout << out::dim("Adaptive solving level: ~" +
                              std::to_string(rec.solveLevel()) +
                              " (from your solve history)")
                  << "\n\n";

        std::cout << out::bold("Problem-Solving Breakdown") << "  "
                  << out::dim("(success rate · solved/attempted)") << "\n";
        for (const auto& ts : rows) {
            std::string rate = padLeft(pct(ts.success_rate()), 4);
            std::string frac =
                std::to_string(ts.solved) + "/" + std::to_string(ts.attempted);
            std::cout << "  " << padRight(displayTag(ts.tag), 26)
                      << out::ratePaint(rate, ts.success_rate()) << "   "
                      << padRight(frac, 9) << " " << weaknessLabel(ts.success_rate())
                      << "\n";
        }

        // Weak areas callout (< 50%).
        std::vector<TopicStat> weak;
        for (const auto& ts : rows)
            if (ts.success_rate() < 0.50) weak.push_back(ts);
        std::sort(weak.begin(), weak.end(), [](const TopicStat& a, const TopicStat& b) {
            return a.success_rate() < b.success_rate();
        });

        if (!weak.empty()) {
            std::cout << "\n" << out::bold("Weak areas") << " "
                      << out::dim("(below 50%)") << "\n";
            int i = 1;
            for (const auto& ts : weak) {
                std::cout << "  " << i++ << ". " << padRight(displayTag(ts.tag), 22)
                          << out::red(pct(ts.success_rate())) << "  ("
                          << ts.solved << "/" << ts.attempted << " solved)\n";
            }
            std::cout << "\nNext: " << out::bold("cf next --weak-topics-only")
                      << "\n";
        }
        std::cout << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << out::red("Error: ") << e.what() << "\n";
        return 1;
    }
}

// ---------------------------------------------------------------------------
// next
// ---------------------------------------------------------------------------
int runNext(const Args& args) {
    try {
        Database db(Database::defaultPath());
        std::vector<Submission> subs;
        std::vector<Problem> probs;
        std::string handle;
        if (!loadCache(db, subs, probs, handle)) return 1;

        RecOptions opts;
        opts.count = std::max(1, std::atoi(args.option("--count", "10").c_str()));
        opts.weak_topics_only = args.has("--weak-topics-only");

        std::string topic = args.option("--topic");
        if (!topic.empty()) opts.topic = canonicalTag(topic);

        // Max-rating precedence: an explicit --difficulty / --rating-range wins,
        // then a one-off --max-rating flag, then the saved config default.
        std::string range = args.option("--difficulty");
        if (range.empty()) range = args.option("--rating-range");
        bool explicit_range = !range.empty();
        if (explicit_range && !parseRange(range, opts.min_rating, opts.max_rating)) {
            std::cerr << "Invalid rating range: " << range
                      << " (use e.g. 1400-1600)\n";
            return 2;
        }
        if (!explicit_range) {
            std::string flag_max = args.option("--max-rating");
            std::string saved_max = db.getMeta("max_rating_pref");
            if (!flag_max.empty()) {
                opts.max_rating = std::atoi(flag_max.c_str());
            } else if (!saved_max.empty()) {
                opts.max_rating = std::atoi(saved_max.c_str());
            }
        }

        Analyzer analyzer(subs);
        Recommender rec(probs, analyzer, userRating(db));
        auto recs = rec.recommend(opts);

        if (recs.empty()) {
            std::cout << "No matching problems found. Try widening the filters "
                         "or running `cf sync`.\n";
            return 0;
        }

        std::cout << "\n" << out::bold("Top " + std::to_string(recs.size()) +
                                       " problems for " + handle)
                  << "  " << out::dim("(solving level ~" +
                                      std::to_string(rec.solveLevel()) + ")");
        if (opts.max_rating)
            std::cout << "  " << out::dim("[max " + std::to_string(opts.max_rating) +
                                          "]");
        std::cout << "\n\n";
        int i = 1;
        for (const auto& r : recs) {
            std::cout << out::bold(std::to_string(i++) + ". " + r.problem.id.key())
                      << "  " << out::dim("rating " + std::to_string(r.problem.rating));
            if (!r.primary_tag.empty())
                std::cout << "  ·  " << out::cyan(displayTag(r.primary_tag));
            if (r.is_weak_area) std::cout << " " << out::red("[weak area]");
            std::cout << "\n";
            std::cout << "   " << r.problem.name << "\n";
            std::cout << "   " << out::dim("Why: ") << r.reason << "\n";
            std::cout << "   " << out::dim("For you: ")
                      << r.difficulty_label << "  "
                      << out::dim("(feels like ~" + std::to_string(r.effective_rating) +
                                  ")")
                      << "   " << out::dim("Est. ")
                      << r.est_minutes_low << "-" << r.est_minutes_high << " min\n";
            std::cout << "   " << out::dim(problemUrl(r.problem.id)) << "\n\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << out::red("Error: ") << e.what() << "\n";
        return 1;
    }
}

// ---------------------------------------------------------------------------
// weak-topics
// ---------------------------------------------------------------------------
int runWeakTopics(const Args& args) {
    try {
        Database db(Database::defaultPath());
        std::vector<Submission> subs;
        std::vector<Problem> probs;
        std::string handle;
        if (!loadCache(db, subs, probs, handle)) return 1;

        int count = std::max(1, std::atoi(args.option("--count", "5").c_str()));

        Analyzer analyzer(subs);
        Recommender rec(probs, analyzer, userRating(db));
        auto weak = rec.rankedWeakTopics();

        // Keep only genuinely weak topics (below the recommendation threshold).
        weak.erase(std::remove_if(weak.begin(), weak.end(),
                                  [](const TopicStat& t) {
                                      return t.success_rate() >= kWeakThreshold;
                                  }),
                   weak.end());
        if (static_cast<int>(weak.size()) > count) weak.resize(count);

        if (weak.empty()) {
            std::cout << "No weak topics detected — nice work! Try "
                      << out::bold("cf next") << " to keep improving.\n";
            return 0;
        }

        std::cout << "\n" << out::bold("Top topics to focus on") << "\n\n";
        int i = 1;
        for (const auto& ts : weak) {
            int avail = rec.availableProblems(ts.tag);
            std::cout << out::bold(std::to_string(i++) + ". " + displayTag(ts.tag))
                      << "  " << out::red(pct(ts.success_rate()) + " success")
                      << "  " << out::dim("(" + std::to_string(ts.solved) + "/" +
                                          std::to_string(ts.attempted) + " solved)")
                      << "\n";
            std::cout << "   " << out::dim("Insight: ") << rec.topicInsight(ts.tag)
                      << "\n";
            std::cout << "   " << out::dim("Available unsolved problems: ") << avail
                      << "\n";
            std::cout << "   " << out::dim("Next: ")
                      << out::bold("cf next --topic \"" + ts.tag + "\"")
                      << "\n\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << out::red("Error: ") << e.what() << "\n";
        return 1;
    }
}

// ---------------------------------------------------------------------------
// progress
// ---------------------------------------------------------------------------
namespace {
struct WindowReport {
    int solved = 0;
    int submissions = 0;
    int accepted = 0;
    double avg_rating = 0.0;
    std::set<std::string> new_topics;
};

WindowReport computeWindow(const std::vector<Submission>& subs, long long cutoff,
                           const Analyzer& before) {
    WindowReport r;
    // First-OK time per problem, to count problems freshly solved in-window.
    std::map<ProblemId, long long> first_ok;
    for (const auto& s : subs) {
        if (s.verdict == "OK") {
            auto it = first_ok.find(s.problem);
            if (it == first_ok.end() || s.creation_time < it->second)
                first_ok[s.problem] = s.creation_time;
        }
    }
    long long rating_sum = 0;
    int rating_n = 0;
    std::map<ProblemId, int> solved_rating;
    for (const auto& [pid, t] : first_ok) {
        if (t >= cutoff) ++r.solved;
    }
    for (const auto& s : subs) {
        if (s.creation_time < cutoff) continue;
        ++r.submissions;
        if (s.verdict == "OK") {
            ++r.accepted;
            if (s.problem_rating > 0) solved_rating[s.problem] = s.problem_rating;
        }
        // A topic is "new" if it was never attempted before the window.
        for (const auto& tag : s.tags)
            if (before.topics().find(tag) == before.topics().end())
                r.new_topics.insert(tag);
    }
    for (const auto& [pid, rt] : solved_rating) {
        rating_sum += rt;
        ++rating_n;
    }
    if (rating_n) r.avg_rating = static_cast<double>(rating_sum) / rating_n;
    return r;
}

int computeStreak(const std::vector<Submission>& subs) {
    std::set<long long> days;
    for (const auto& s : subs) days.insert(s.creation_time / 86400);
    if (days.empty()) return 0;
    long long day = *days.rbegin();
    int streak = 0;
    while (days.count(day)) {
        ++streak;
        --day;
    }
    return streak;
}
}  // namespace

int runProgress(const Args&) {
    try {
        Database db(Database::defaultPath());
        std::vector<Submission> subs;
        std::vector<Problem> probs;
        std::string handle;
        if (!loadCache(db, subs, probs, handle)) return 1;

        long long now = static_cast<long long>(std::time(nullptr));
        long long cut7 = now - 7 * 86400;
        long long cut30 = now - 30 * 86400;

        // "before" = state of the world up to 7 days ago, for delta comparisons.
        std::vector<Submission> before7;
        for (const auto& s : subs)
            if (s.creation_time < cut7) before7.push_back(s);
        Analyzer beforeAnalyzer(before7);
        Analyzer nowAnalyzer(subs);

        WindowReport w7 = computeWindow(subs, cut7, beforeAnalyzer);
        WindowReport w30 = computeWindow(subs, cut30, Analyzer({}));

        std::cout << "\n" << out::bold(handle + "  —  practice progress") << "\n\n";

        std::cout << out::bold("Last 7 days") << "\n";
        std::cout << "  Problems solved : " << w7.solved << "\n";
        std::cout << "  Submissions     : " << w7.submissions << " ("
                  << (w7.submissions ? pct((double)w7.accepted / w7.submissions)
                                     : "0%")
                  << " accepted)\n";
        if (w7.avg_rating > 0)
            std::cout << "  Avg solved rating: "
                      << static_cast<int>(std::lround(w7.avg_rating)) << "\n";
        if (!w7.new_topics.empty()) {
            std::cout << "  New topics tried: ";
            bool first = true;
            for (const auto& t : w7.new_topics) {
                std::cout << (first ? "" : ", ") << displayTag(t);
                first = false;
            }
            std::cout << "\n";
        }

        std::cout << "\n" << out::bold("Last 30 days") << "\n";
        std::cout << "  Problems solved : " << w30.solved << "\n";
        std::cout << "  Submissions     : " << w30.submissions << "\n";

        // Topic improvements: success-rate delta vs 7 days ago.
        struct Delta { std::string tag; double before, now; };
        std::vector<Delta> deltas;
        for (const auto& [tag, ts] : nowAnalyzer.topics()) {
            if (ts.attempted < 5) continue;
            auto bit = beforeAnalyzer.topics().find(tag);
            if (bit == beforeAnalyzer.topics().end() ||
                bit->second.attempted < kMinReliableAttempts)
                continue;
            double d = ts.success_rate() - bit->second.success_rate();
            if (d > 0.001) deltas.push_back({tag, bit->second.success_rate(),
                                             ts.success_rate()});
        }
        std::sort(deltas.begin(), deltas.end(), [](const Delta& a, const Delta& b) {
            return (a.now - a.before) > (b.now - b.before);
        });
        if (!deltas.empty()) {
            std::cout << "\n" << out::bold("Improvements (last 7 days)") << "\n";
            for (std::size_t i = 0; i < deltas.size() && i < 5; ++i) {
                const auto& d = deltas[i];
                int delta_pp = static_cast<int>(std::lround((d.now - d.before) * 100));
                std::cout << "  " << padRight(displayTag(d.tag), 22)
                          << pct(d.before) << " -> " << pct(d.now) << "  "
                          << out::green("+" + std::to_string(delta_pp) + "%") << "\n";
            }
        }

        int streak = computeStreak(subs);
        long long last = nowAnalyzer.lastActivityTime();
        bool alive = last && (now / 86400 - last / 86400) <= 1;
        std::cout << "\n";
        if (alive) {
            std::cout << out::bold("Streak: ") << out::green(std::to_string(streak) +
                                                             " day(s)");
        } else {
            std::cout << out::bold("Streak: ") << "0 day(s) "
                      << out::dim("(broken — last streak was " +
                                  std::to_string(streak) + " day(s))");
        }
        std::cout << "   " << out::dim("(last active " + relativeTime(last) + ")")
                  << "\n\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << out::red("Error: ") << e.what() << "\n";
        return 1;
    }
}

// ---------------------------------------------------------------------------
// config
// ---------------------------------------------------------------------------
int runConfig(const Args& args) {
    try {
        Database db(Database::defaultPath());

        std::string key = args.positional(0);

        // No key: show current configuration.
        if (key.empty()) {
            std::string saved = db.getMeta("max_rating_pref");
            std::cout << "\n" << out::bold("Configuration") << "\n";
            std::cout << "  max-rating: "
                      << (saved.empty() ? out::dim("(not set)") : saved) << "\n";
            std::cout << "\nSet with: " << out::bold("cf config max-rating <N>")
                      << "  (or " << out::bold("off") << " to clear)\n\n";
            return 0;
        }

        if (key == "max-rating") {
            std::string value = args.positional(1);
            if (value.empty()) {
                std::string saved = db.getMeta("max_rating_pref");
                std::cout << "max-rating: "
                          << (saved.empty() ? "(not set)" : saved) << "\n";
                return 0;
            }
            if (value == "off" || value == "none" || value == "0") {
                db.setMeta("max_rating_pref", "");
                std::cout << out::green("Cleared max-rating.") << "\n";
                return 0;
            }
            int n = std::atoi(value.c_str());
            if (n < 100 || n > 4000) {
                std::cerr << "max-rating should be a Codeforces rating (100-4000), "
                             "or 'off'.\n";
                return 2;
            }
            db.setMeta("max_rating_pref", std::to_string(n));
            std::cout << out::green("Set max-rating to " + std::to_string(n))
                      << out::dim("  — `cf next` will only suggest problems at or "
                                  "below this rating.")
                      << "\n";
            return 0;
        }

        std::cerr << "Unknown config key: " << key << "\n";
        std::cerr << "Known keys: max-rating\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << out::red("Error: ") << e.what() << "\n";
        return 1;
    }
}

// ---------------------------------------------------------------------------
// help / dispatch
// ---------------------------------------------------------------------------
int printHelp() {
    std::cout <<
        out::bold("cf") << " — Codeforces practice recommender\n\n"
        "Usage: cf <command> [options]\n\n"
        << out::bold("Commands:\n")
        << "  login <handle>        Verify a handle and download history\n"
           "  sync                  Refresh cached submissions & problem set\n"
           "  analyze               Show your per-topic success breakdown\n"
           "  next [options]        Recommend problems to solve next\n"
           "  weak-topics           Show your weakest topics with insights\n"
           "  progress              Show recent practice progress & streak\n"
           "  config [key value]    View or change settings (e.g. max-rating)\n"
           "  help                  Show this help\n\n"
        << out::bold("`next` options:\n")
        << "  --count N             Number of problems (default 10)\n"
           "  --topic NAME          Filter by topic (e.g. dp, greedy, binsearch)\n"
           "  --difficulty A-B      Rating range, e.g. 1400-1600\n"
           "  --rating-range A-B    Alias for --difficulty\n"
           "  --max-rating N        Cap this run's problem rating at N\n"
           "  --weak-topics-only    Only problems touching your weak topics\n\n"
        << out::dim("Recommendations adapt to your solving level (from your solve "
                    "history),\nnot just your contest rating.\n\n")
        << "Examples:\n"
        "  cf login tourist\n"
        "  cf config max-rating 1700      # cap all recommendations at 1700\n"
        "  cf next --topic greedy --difficulty 1400-1600 --count 5\n";
    return 0;
}

int dispatch(int argc, char** argv) {
    Args args;
    if (argc >= 2) args.command = argv[1];
    for (int i = 2; i < argc; ++i) args.rest.push_back(argv[i]);

    const std::string& c = args.command;
    if (c.empty() || c == "help" || c == "--help" || c == "-h") return printHelp();
    if (c == "login") return runLogin(args);
    if (c == "sync") return runSync(args);
    if (c == "analyze") return runAnalyze(args);
    if (c == "next") return runNext(args);
    if (c == "weak-topics") return runWeakTopics(args);
    if (c == "progress") return runProgress(args);
    if (c == "config") return runConfig(args);

    std::cerr << "Unknown command: " << c << "\n";
    std::cerr << "Run `cf help` for usage.\n";
    return 2;
}

}  // namespace cfr
