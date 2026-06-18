// Implementations of the cf-recommend subcommands.
#pragma once

#include <string>
#include <vector>

namespace cfr {

// Parsed command line: the subcommand plus its remaining arguments.
struct Args {
    std::string command;
    std::vector<std::string> rest;

    // Returns the value of "--flag value" or "--flag=value"; empty if absent.
    std::string option(const std::string& name, const std::string& def = "") const;
    bool has(const std::string& flag) const;
    std::string positional(std::size_t i) const;
};

int runLogin(const Args& args);
int runSync(const Args& args);
int runAnalyze(const Args& args);
int runNext(const Args& args);
int runWeakTopics(const Args& args);
int runProgress(const Args& args);
int runConfig(const Args& args);
int printHelp();

// Dispatches argv to the right command. Returns a process exit code.
int dispatch(int argc, char** argv);

}  // namespace cfr
