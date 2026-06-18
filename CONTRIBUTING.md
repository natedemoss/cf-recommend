# Contributing to cf-recommend

Thanks for your interest in improving **cf-recommend**!  Whether it's a bug
report, a new feature, better recommendation heuristics, or docs — contributions
are very welcome.

## Table of contents

- [Ways to contribute](#ways-to-contribute)
- [Development setup](#development-setup)
- [Project layout](#project-layout)
- [Building & running](#building--running)
- [Coding guidelines](#coding-guidelines)
- [Submitting a change](#submitting-a-change)
- [Reporting bugs](#reporting-bugs)
- [Feature ideas](#feature-ideas)

## Ways to contribute

-  **Report bugs** — open an issue with steps to reproduce.
-  **Suggest features** — new commands, filters, or smarter ranking.
-  **Improve the algorithm** — the recommendation score and difficulty model
  are heuristics; better ones are very welcome (with reasoning).
-  **Docs** — clarify the README, add examples, fix typos.
-  **Packaging** — help test the prebuilt binaries on more platforms.

## Development setup

You need a C++17 compiler plus the **libcurl** and **sqlite3** development
libraries.

**Windows (MSYS2 / UCRT64)**

```sh
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-curl \
          mingw-w64-ucrt-x86_64-sqlite3 \
          mingw-w64-ucrt-x86_64-pkgconf
```

Build from a UCRT64 shell (or with `C:\msys64\ucrt64\bin` on your PATH). Note the
make binary is `mingw32-make`, and use `mingw32-make`, **not** `cmake` — the
project builds cleanly with the provided `Makefile`.

**Linux**

```sh
sudo apt-get install g++ make libcurl4-openssl-dev libsqlite3-dev
```

**macOS**

```sh
xcode-select --install   # libcurl + sqlite3 ship with the SDK
```

## Project layout

```
src/
  json/        minimal dependency-free JSON parser
  http/        libcurl wrapper
  api/         typed Codeforces API client
  model/       domain types (Problem, Submission, …)
  db/          SQLite cache
  analysis/    analyzer, recommender, topic helpers
  cli/         command implementations + output formatting
  main.cpp     entry point
bin/cf.js      Node launcher for the npm package
.github/       release workflow
```

## Building & running

```sh
mingw32-make           # Windows  (or: make, on Linux/macOS)
./cf.exe help          # ./cf on Linux/macOS
```

Try a full flow against the live API:

```sh
./cf.exe login tourist
./cf.exe analyze
./cf.exe next --weak-topics-only
```

The local cache lives in `%LOCALAPPDATA%\cf-recommend\` (Windows) or
`~/.cf-recommend/` (Unix); delete it to start fresh.

## Coding guidelines

- **C++17**, no extra third-party dependencies beyond libcurl and sqlite3.
- Match the surrounding style: 4-space indent, `lowerCamelCase` functions,
  `snake_case` locals, `namespace cfr`.
- Keep the build **warning-free** (`-Wall -Wextra`).
- Prefer small, focused functions and clear comments explaining *why*, not *what*.
- New user-facing output should respect `NO_COLOR` and stay readable when piped.

## Submitting a change

1. Fork the repo and create a branch: `git checkout -b my-feature`.
2. Make your change; keep commits focused.
3. Build and run a few commands to confirm nothing regressed.
4. Open a pull request describing **what** changed and **why**. Screenshots of
   CLI output are great for UX changes.

Small PRs are easiest to review and merge. If you're planning something large,
open an issue first so we can discuss the approach.

## Reporting bugs

Include:

- The command you ran and the full output.
- Your OS and how you installed (`npm` or from source).
- Your Codeforces handle if relevant (the data is public).

## Feature ideas

Some directions on the roadmap — jump in on any of these:

- Friend / rival comparison (`cf compare <handle>`).
- Contest-division awareness in filtering.
- Per-tag "learning path" ordering.
- Export recommendations to a file or clipboard.

Happy hacking!
