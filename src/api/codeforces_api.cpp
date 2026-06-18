#include "api/codeforces_api.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <map>
#include <stdexcept>
#include <thread>

namespace cfr {

namespace {
std::vector<std::string> readTags(const json::Value& problem) {
    std::vector<std::string> tags;
    for (const auto& t : problem["tags"].as_array()) {
        if (t.is_string()) tags.push_back(t.as_string());
    }
    return tags;
}

ProblemId readProblemId(const json::Value& problem) {
    ProblemId id;
    id.contest_id = problem["contestId"].as_int();
    id.index = problem["index"].as_string();
    return id;
}

bool mentionsRateLimit(const std::string& comment) {
    std::string lc;
    lc.reserve(comment.size());
    for (char c : comment) lc.push_back(static_cast<char>(std::tolower((unsigned char)c)));
    return lc.find("limit") != std::string::npos;
}
}  // namespace

json::Value CodeforcesApi::fetchResult(const std::string& url) {
    // Codeforces rate-limits to roughly one call every two seconds. Retry a few
    // times with backoff so a burst of calls (e.g. during login) doesn't fail.
    constexpr int kMaxAttempts = 4;
    std::string last_error = "request failed";

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        if (attempt > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500 * attempt));
        }

        HttpResponse resp;
        try {
            resp = http_.get(url);
        } catch (const std::exception& e) {
            last_error = e.what();  // transport error: retry
            continue;
        }

        json::Value root;
        try {
            root = json::Value::parse(resp.body);
        } catch (const std::exception& e) {
            if (resp.status_code == 503 || resp.status_code == 429) {
                last_error = "Codeforces temporarily unavailable (HTTP " +
                             std::to_string(resp.status_code) + ")";
                continue;  // server busy: retry
            }
            if (!resp.ok()) {
                last_error = "Codeforces API returned HTTP " +
                             std::to_string(resp.status_code);
                continue;
            }
            throw std::runtime_error(
                std::string("could not parse API response: ") + e.what());
        }

        if (root["status"].as_string() == "OK") return root["result"];

        std::string comment = root["comment"].as_string();
        if (mentionsRateLimit(comment)) {
            last_error = comment;  // hit the call limit: back off and retry
            continue;
        }
        // A genuine API error (e.g. bad handle) — don't retry.
        throw std::runtime_error("Codeforces API error: " +
                                 (comment.empty() ? "unknown" : comment));
    }
    throw std::runtime_error(last_error);
}

UserInfo CodeforcesApi::getUserInfo(const std::string& handle) {
    json::Value result =
        fetchResult("https://codeforces.com/api/user.info?handles=" + handle);

    if (!result.is_array() || result.size() == 0) {
        throw std::runtime_error("no user info returned for handle '" + handle + "'");
    }
    const json::Value& u = result.as_array().front();

    UserInfo info;
    info.handle = u["handle"].as_string();
    info.rating = static_cast<int>(u["rating"].as_int());
    info.max_rating = static_cast<int>(u["maxRating"].as_int());
    info.rank = u["rank"].as_string();
    return info;
}

std::vector<Submission> CodeforcesApi::getUserSubmissions(const std::string& handle) {
    // count is intentionally huge to grab the full history in one request.
    json::Value result = fetchResult(
        "https://codeforces.com/api/user.status?handle=" + handle +
        "&from=1&count=1000000000");

    std::vector<Submission> out;
    out.reserve(result.size());
    for (const auto& s : result.as_array()) {
        const json::Value& problem = s["problem"];
        // Some submissions (e.g. to gym/unknown problems) lack a contestId.
        if (!problem["contestId"].is_number()) continue;

        Submission sub;
        sub.id = s["id"].as_int();
        sub.problem = readProblemId(problem);
        sub.creation_time = s["creationTimeSeconds"].as_int();
        sub.verdict = s["verdict"].as_string();
        sub.problem_rating = static_cast<int>(problem["rating"].as_int());
        sub.tags = readTags(problem);
        out.push_back(std::move(sub));
    }
    return out;
}

std::vector<Problem> CodeforcesApi::getProblemSet() {
    json::Value result = fetchResult("https://codeforces.com/api/problemset.problems");

    // Build solve-count lookup from problemStatistics first.
    std::map<std::string, long long> solved;
    for (const auto& st : result["problemStatistics"].as_array()) {
        ProblemId id = readProblemId(st);
        solved[id.key()] = st["solvedCount"].as_int();
    }

    std::vector<Problem> out;
    out.reserve(result["problems"].size());
    for (const auto& p : result["problems"].as_array()) {
        Problem prob;
        prob.id = readProblemId(p);
        prob.name = p["name"].as_string();
        prob.rating = static_cast<int>(p["rating"].as_int());
        prob.tags = readTags(p);
        auto it = solved.find(prob.id.key());
        prob.solved_count = it == solved.end() ? 0 : it->second;
        out.push_back(std::move(prob));
    }
    return out;
}

}  // namespace cfr
