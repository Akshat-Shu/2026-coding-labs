/* ========================================================================= *
 * Test harness for Assignment 1: Marriage Pact.                             *
 *                                                                           *
 * This file provides main(). It runs the user-facing CLI modes              *
 * (./main "<name>", ./main --mixer) and also a test mode                    *
 * (./main --test [name]) that checks invariants on the student's            *
 * functions.                                                                *
 *                                                                           *
 * You don't need to read past this point unless you're curious!             *
 * ========================================================================= */

#include <algorithm>
#include <exception>
#include <map>
#include <ranges>
#include <set>
#include <string>
#include <unordered_map>

namespace tests {

inline int failures = 0;

inline void check(bool cond, const std::string& msg) {
  if (cond) {
    std::cout << "  PASS: " << msg << "\n";
  } else {
    std::cout << "  FAIL: " << msg << "\n";
    ++failures;
  }
}

inline void test_initials() {
  check(initials("Marceline McMillan") == "MM", "Marceline McMillan -> MM");
  check(initials("alice bobson") == "AB",       "alice bobson -> AB (uppercased)");
  check(initials("Bob Jones") == "BJ",          "Bob Jones -> BJ");
}

inline void test_get_applicants() {
  auto v = get_applicants("students.txt");
  check(v.size() == 1000, "students.txt yields 1000 entries");
  check(!v.empty() && v.front() == "Marceline McMillan",
        "first applicant is 'Marceline McMillan'");
  check(std::ranges::none_of(v, [](const auto& s) { return s.empty(); }),
        "no empty names in the result");
}

inline void test_find_matches() {
  auto applicants = get_applicants("students.txt");
  if (applicants.empty()) {
    check(false, "find_matches: applicants list was empty, cannot test");
    return;
  }
  const std::string& probe = applicants.front();
  auto matches = find_matches(probe, applicants);
  check(std::ranges::find(matches, probe) != matches.end(),
        "find_matches(<first applicant>) contains the first applicant");
  check(std::ranges::all_of(matches,
          [&](const auto& s) { return initials(s) == initials(probe); }),
        "every match shares initials with the input");

  auto none = find_matches("Zz Zz", applicants);
  check(std::ranges::all_of(none,
          [](const auto& s) { return initials(s) == "ZZ"; }),
        "find_matches('Zz Zz') returns only ZZ-initialled names (possibly empty)");
}

inline void test_get_match() {
  std::vector<std::string> empty;
  check(get_match(empty) == "NO MATCHES FOUND.",
        "empty input returns 'NO MATCHES FOUND.'");

  std::vector<std::string> some = {"Alice Adams", "Andy Anderson", "Anna Apple"};
  auto m = get_match(some);
  check(std::ranges::find(some, m) != some.end(),
        "returned match is one of the inputs");
}

inline void test_run_mixer() {
  auto applicants = get_applicants("students.txt");
  const std::size_t total_before = applicants.size();
  auto pairs = run_mixer(applicants);

  check(std::ranges::all_of(pairs,
          [](const auto& p) { return initials(p.first) == initials(p.second); }),
        "every pair shares initials");

  std::set<std::string> seen;
  bool no_dupes = true;
  for (const auto& [a, b] : pairs) {
    if (!seen.insert(a).second) no_dupes = false;
    if (!seen.insert(b).second) no_dupes = false;
  }
  check(no_dupes, "no name appears twice across the pairs");

  check(applicants.size() + 2 * pairs.size() == total_before,
        "applicants vector shrunk by exactly 2 * pairs.size()");

  std::unordered_map<std::string, int> counts;
  for (const auto& s : applicants) counts[initials(s)]++;
  check(std::ranges::all_of(counts,
          [](const auto& kv) { return kv.second <= 1; }),
        "leftover pool has no two people sharing initials (mixer is maximal)");
}

using TestFn = void(*)();
inline const std::map<std::string, TestFn> test_functions = {
  {"initials",       test_initials},
  {"get_applicants", test_get_applicants},
  {"find_matches",   test_find_matches},
  {"get_match",      test_get_match},
  {"run_mixer",      test_run_mixer},
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

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Usage:\n"
              << "  " << argv[0] << " \"<your name>\"      # find your match\n"
              << "  " << argv[0] << " --mixer              # pair everyone up\n"
              << "  " << argv[0] << " --test               # run all tests\n"
              << "  " << argv[0] << " --test <name>        # run one named test\n";
    return 1;
  }

  std::string arg = argv[1];

  if (arg == "--test") {
    if (argc >= 3) {
      tests::failures = 0;
      tests::run_one(argv[2]);
      return tests::failures == 0 ? 0 : 1;
    }
    return tests::run_all();
  }

  std::vector<std::string> applicants = get_applicants("students.txt");

  if (arg == "--mixer") {
    auto pairs = run_mixer(applicants);
    std::cout << "Paired " << pairs.size() << " couple(s):\n";
    for (const auto& [a, b] : pairs) {
      std::cout << "  " << a << "  <3  " << b << "\n";
    }
    std::cout << applicants.size() << " applicant(s) left unpaired.\n";
    return 0;
  }

  if (arg.empty()) {
    std::cout << "Please provide a non-empty name.\n";
    return 1;
  }

  auto matches = find_matches(arg, applicants);
  std::cout << "Your match is: " << get_match(matches) << std::endl;
  return 0;
}
