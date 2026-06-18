// SQLite-backed local cache of problems, submissions, and sync metadata.
#pragma once

#include <string>
#include <vector>

#include "model/models.hpp"

struct sqlite3;

namespace cfr {

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Key/value metadata (handle, ratings, last sync time).
    void setMeta(const std::string& key, const std::string& value);
    std::string getMeta(const std::string& key,
                        const std::string& fallback = "") const;

    // Bulk replace: wipes the table and re-inserts, inside one transaction.
    void replaceProblems(const std::vector<Problem>& problems);
    void replaceSubmissions(const std::vector<Submission>& submissions);

    std::vector<Problem> loadProblems() const;
    std::vector<Submission> loadSubmissions() const;

    // Returns the standard cache file path (e.g. %LOCALAPPDATA%\cf-recommend).
    static std::string defaultPath();

private:
    void exec(const std::string& sql);
    void initSchema();

    sqlite3* db_ = nullptr;
};

}  // namespace cfr
