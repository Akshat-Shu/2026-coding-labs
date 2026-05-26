/* ========================================================================= *
 * Don't worry about anything beyond this point.                             *
 * (unless you are really curious and want to!)                              *
 * ========================================================================= */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace ansi {

const int style_idx = std::ios_base::xalloc();

template <class CharT, class Traits>
constexpr std::basic_ostream<CharT, Traits>& styled(std::basic_ostream<CharT, Traits>& os) {
  os.iword(style_idx) = 1;
  return os;
}

template <class CharT, class Traits>
constexpr std::basic_ostream<CharT, Traits>& unstyled(std::basic_ostream<CharT, Traits>& os) {
  os.iword(style_idx) = 0;
  return os;
}

#define make_style(NAME, VALUE)                                                                    \
  template <class CharT, class Traits>                                                             \
  constexpr std::basic_ostream<CharT, Traits>& NAME(std::basic_ostream<CharT, Traits>& os) {       \
    if (os.iword(style_idx) > 0)                                                                   \
      os << VALUE;                                                                                 \
    return os;                                                                                     \
  }

make_style(reset, "\033[0m\e[0m");
make_style(fg_red, "\033[31m");
make_style(fg_lightred, "\033[91m");
make_style(fg_green, "\033[92m");
make_style(fg_gray, "\033[90m");
make_style(bg_yellow, "\e[43m");

} // namespace ansi

std::string read_stream(std::istream& is) {
  std::istreambuf_iterator<char> begin(is), end;
  return std::string(begin, end);
}

struct TimerResult {
  std::string name;
  size_t trials;
  std::chrono::nanoseconds ns;
};

class TimerSummary {
public:
  TimerSummary() : trial_noun{"trial"}, enabled{false} {}
  void add(const TimerResult& result) { results.push_back(result); }
  void set_trial_noun(const std::string& trial_noun) { this->trial_noun = trial_noun; }

  TimerSummary& operator=(const TimerSummary&) = delete;
  ~TimerSummary() {
    if (!enabled)
      return;

    std::cout << "\n";
    std::cout << ansi::bg_yellow << std::left << std::setw(120) << "Timing Results:" << ansi::reset << '\n';
    std::cout << ansi::fg_gray;
    for (const auto& [name, trials, ns] : results) {
      std::cout << " · " << name << " took ";
      format_time(ns);
      if (trials > 1) {
        std::cout << ", averaging ";
        format_time(ns / trials);
        std::cout << " per " << trial_noun;
      }
      std::cout << " (" << trials << " " << trial_noun;
      if (trials != 1) std::cout << "s";
      std::cout << ")\n";
    }
    std::cout << ansi::reset;
  }

  void enable() { enabled = true; }
  void disable() { enabled = false; }

private:
  bool enabled;
  std::string trial_noun;
  std::vector<TimerResult> results;

  void format_time(const std::chrono::nanoseconds& ns) {
    using namespace std::chrono;

    if (ns < 10us) {
      std::cout << ns.count() << "ns";
    } else if (ns < 1s) {
      std::cout << std::fixed << std::setprecision(2)
                << duration_cast<microseconds>(ns).count() / 1000.0 << "ms" << std::defaultfloat;
    } else {
      std::cout << std::fixed << std::setprecision(3)
                << duration_cast<milliseconds>(ns).count() / 1000.0 << "s" << std::defaultfloat;
    }
  }
};

class Timer {
public:
  Timer(TimerSummary& summary, const std::string& name, size_t trials = 1)
      : summary{summary}, name{name}, trials{trials},
        start{std::chrono::high_resolution_clock::now()}, stopped{false} {}

  ~Timer() {
    stop();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    summary.add({name, trials, ns});
  }

  Timer& operator=(const Timer&) = delete;

  void set_trials(size_t trials) { this->trials = trials; }

  void stop() {
    if (stopped)
      return;
    stopped = true;
    end = std::chrono::high_resolution_clock::now();
  }

private:
  std::string name;
  size_t trials;
  std::chrono::high_resolution_clock::time_point start;
  std::chrono::high_resolution_clock::time_point end;
  bool stopped;

  TimerSummary& summary;
};

/* ---- Test runner ---- */

#include <algorithm>
#include <map>
#include "../spellcheck.h"

namespace tests {

inline int failures = 0;

inline void check(bool cond, const std::string& msg) {
  std::cout << "  " << (cond ? "PASS: " : "FAIL: ") << msg << "\n";
  if (!cond) ++failures;
}

inline void test_tokenize_simple() {
  std::string s = "history will absolve me";
  Corpus c = tokenize(s);
  check(c.size() == 4, "history will absolve me -> 4 tokens");
  std::vector<std::pair<std::size_t, std::string>> toks;
  for (const auto& t : c) toks.emplace_back(t.src_offset, t.content);
  std::ranges::sort(toks);
  check(toks.size() == 4
        && toks[0] == std::pair<std::size_t,std::string>{0,  "history"}
        && toks[1] == std::pair<std::size_t,std::string>{8,  "will"}
        && toks[2] == std::pair<std::size_t,std::string>{13, "absolve"}
        && toks[3] == std::pair<std::size_t,std::string>{21, "me"},
        "tokens have correct content and offsets");
}

inline void test_tokenize_empty() {
  std::string s = "";
  check(tokenize(s).empty(), "empty input -> 0 tokens");
}

inline void test_tokenize_multispace() {
  std::string s = "  hello   world  ";
  check(tokenize(s).size() == 2, "extra whitespace collapsed to 2 tokens");
}

inline void test_spellcheck_basic() {
  Dictionary dict = {"hello", "world", "test"};
  std::string s = "hello wrld test";
  Corpus c = tokenize(s);
  auto mis = spellcheck(c, dict);
  check(mis.size() == 1, "one misspelling in 'hello wrld test'");
  if (!mis.empty()) {
    check(mis.begin()->token.content == "wrld",
          "the misspelled token is 'wrld'");
    check(mis.begin()->suggestions.contains("world"),
          "suggestions include 'world'");
  }
}

inline void test_spellcheck_clean() {
  Dictionary dict = {"hello", "world"};
  std::string s = "hello world";
  Corpus c = tokenize(s);
  check(spellcheck(c, dict).empty(),
        "no misspellings in correctly-spelled input");
}

inline void test_spellcheck_drops_no_suggestion() {
  Dictionary dict = {"hello"};
  std::string s = "asdfqwerty";
  Corpus c = tokenize(s);
  check(spellcheck(c, dict).empty(),
        "misspellings with no Damerau-Levenshtein-1 suggestion are dropped");
}

using TestFn = void(*)();
inline const std::map<std::string, TestFn> test_functions = {
  {"tokenize_simple",                test_tokenize_simple},
  {"tokenize_empty",                 test_tokenize_empty},
  {"tokenize_multispace",            test_tokenize_multispace},
  {"spellcheck_basic",               test_spellcheck_basic},
  {"spellcheck_clean",               test_spellcheck_clean},
  {"spellcheck_drops_no_suggestion", test_spellcheck_drops_no_suggestion},
};

inline int run_one(const std::string& name) {
  auto it = test_functions.find(name);
  if (it == test_functions.end()) {
    std::cerr << "Test '" << name << "' not found.\nAvailable tests:\n";
    for (const auto& [n, _] : test_functions) std::cerr << "  " << n << "\n";
    return 1;
  }
  std::cout << "===== " << name << " =====\n";
  try { it->second(); }
  catch (const std::exception& e) {
    std::cout << "  ERROR: " << e.what() << "\n";
    ++failures;
  }
  return 0;
}

inline int run_all() {
  failures = 0;
  for (const auto& [name, _] : test_functions) run_one(name);
  std::cout << "\n"
            << (failures == 0 ? "All checks passed.\n"
                              : std::to_string(failures) + " check(s) failed.\n");
  return failures == 0 ? 0 : 1;
}

} // namespace tests
