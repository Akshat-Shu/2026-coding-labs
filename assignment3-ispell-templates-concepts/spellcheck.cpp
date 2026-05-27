#include "spellcheck.h"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <ranges>
#include <set>
#include <vector>

template <typename Iterator, typename UnaryPred>
requires std::input_iterator<Iterator> && std::indirect_unary_predicate<UnaryPred, Iterator>
std::vector<Iterator> find_all(Iterator begin, Iterator end, UnaryPred pred);

Corpus tokenize(std::string &source)
{
  /* TODO: Implement this method */

  auto iterators_to_tokens = [&source](auto it1, auto it2)
  {
    return Token(source, it1, it2);
  };

  auto is_token_empty = [](const Token &t)
  { return t.content.empty(); };

  auto boundary_iterators = find_all(source.begin(), source.end(), ::isspace);

  Corpus corpus;
  std::transform(boundary_iterators.begin(),
                 std::prev(boundary_iterators.end()),
                 std::next(boundary_iterators.begin()),
                 std::inserter(corpus, corpus.end()),
                 iterators_to_tokens);

  std::erase_if(corpus, is_token_empty);

  return corpus;
}

std::set<Misspelling> spellcheck(const Corpus &source, const Dictionary &dictionary)
{
  auto is_misspelled = [&dictionary](const Token &t)
  { return !dictionary.contains(t.content); };

  auto is_misspelling_non_empty = [](const Misspelling &m)
  { return !m.suggestions.empty(); };

  auto token_to_misspelling = [&dictionary](const Token &t) {
    auto filtered_suggestions = dictionary 
        | std::views::filter(
          [&t](const std::string& word) { 
            return levenshtein(word, t.content) == 1; });
    
    return Misspelling {
      .token = t,
      .suggestions = std::set<std::string>(
              filtered_suggestions.begin(), 
              filtered_suggestions.end())
    };
  };

  /* TODO: Implement this method */
  auto view = source 
      | std::views::filter(is_misspelled)
      | std::views::transform(token_to_misspelling);
  
  std::set<Misspelling> misspellings;
  auto inserter = std::inserter(misspellings, misspellings.end());
  std::copy_if(view.begin(), view.end(), inserter, 
                is_misspelling_non_empty);

  return misspellings;
};

/* Helper methods */

#include "utils.cpp"