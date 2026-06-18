<div align="center">

# cf-recommend

### Know exactly what to practice next on Codeforces.

A fast, local-first **CLI** that analyses your Codeforces submission history and
recommends the best problems to solve next — laser-focused on your weak topics.

[![npm version](https://img.shields.io/npm/v/cf-recommend.svg?color=cb3837&logo=npm)](https://www.npmjs.com/package/cf-recommend)
[![npm downloads](https://img.shields.io/npm/dm/cf-recommend.svg?color=cb3837)](https://www.npmjs.com/package/cf-recommend)
[![license](https://img.shields.io/npm/l/cf-recommend.svg?color=blue)](LICENSE)
![platforms](https://img.shields.io/badge/platform-windows%20%7C%20macos%20%7C%20linux-555)
[![PRs welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](CONTRIBUTING.md)

```sh
npm install -g cf-recommend
```

No browser. No account linking. No C++ compiler. Just your handle.

</div>

---

##  Why cf-recommend?

Every competitive programmer hits the same wall: **"what should I solve next?"**
Existing tools are web-based and generic. cf-recommend is built for people who
live in the terminal and want *personalised, data-driven* practice.

-  **Weak-topic targeting** — it finds the tags you actually struggle with.
-  **Adaptive difficulty** — recommendations centre on a *solving level*
  inferred from the problems you really solve, not a static rating.
-  **Explains itself** — every suggestion comes with a reason.
-  **Local-first** — cached in SQLite, instant, works offline.
-  **Zero-friction install** — prebuilt binary via npm.

## 🎬 Demo

```text
$ cf analyze

  tourist  —  616 problems solved across 683 attempted
  Adaptive solving level: ~1650 (from your solve history)

  Problem-Solving Breakdown  (success rate · solved/attempted)
    Math                  92%   291/315
    Greedy                90%   300/334
    DP                    84%   75/89
    Binary Search         82%   54/66
    Graphs                69%   11/16
    Divide And Conquer    63%   5/8     (weak)

  Weak areas (below 50%) ...

$ cf next --weak-topics-only --count 1

  1. 1167B  rating 1400  ·  Divide And Conquer [weak area]
     Lost Numbers
     Why: You're only 63% proficient in Divide And Conquer — focused practice on a weak area.
     For you: Medium  (feels like ~1475)   Est. 25-40 min
     https://codeforces.com/problemset/problem/1167/B
```

##  Install

```sh
npm install -g cf-recommend
```

Installs the `cf` command (plus a `cf-recommend` alias). Prebuilt binaries ship
for **Windows x64**, **Linux x64**, and **macOS x64 & arm64** — no compiler
needed. Prefer to build it yourself? See [Build from source](#-build-from-source).

> **Heads up:** the command is `cf`. If you also use the Cloud Foundry CLI (also
> `cf`), use the `cf-recommend` alias instead.

##  Quick start

```sh
cf login <your-handle>     # download & cache your history (public data)
cf analyze                 # see your per-topic breakdown
cf next                    # get problem recommendations
cf weak-topics             # focus areas with insights
cf progress                # recent activity, improvements, streak
```

##  Commands

| Command | Description |
|---|---|
| `cf login <handle>` | Verify a handle and download its full history |
| `cf sync` | Re-fetch submissions & the problem set |
| `cf analyze` | Per-topic success-rate breakdown |
| `cf next [options]` | Recommend problems to solve next |
| `cf weak-topics` | Your weakest topics, with insights |
| `cf progress` | Recent activity, improvements, and streak |
| `cf config [key value]` | View or change settings (e.g. `max-rating`) |
| `cf help` | Show usage |

### `next` options

| Flag | Meaning |
|---|---|
| `--count N` | How many problems to show (default 10) |
| `--topic NAME` | Filter by topic — `dp`, `greedy`, `binsearch`, `graphs`, … |
| `--difficulty A-B` | Rating range, e.g. `1400-1600` (single value also works) |
| `--rating-range A-B` | Alias for `--difficulty` |
| `--max-rating N` | Cap this run's problem rating at N |
| `--weak-topics-only` | Only problems that touch your weak topics |

Topic names are flexible: `binsearch`, `binary-search`, and `binary search` all
resolve to the same Codeforces tag.

### Set a permanent rating cap

```sh
cf config max-rating 1700   # cap every recommendation at 1700
cf config max-rating off    # remove the cap
cf config                   # show current settings
```

Precedence for `next`: explicit `--difficulty` / `--rating-range` → one-off
`--max-rating` → saved `config max-rating` default.

### Examples

```sh
cf next --weak-topics-only --count 10
cf next --topic dp --difficulty 1400-1600 --count 5
cf weak-topics
cf progress
```

##  How it works

### Adaptive solving level

Recommendations centre on a *solving level* rather than your raw contest rating.
Once you have at least 12 rated solves, it's the **80th percentile of the ratings
you've solved**, blended 50/50 with your Codeforces rating:

```
solving_level = round(0.5 * cf_rating + 0.5 * p80(solved problem ratings))
```

With less history it falls back to your Codeforces rating (or 1400 for a new
account). Both `cf analyze` and `cf next` show it.

### Proficiency

Submissions are aggregated **per problem** (not per submission), so retries and
upsolves don't distort the numbers. A problem counts as *solved* if it ever got
an `OK` verdict; in-progress submissions are ignored.

```
success_rate(topic) = solved problems in topic / attempted problems in topic
```

A topic needs at least **3 attempted problems** before its rate is treated as
reliable, and topics below **70%** are considered weak.

### Recommendation score

Each unsolved, rated problem gets a score in `[0, 1]`:

```
score = 0.40 * topic_weakness      // 1 - success_rate of its weakest tag
      + 0.30 * rating_fit          // Gaussian around your solving level
      + 0.20 * popularity          // log-scaled solve count (a proxy for "standard")
      + 0.10 * variety             // penalises tags you've drilled very recently
```

### Per-user difficulty

A weak topic makes a problem *feel* harder, so the effective rating adds up to
+200 for your weakest involved tag:

```
effective_rating = problem_rating + round(weakness * 200)
```

The `Easy / Medium / Hard` label and time estimate compare that effective rating
to your solving level.

##  Data & caching

- Data comes from the public Codeforces API (`user.info`, `user.status`,
  `problemset.problems`), with automatic retry on rate limits.
- Cached in a SQLite database under `%LOCALAPPDATA%\cf-recommend\` (Windows) or
  `~/.cf-recommend/` (Unix).
- `analyze`, `next`, `weak-topics`, and `progress` read only from the cache, so
  they work offline. Run `cf sync` to refresh.

##  Build from source

Only needed if you're not installing via npm. Requires a C++17 compiler plus the
**libcurl** and **sqlite3** development libraries. The resulting binary is `cf`
(`cf.exe` on Windows).

<details>
<summary><b>Windows (MSYS2 / UCRT64)</b></summary>

```sh
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-curl \
          mingw-w64-ucrt-x86_64-sqlite3 \
          mingw-w64-ucrt-x86_64-pkgconf

# from a UCRT64 shell (or with C:\msys64\ucrt64\bin on PATH):
mingw32-make
```
</details>

<details>
<summary><b>Linux / macOS</b></summary>

```sh
# Debian/Ubuntu:  sudo apt install g++ libcurl4-openssl-dev libsqlite3-dev
# macOS:          xcode-select --install
make
sudo cp cf /usr/local/bin/   # optional: put it on PATH
```
</details>

<details>
<summary><b>CMake</b></summary>

```sh
cmake -S . -B build && cmake --build build
```
</details>

##  Contributing

Contributions are very welcome — see **[CONTRIBUTING.md](CONTRIBUTING.md)** for
dev setup, project layout, and guidelines. Good first areas: smarter ranking
heuristics, new filters, and platform testing.

##  Roadmap

- [ ] Friend / rival comparison (`cf compare <handle>`)
- [ ] Contest-division awareness in filtering
- [ ] Per-tag "learning path" ordering
- [ ] Export recommendations to a file

##  License

[MIT](LICENSE) © nathandemoss

<div align="center">
<sub>Built for the competitive programming community. If it helps you, give it a ⭐.</sub>
</div>
