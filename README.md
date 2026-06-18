<div align="center">

# cf-recommend

**Know exactly what to practice next on Codeforces.** A fast, local-first CLI that analyzes your submission history and recommends problems tailored to your weak topics.

[![npm version](https://img.shields.io/npm/v/cf-recommend.svg?color=cb3837&logo=npm)](https://www.npmjs.com/package/cf-recommend)
[![npm downloads](https://img.shields.io/npm/dm/cf-recommend.svg?color=cb3837)](https://www.npmjs.com/package/cf-recommend)
[![license](https://img.shields.io/npm/l/cf-recommend.svg?color=blue)](LICENSE)
![platforms](https://img.shields.io/badge/platform-windows%20%7C%20macos%20%7C%20linux-555)

```sh
npm install -g cf-recommend
```

No browser. No account linking. No C++ compiler. Just your handle.

</div>

---

## Features

-  **Weak-topic targeting** — finds tags you struggle with
-  **Adaptive difficulty** — recommendations based on your solving level
-  **Explains itself** — every suggestion comes with a reason
-  **Local-first** — cached in SQLite, instant, works offline
-  **Zero-friction** — prebuilt binary via npm

##  Demo

```text
$ cf analyze
  tourist  —  616 problems solved across 683 attempted
  Adaptive solving level: ~1650

  Problem-Solving Breakdown  (success rate · solved/attempted)
    Math                  92%   291/315
    Greedy                90%   300/334
    DP                    84%   75/89
    Binary Search         82%   54/66
    Graphs                69%   11/16
    Divide And Conquer    63%   5/8     (weak)

$ cf next --weak-topics-only --count 1
  1. 1167B  rating 1400  ·  Divide And Conquer [weak area]
     Lost Numbers
     Why: You're only 63% proficient in Divide And Conquer.
     For you: Medium  (feels like ~1475)   Est. 25-40 min
     https://codeforces.com/problemset/problem/1167/B
```

## Quick Start

```sh
cf login <your-handle>     # download & cache your history
cf analyze                 # per-topic breakdown
cf next                    # get recommendations
cf weak-topics             # your weakest topics
cf progress                # recent activity & streak
```

## Commands

| Command | Description |
|---|---|
| `cf login <handle>` | Verify handle & download history |
| `cf sync` | Re-fetch submissions & problem set |
| `cf analyze` | Per-topic success-rate breakdown |
| `cf next [options]` | Recommend problems to solve |
| `cf weak-topics` | Weakest topics with insights |
| `cf progress` | Recent activity & improvements |
| `cf config [key value]` | View or change settings |

### `next` Command Options

| Flag | Meaning |
|---|---|
| `--count N` | Problems to show (default 10) |
| `--topic NAME` | Filter by topic (`dp`, `greedy`, `graphs`, etc.) |
| `--difficulty A-B` | Rating range (e.g., `1400-1600`) |
| `--max-rating N` | Cap problem rating at N |
| `--weak-topics-only` | Only problems from weak topics |

Topic names are flexible: `binsearch`, `binary-search`, and `binary search` all work.

### Examples

```sh
cf next --weak-topics-only --count 10
cf next --topic dp --difficulty 1400-1600 --count 5
cf config max-rating 1700
```

## How It Works

**Adaptive Solving Level:** Based on the 80th percentile of your solved ratings (blended 50/50 with your Codeforces rating). Falls back to your CF rating or 1400 for new accounts.

**Proficiency:** Aggregated per-problem (retries don't distort rates). A topic needs ≥3 attempted problems to be reliable; below 70% is weak.

**Recommendation Score:** Each unsolved problem receives a score between 0 and 1:
```
0.40 × topic_weakness + 0.30 × rating_fit + 0.20 × popularity + 0.10 × variety
```

**Per-User Difficulty:** Weak topics add up to +200 to a problem's effective rating for you.

## Data & Caching

- Data from public Codeforces API with automatic rate-limit retry
- Cached in SQLite: `~/.cf-recommend/` (Unix) or `%LOCALAPPDATA%\cf-recommend\` (Windows)
- `analyze`, `next`, `weak-topics`, `progress` work offline; run `cf sync` to refresh

## Install from Source

Requires C++17 compiler + **libcurl** & **sqlite3** development libraries.

<details>
<summary><b>Windows (MSYS2 / UCRT64)</b></summary>

```sh
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-curl mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-pkgconf
mingw32-make
```
</details>

<details>
<summary><b>Linux / macOS</b></summary>

```sh
# Debian/Ubuntu:  sudo apt install g++ libcurl4-openssl-dev libsqlite3-dev
# macOS:          xcode-select --install
make
sudo cp cf /usr/local/bin/
```
</details>

<details>
<summary><b>CMake</b></summary>

```sh
cmake -S . -B build && cmake --build build
```
</details>

## Contributing & Roadmap

See [CONTRIBUTING.md](CONTRIBUTING.md) for dev setup. Good first areas: smarter ranking, new filters, platform testing.

**Planned:** Friend comparison · Division-aware filtering · Learning paths · Export functionality

##  License

[MIT](LICENSE) © nathandemoss

<div align="center">
<sub>Built for the competitive programming community. If it helps you, give it a ⭐.</sub>
</div>
