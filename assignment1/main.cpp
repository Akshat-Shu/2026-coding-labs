/*
 * Assignment 1: Marriage Pact
 * Adapted by Tinkercademy from Stanford CS106L
 * (originally by Haven Whitney, with modifications by Fabio Ibanez
 * & Jacob Roberts-Baca).
 *
 * Complete each STUDENT TODO below. Read the README carefully — the
 * requirements there (ranges, projections, sample, reserve, no raw
 * for-loops in find_matches, iterator-safe erase in run_mixer) are
 * part of the assignment, not optional polish.
 */

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * Reads `filename` line by line and returns the applicants.
 *
 * Requirements:
 *   - Take `filename` as `const std::string&`.
 *   - Call `reserve()` before populating, with a sensible capacity.
 *     Justify your choice in short_answer.txt.
 */
std::vector<std::string> get_applicants(const std::string &filename)
{
  // STUDENT TODO: Implement this function.
  // throw std::runtime_error("Not implemented: get_applicants");

  const int EXPECTED_APPLICANTS = 1000;
  std::vector<std::string> applicants;
  applicants.reserve(EXPECTED_APPLICANTS);

  std::ifstream infile(filename);
  std::string line;

  while (std::getline(infile, line))
    applicants.push_back(line);

  return applicants;
}

char to_upper_case(char c)
{
  if (c >= 'a' && c <= 'z')
  {
    return c - ('a' - 'A');
  }
  return c;
}

/**
 * Returns the initials of `name`, uppercased.
 *   e.g. initials("Marceline McMillan") == "MM"
 *
 * Requirements:
 *   - Parameter must be `std::string_view` (no allocation).
 */
std::string initials(std::string_view name)
{
  // STUDENT TODO: Implement this function.
  // throw std::runtime_error("Not implemented: initials");

  // elper. Given a name like "Marceline McMillan", return its initials ("MM"), uppercased. This function should take a std::string_view, not a std::string, to avoid unnecessary allocations.
  std::string result;
  if (name.empty())
    throw std::invalid_argument("Name cannot be empty");

  result.push_back(to_upper_case(name[0]));

  auto space_pos = name.find(' ');
  if (space_pos == std::string_view::npos)
    throw std::invalid_argument("Name must contain at least a first and last name");

  result.push_back(to_upper_case(name[space_pos + 1]));
  return result;
}

/**
 * Returns every applicant in `students` who shares initials with `name`.
 *
 * Requirements:
 *   - No raw `for` loops. Use std::ranges::copy_if (or views::filter
 *     piped into a vector). Use a projection where it makes the call
 *     clearer.
 *   - Take `students` as `const std::vector<std::string>&`.
 */
std::vector<std::string> find_matches(std::string_view name,
                                      const std::vector<std::string> &students)
{
  // STUDENT TODO: Implement this function.
  throw std::runtime_error("Not implemented: find_matches");
}

/**
 * Returns one randomly-chosen match, or "NO MATCHES FOUND." if empty.
 *
 * Requirements:
 *   - Use std::sample with a seeded std::mt19937.
 *   - Do NOT use pop_back() or rand() % size.
 */
std::string get_match(const std::vector<std::string> &matches)
{
  // STUDENT TODO: Implement this function.
  throw std::runtime_error("Not implemented: get_match");
}

/**
 * Runs a multi-round mixer. In each round, scan the remaining
 * applicants left-to-right; for each applicant, look for another
 * applicant with the same initials still in the pool. If found,
 * pair them, remove both from `applicants`, and record the pair.
 * Continue rounds until a full pass yields no new pairs.
 *
 * `applicants` is mutated: paired names are removed. Whatever is
 * left over at the end is unpaired.
 *
 * Requirements:
 *   - The naive "iterate and erase as you go" approach WILL invalidate
 *     your iterator. You must handle this — see the README for the
 *     three acceptable strategies — and document your choice in
 *     short_answer.txt.
 */
std::vector<std::pair<std::string, std::string>>
run_mixer(std::vector<std::string> &applicants)
{
  // STUDENT TODO: Implement this function.
  throw std::runtime_error("Not implemented: run_mixer");
}

/* #### Please don't remove this line! #### */
#include "tests/utils.hpp"
