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

  std::string result;
  if (name.empty())
    throw std::invalid_argument("Name cannot be empty");

  result.push_back(to_upper_case(name.at(0)));

  auto space_pos = name.find(' ');
  if (space_pos == std::string_view::npos)
    throw std::invalid_argument("Name must contain at least a first and last name");

  result.push_back(to_upper_case(name.at(space_pos + 1)));
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
  // throw std::runtime_error("Not implemented: find_matches");

  const std::string name_initials = initials(name);

  std::vector<std::string> matches;
  std::ranges::copy_if(
      students, std::back_inserter(matches),
      [&name_initials](const std::string &student_intials)
      {
        return student_intials == name_initials;
      },
      &initials);

  return matches;
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
  // throw std::runtime_error("Not implemented: get_match");

  if (matches.empty())
    return "NO MATCHES FOUND.";

  std::random_device rd;
  std::mt19937 gen(rd());

  std::string result; // just want to sample a single string here, no vector needed
  std::sample(matches.begin(), matches.end(), &result, 1, gen);

  return result;
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
  // throw std::runtime_error("Not implemented: run_mixer");
  std::vector<std::pair<std::string, std::string>> mixer_pairs;

  auto begin_it = applicants.begin();

  while (begin_it != applicants.end())
  {
    std::vector<std::string> matches = find_matches(*begin_it, applicants);

    if (matches.size() <= 1)
    { // this one is a lonely heart.
      begin_it = next(begin_it);
      continue;
    }

    matches.erase(matches.begin()); // remove the first match since it's the one we're trying to pair
    std::string match = get_match(matches);

    auto match_it = std::find(applicants.begin(), applicants.end(), match);

    if (match_it == applicants.end())
    {
      // this should never happen.
      begin_it = next(begin_it);
      continue;
    }

    mixer_pairs.emplace_back(*begin_it, *match_it);
    applicants.erase(match_it);
    begin_it = applicants.erase(begin_it);
  }

  return mixer_pairs;
}

/* #### Please don't remove this line! #### */
#include "tests/utils.hpp"
