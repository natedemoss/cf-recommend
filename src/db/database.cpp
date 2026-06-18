#include "db/database.hpp"

#include <sqlite3.h>

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#include "util/strings.hpp"

namespace cfr {

namespace {
// RAII wrapper for a prepared statement.
class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(std::string("SQL prepare failed: ") +
                                     sqlite3_errmsg(db));
        }
    }
    ~Stmt() { sqlite3_finalize(stmt_); }
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    sqlite3_stmt* get() { return stmt_; }

    void bindInt(int i, long long v) { sqlite3_bind_int64(stmt_, i, v); }
    void bindText(int i, const std::string& v) {
        sqlite3_bind_text(stmt_, i, v.c_str(), -1, SQLITE_TRANSIENT);
    }
    int step() { return sqlite3_step(stmt_); }
    void reset() {
        sqlite3_reset(stmt_);
        sqlite3_clear_bindings(stmt_);
    }

    long long colInt(int i) { return sqlite3_column_int64(stmt_, i); }
    std::string colText(int i) {
        const unsigned char* p = sqlite3_column_text(stmt_, i);
        return p ? reinterpret_cast<const char*>(p) : "";
    }

private:
    sqlite3_stmt* stmt_ = nullptr;
};
}  // namespace

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
        sqlite3_close(db_);
        throw std::runtime_error("cannot open database '" + path + "': " + msg);
    }
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
    initSchema();
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("SQL error: " + msg);
    }
}

void Database::initSchema() {
    exec(R"sql(
        CREATE TABLE IF NOT EXISTS meta (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS problems (
            contest_id   INTEGER NOT NULL,
            idx          TEXT NOT NULL,
            name         TEXT NOT NULL,
            rating       INTEGER NOT NULL,
            tags         TEXT NOT NULL,
            solved_count INTEGER NOT NULL,
            PRIMARY KEY (contest_id, idx)
        );
        CREATE TABLE IF NOT EXISTS submissions (
            id             INTEGER PRIMARY KEY,
            contest_id     INTEGER NOT NULL,
            idx            TEXT NOT NULL,
            creation_time  INTEGER NOT NULL,
            verdict        TEXT NOT NULL,
            problem_rating INTEGER NOT NULL,
            tags           TEXT NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_sub_problem
            ON submissions (contest_id, idx);
    )sql");
}

void Database::setMeta(const std::string& key, const std::string& value) {
    Stmt s(db_, "INSERT INTO meta(key,value) VALUES(?,?) "
                "ON CONFLICT(key) DO UPDATE SET value=excluded.value;");
    s.bindText(1, key);
    s.bindText(2, value);
    if (s.step() != SQLITE_DONE)
        throw std::runtime_error("failed to write meta key");
}

std::string Database::getMeta(const std::string& key,
                              const std::string& fallback) const {
    Stmt s(db_, "SELECT value FROM meta WHERE key=?;");
    s.bindText(1, key);
    if (s.step() == SQLITE_ROW) return s.colText(0);
    return fallback;
}

void Database::replaceProblems(const std::vector<Problem>& problems) {
    exec("BEGIN;");
    try {
        exec("DELETE FROM problems;");
        Stmt s(db_, "INSERT OR REPLACE INTO problems"
                    "(contest_id,idx,name,rating,tags,solved_count) "
                    "VALUES(?,?,?,?,?,?);");
        for (const auto& p : problems) {
            s.bindInt(1, p.id.contest_id);
            s.bindText(2, p.id.index);
            s.bindText(3, p.name);
            s.bindInt(4, p.rating);
            s.bindText(5, joinTags(p.tags));
            s.bindInt(6, p.solved_count);
            if (s.step() != SQLITE_DONE)
                throw std::runtime_error("failed to insert problem");
            s.reset();
        }
        exec("COMMIT;");
    } catch (...) {
        exec("ROLLBACK;");
        throw;
    }
}

void Database::replaceSubmissions(const std::vector<Submission>& submissions) {
    exec("BEGIN;");
    try {
        exec("DELETE FROM submissions;");
        Stmt s(db_, "INSERT OR REPLACE INTO submissions"
                    "(id,contest_id,idx,creation_time,verdict,problem_rating,tags) "
                    "VALUES(?,?,?,?,?,?,?);");
        for (const auto& sub : submissions) {
            s.bindInt(1, sub.id);
            s.bindInt(2, sub.problem.contest_id);
            s.bindText(3, sub.problem.index);
            s.bindInt(4, sub.creation_time);
            s.bindText(5, sub.verdict);
            s.bindInt(6, sub.problem_rating);
            s.bindText(7, joinTags(sub.tags));
            if (s.step() != SQLITE_DONE)
                throw std::runtime_error("failed to insert submission");
            s.reset();
        }
        exec("COMMIT;");
    } catch (...) {
        exec("ROLLBACK;");
        throw;
    }
}

std::vector<Problem> Database::loadProblems() const {
    std::vector<Problem> out;
    Stmt s(db_, "SELECT contest_id,idx,name,rating,tags,solved_count "
                "FROM problems;");
    while (s.step() == SQLITE_ROW) {
        Problem p;
        p.id.contest_id = s.colInt(0);
        p.id.index = s.colText(1);
        p.name = s.colText(2);
        p.rating = static_cast<int>(s.colInt(3));
        p.tags = splitTags(s.colText(4));
        p.solved_count = s.colInt(5);
        out.push_back(std::move(p));
    }
    return out;
}

std::vector<Submission> Database::loadSubmissions() const {
    std::vector<Submission> out;
    Stmt s(db_, "SELECT id,contest_id,idx,creation_time,verdict,problem_rating,tags "
                "FROM submissions;");
    while (s.step() == SQLITE_ROW) {
        Submission sub;
        sub.id = s.colInt(0);
        sub.problem.contest_id = s.colInt(1);
        sub.problem.index = s.colText(2);
        sub.creation_time = s.colInt(3);
        sub.verdict = s.colText(4);
        sub.problem_rating = static_cast<int>(s.colInt(5));
        sub.tags = splitTags(s.colText(6));
        out.push_back(std::move(sub));
    }
    return out;
}

std::string Database::defaultPath() {
    namespace fs = std::filesystem;
    fs::path dir;
    if (const char* local = std::getenv("LOCALAPPDATA")) {
        dir = fs::path(local) / "cf-recommend";
    } else if (const char* home = std::getenv("HOME")) {
        dir = fs::path(home) / ".cf-recommend";
    } else {
        dir = fs::current_path() / ".cf-recommend";
    }
    std::error_code ec;
    fs::create_directories(dir, ec);
    return (dir / "cache.db").string();
}

}  // namespace cfr
