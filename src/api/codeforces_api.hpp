// Typed client for the public Codeforces API (https://codeforces.com/apiHelp).
#pragma once

#include <string>
#include <vector>

#include "http/http_client.hpp"
#include "json/json.hpp"
#include "model/models.hpp"

namespace cfr {

class CodeforcesApi {
public:
    CodeforcesApi() = default;

    // GET /user.info — validates the handle and returns rating info.
    // Throws std::runtime_error if the handle does not exist.
    UserInfo getUserInfo(const std::string& handle);

    // GET /user.status — the user's entire submission history (newest first).
    std::vector<Submission> getUserSubmissions(const std::string& handle);

    // GET /problemset.problems — the full problem set with solve counts.
    std::vector<Problem> getProblemSet();

private:
    HttpClient http_;

    // Fetches a URL, parses JSON, verifies status=="OK", returns the "result".
    json::Value fetchResult(const std::string& url);
};

}  // namespace cfr
