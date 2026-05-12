<p align="center">
  <img src="docs/marriage_pact.png" alt="Marriage Pact Logo" />
</p>

# Assignment 1: Marriage Pact

## Overview

Happy assignment 1! This is your warm-up with the STL — but with teeth. You'll touch **vectors**, **iterators**, **ranges**, **projections**, and the single most common iterator pitfall in C++: **invalidation**.

These are the files you need to care about:

- `main.cpp` — all your code goes here 😀
- `short_answer.txt` — short answer responses go here 📝

To download the starter code for this assignment, please see the instructions for [**Getting Started**](../README.md#getting-started) on the course assignments repository.

## Running your code

Make sure you are in the `assignment1/` directory and run:

```sh
g++ -std=c++20 main.cpp -o main
```

You can then run the binary in four modes:

```sh
./main "Marceline McMillan"   # find one match for a single name
./main --mixer                # run the multi-round mixer over everyone
./main --test                 # run every test in tests/utils.hpp
./main --test run_mixer       # run a single named test
```

The tests live in `tests/utils.hpp` and check invariants on your implementations (e.g. every pair from `run_mixer` shares initials, no duplicates across pairs, the leftover pool is maximal). Use `--test` as your feedback loop while you work.

> [!NOTE]
>
> ### Note for Windows
>
> On Windows, you may need to compile with `-static-libstdc++`, and the executable may be called `main.exe`:
>
> ```sh
> g++ -static-libstdc++ -std=c++20 main.cpp -o main
> ./main.exe "Marceline McMillan"
> ```

## The story so far

You’ve been waiting for days to get your Marriage Pact initials this year, and they’ve finally arrived in your inbox! This year, they’re implementing a new rule: your match MUST share your own initials to be eligible. There are thousands of students on campus — but you’re in CS106L, and you remember that C++ has a pretty quick way to chew through this kind of data: containers.

`students.txt` contains the (fictional) applicants — one `First Last` per line.

> [!NOTE]
> All names appearing in this assignment are fictitious. Any resemblance to real persons, living or dead, is purely coincidental.

---

## Part 1: `get_applicants`

> [!IMPORTANT]
> ### `get_applicants`
>
> Read `filename` line by line and return a `std::vector<std::string>` of applicant names.
>
> **Requirements**
> - Take `filename` as `const std::string&`.
> - Call `reserve()` on the vector *before* you start pushing, with a sensible starting capacity. Explain your choice in `short_answer.txt`.

## Part 2: `initials`

> [!IMPORTANT]
> ### `initials`
>
> Helper. Given a name like `"Marceline McMillan"`, return its initials (`"MM"`), uppercased.
>
> **Requirements**
> - Take the parameter as `std::string_view` (no allocation).
> - Use this helper from `find_matches` and `run_mixer`.

## Part 3: `find_matches`

> [!IMPORTANT]
> ### `find_matches`
>
> Return every applicant in `students` whose initials match those of `name`.
>
> **Requirements**
> - **No raw `for` loops.** Use a `std::ranges` algorithm such as `std::ranges::copy_if`, or a `views::filter` pipeline. Use a **projection** (`&` to a lambda or a projection argument) where it makes the call clearer.
> - Take `students` as `const std::vector<std::string>&`.
> - See: <https://en.cppreference.com/w/cpp/algorithm/ranges>

## Part 4: `get_match`

> [!IMPORTANT]
> ### `get_match`
>
> Return one randomly-chosen match. Return the literal string `"NO MATCHES FOUND."` if the input is empty.
>
> **Requirements**
> - Use `std::sample` with a seeded `std::mt19937`. `std::sample` is iterator-based — that's the point.
> - Do **not** use `pop_back()` or `rand() % size`.

## Part 5: `run_mixer` 💃🕺

The Marriage Pact mixer is at Main Quad tonight, and matches are being paired off two-by-two until everyone with a partner has found one. Your job: simulate it.

> [!IMPORTANT]
> ### `run_mixer`
>
> Walk the pool of applicants left-to-right. For each applicant, search the remaining pool for another applicant with the same initials. If you find one, pair them, **remove both** from the pool, and record the pair. Keep doing rounds until a full pass produces no new pairs. The input vector is mutated — the leftovers at the end are the lonely hearts.
>
> Signature:
> ```cpp
> std::vector<std::pair<std::string, std::string>>
> run_mixer(std::vector<std::string>& applicants);
> ```
>
> **Requirements**
> - The naive implementation — "iterate the vector, erase a pair when you find one, keep going" — **will invalidate your iterator** and either crash or silently skip names. Fix this by one of:
>   1. Using the iterator returned by `erase()`,
>   2. Walking by index with care, or
>   3. Re-starting the scan each time you remove a pair.
> - Explain which strategy you chose, and why, in `short_answer.txt`.

---

## Short answer questions

Answer briefly (2–4 sentences each) in `short_answer.txt`:

1. **`reserve` vs `resize`** — what's the difference, and why did you choose the capacity you did in `get_applicants`?
2. **Projection** — what does it mean for a `std::ranges` algorithm to take a projection? Give one example from your `find_matches` (or explain why a projection didn't help there).
3. **Iterator invalidation in `run_mixer`** — which strategy did you choose for handling the erase? What specifically goes wrong with the naive approach?
4. **`at` vs `[]`** — when, if ever, would you use `vec.at(i)` over `vec[i]` in this assignment?

## Admin instructions

To test the matching mode: `./main "<name to test>"`. To test the mixer: `./main --mixer`.

## 🚀 Submission Instructions

Commit your changes to the GitHub repository.

Your deliverable should be:

- `main.cpp`
- `short_answer.txt`

*Adapted from Stanford CS106L.*
