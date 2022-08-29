// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/titled_url_index.h"

#include <stdint.h>

#include <algorithm>
#include <unordered_set>
#include <utility>

#include "base/containers/cxx20_erase.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/unicodestring.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/titled_url_node.h"
#include "components/query_parser/snippet.h"
#include "third_party/icu/source/common/unicode/normalizer2.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace bookmarks {

namespace {

// The maximum number of nodes to fetch when looking for nodes that match any
// term (i.e. when invoking `RetrieveNodesMatchingAnyTerms()`). Parameterized
// for testing.
constexpr size_t kMatchingAnyTermsMaxNodes = 3000;

// Returns a normalized version of the UTF16 string |text|.  If it fails to
// normalize the string, returns |text| itself as a best-effort.
std::u16string Normalize(const std::u16string& text) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::Normalizer2* normalizer2 =
      icu::Normalizer2::getInstance(nullptr, "nfkc", UNORM2_COMPOSE, status);
  if (U_FAILURE(status)) {
    // Log and crash right away to capture the error code in the crash report.
    LOG(FATAL) << "failed to create a normalizer: " << u_errorName(status);
  }
  icu::UnicodeString unicode_text(
      text.data(), static_cast<int32_t>(text.length()));
  icu::UnicodeString unicode_normalized_text;
  normalizer2->normalize(unicode_text, unicode_normalized_text, status);
  if (U_FAILURE(status)) {
    // This should not happen. Log the error and fall back.
    LOG(ERROR) << "normalization failed: " << u_errorName(status);
    return text;
  }
  return base::i18n::UnicodeStringToString16(unicode_normalized_text);
}

}  // namespace

TitledUrlIndex::TitledUrlIndex(std::unique_ptr<TitledUrlNodeSorter> sorter)
    : sorter_(std::move(sorter)) {
}

TitledUrlIndex::~TitledUrlIndex() {
}

void TitledUrlIndex::SetNodeSorter(
    std::unique_ptr<TitledUrlNodeSorter> sorter) {
  sorter_ = std::move(sorter);
}

void TitledUrlIndex::Add(const TitledUrlNode* node) {
  for (const std::u16string& term : ExtractIndexTerms(node))
    RegisterNode(term, node);
}

void TitledUrlIndex::Remove(const TitledUrlNode* node) {
  for (const std::u16string& term : ExtractIndexTerms(node))
    UnregisterNode(term, node);
}

std::vector<TitledUrlMatch> TitledUrlIndex::GetResultsMatching(
    const std::u16string& input_query,
    size_t max_count,
    query_parser::MatchingAlgorithm matching_algorithm,
    bool match_ancestor_titles) {
  SCOPED_UMA_HISTOGRAM_TIMER("Bookmarks.GetResultsMatching.Timing.Total");
  const std::u16string query = Normalize(input_query);
  std::vector<std::u16string> terms = ExtractQueryWords(query);
  base::UmaHistogramExactLinear("Bookmarks.GetResultsMatching.Terms.TermsCount",
                                terms.size(), 16);
  if (terms.empty())
    return {};

  for (const std::u16string& term : terms) {
    base::UmaHistogramExactLinear(
        "Bookmarks.GetResultsMatching.Terms.TermLength", term.size(), 16);
  }
  // When |match_ancestor_titles| is true, |matches| shouldn't exclude nodes
  // that don't match every query term, as the query terms may match in the
  // ancestors. |MatchTitledUrlNodeWithQuery()| below will filter out nodes that
  // neither match nor ancestor-match every query term.
  TitledUrlNodeSet matches;
  {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "Bookmarks.GetResultsMatching.Timing.RetrievingNodes");
    matches = match_ancestor_titles
                  ? RetrieveNodesMatchingAnyTerms(terms, matching_algorithm,
                                                  kMatchingAnyTermsMaxNodes)
                  : RetrieveNodesMatchingAllTerms(terms, matching_algorithm);
  }
  base::UmaHistogramBoolean("Bookmarks.GetResultsMatching.AnyTermApproach.Used",
                            match_ancestor_titles);
  base::UmaHistogramCounts10000("Bookmarks.GetResultsMatching.Nodes.Count",
                                matches.size());
  // Slice by 3, the default value of
  // `kShortBookmarkSuggestionsByTotalInputLengthThreshold`. Shorter inputs will
  // likely have many fewer nodes than longer queries.
  if (input_query.size() < 3) {
    base::UmaHistogramCounts10000(
        "Bookmarks.GetResultsMatching.Nodes.Count."
        "InputsShorterThan3CharsLong",
        matches.size());
  } else {
    base::UmaHistogramCounts10000(
        "Bookmarks.GetResultsMatching.Nodes.Count.InputsAtLeast3CharsLong",
        matches.size());
  }

  if (matches.empty())
    return {};

  TitledUrlNodes sorted_nodes;
  SortMatches(matches, &sorted_nodes);

  // We use a QueryParser to fill in match positions for us. It's not the most
  // efficient way to go about this, but by the time we get here we know what
  // matches and so this shouldn't be performance critical.
  query_parser::QueryNodeVector query_nodes;
  query_parser::QueryParser::ParseQueryNodes(query, matching_algorithm,
                                             &query_nodes);

  std::vector<TitledUrlMatch> results = MatchTitledUrlNodesWithQuery(
      sorted_nodes, query_nodes, max_count, match_ancestor_titles);

  // In practice, `max_count`, is always 50 (`kMaxBookmarkMatches`), so
  // `results.size()` is at most 50.
  base::UmaHistogramExactLinear(
      "Bookmarks.GetResultsMatching.Matches.ReturnedCount", results.size(), 51);
  return results;
}

void TitledUrlIndex::SortMatches(const TitledUrlNodeSet& matches,
                                 TitledUrlNodes* sorted_nodes) const {
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Bookmarks.GetResultsMatching.Timing.SortingNodes");
  if (sorter_) {
    sorter_->SortMatches(matches, sorted_nodes);
  } else {
    sorted_nodes->insert(sorted_nodes->end(), matches.begin(), matches.end());
  }
}

std::vector<TitledUrlMatch> TitledUrlIndex::MatchTitledUrlNodesWithQuery(
    const TitledUrlNodes& nodes,
    const query_parser::QueryNodeVector& query_nodes,
    size_t max_count,
    bool match_ancestor_titles) {
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Bookmarks.GetResultsMatching.Timing.CreatingMatches");
  // The highest typed counts should be at the beginning of the `matches` vector
  // so that the best matches will always be included in the results. The loop
  // that calculates match relevance in
  // `HistoryContentsProvider::ConvertResults()` will run backwards to assure
  // higher relevance will be attributed to the best matches.
  std::vector<TitledUrlMatch> matches;
  int nodes_considered = 0;
  for (TitledUrlNodes::const_iterator i = nodes.begin();
       i != nodes.end() && matches.size() < max_count; ++i) {
    absl::optional<TitledUrlMatch> match =
        MatchTitledUrlNodeWithQuery(*i, query_nodes, match_ancestor_titles);
    if (match)
      matches.emplace_back(std::move(match).value());
    ++nodes_considered;
  }
  base::UmaHistogramCounts10000(
      "Bookmarks.GetResultsMatching.Matches.ConsideredCount", nodes_considered);
  return matches;
}

absl::optional<TitledUrlMatch> TitledUrlIndex::MatchTitledUrlNodeWithQuery(
    const TitledUrlNode* node,
    const query_parser::QueryNodeVector& query_nodes,
    bool match_ancestor_titles) {
  if (!node) {
    return absl::nullopt;
  }
  // Check that the result matches the query.  The previous search
  // was a simple per-word search, while the more complex matching
  // of QueryParser may filter it out.  For example, the query
  // ["thi"] will match the title [Thinking], but since
  // ["thi"] is quoted we don't want to do a prefix match.
  query_parser::QueryWordVector title_words, url_words, ancestor_words;
  const std::u16string lower_title =
      base::i18n::ToLower(Normalize(node->GetTitledUrlNodeTitle()));
  query_parser::QueryParser::ExtractQueryWords(lower_title, &title_words);
  base::OffsetAdjuster::Adjustments adjustments;
  query_parser::QueryParser::ExtractQueryWords(
      CleanUpUrlForMatching(node->GetTitledUrlNodeUrl(), &adjustments),
      &url_words);
  if (match_ancestor_titles) {
    for (auto ancestor : node->GetTitledUrlNodeAncestorTitles()) {
      query_parser::QueryParser::ExtractQueryWords(
          base::i18n::ToLower(Normalize(std::u16string(ancestor))),
          &ancestor_words);
    }
  }

  query_parser::Snippet::MatchPositions title_matches, url_matches;
  bool query_has_ancestor_matches = false;
  for (const auto& query_node : query_nodes) {
    const bool has_title_matches =
        query_node->HasMatchIn(title_words, &title_matches);
    const bool has_url_matches =
        query_node->HasMatchIn(url_words, &url_matches);
    const bool has_ancestor_matches =
        match_ancestor_titles && query_node->HasMatchIn(ancestor_words, false);
    query_has_ancestor_matches =
        query_has_ancestor_matches || has_ancestor_matches;
    if (!has_title_matches && !has_url_matches && !has_ancestor_matches)
      return absl::nullopt;
    query_parser::QueryParser::SortAndCoalesceMatchPositions(&title_matches);
    query_parser::QueryParser::SortAndCoalesceMatchPositions(&url_matches);
  }

  TitledUrlMatch match;
  if (lower_title.length() == node->GetTitledUrlNodeTitle().length()) {
    // Only use title matches if the lowercase string is the same length
    // as the original string, otherwise the matches are meaningless.
    // TODO(mpearson): revise match positions appropriately.
    match.title_match_positions.swap(title_matches);
  }
  // Now that we're done processing this entry, correct the offsets of the
  // matches in |url_matches| so they point to offsets in the original URL
  // spec, not the cleaned-up URL string that we used for matching.
  std::vector<size_t> offsets =
      TitledUrlMatch::OffsetsFromMatchPositions(url_matches);
  base::OffsetAdjuster::UnadjustOffsets(adjustments, &offsets);
  url_matches =
      TitledUrlMatch::ReplaceOffsetsInMatchPositions(url_matches, offsets);
  match.url_match_positions.swap(url_matches);
  match.has_ancestor_match = query_has_ancestor_matches;
  match.node = node;
  return match;
}

TitledUrlIndex::TitledUrlNodeSet TitledUrlIndex::RetrieveNodesMatchingAllTerms(
    const std::vector<std::u16string>& terms,
    query_parser::MatchingAlgorithm matching_algorithm) const {
  DCHECK(!terms.empty());
  TitledUrlNodeSet matches =
      RetrieveNodesMatchingTerm(terms[0], matching_algorithm);
  for (size_t i = 1; i < terms.size() && !matches.empty(); ++i) {
    TitledUrlNodeSet term_matches =
        RetrieveNodesMatchingTerm(terms[i], matching_algorithm);
    // Compute intersection between the two sets.
    base::EraseIf(matches, base::IsNotIn<TitledUrlNodeSet>(term_matches));
  }

  return matches;
}

TitledUrlIndex::TitledUrlNodeSet TitledUrlIndex::RetrieveNodesMatchingAnyTerms(
    const std::vector<std::u16string>& terms,
    query_parser::MatchingAlgorithm matching_algorithm,
    size_t max_nodes) const {
  DCHECK(!terms.empty());

  std::vector<TitledUrlNodes> matches_per_term;
  bool some_term_had_empty_matches = false;
  for (const std::u16string& term : terms) {
    // Use `matching_algorithm`, as opposed to exact matching, to allow inputs
    // like 'myFolder goog' to match a 'google.com' bookmark in a 'myFolder'
    // folder.
    TitledUrlNodes term_matches =
        RetrieveNodesMatchingTerm(term, matching_algorithm);
    base::UmaHistogramCounts1000(
        "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCountPerTerm",
        term_matches.size());
    if (term_matches.empty())
      some_term_had_empty_matches = true;
    else
      matches_per_term.push_back(std::move(term_matches));
  }
  base::UmaHistogramExactLinear(
      "Bookmarks.GetResultsMatching.AnyTermApproach.TermsUnionedCount",
      matches_per_term.size(), 16);

  // Sort `matches_per_term` least frequent first. This prevents terms like
  // 'https', which match a lot of nodes, from wasting `max_nodes` capacity.
  base::ranges::sort(
      matches_per_term,
      [](size_t first, size_t second) { return first < second; },
      [](const auto& matches) { return matches.size(); });

  // Use an `unordered_set` to avoid potentially 1000's of linear time
  // insertions into the ordered `TitledUrlNodeSet` (i.e. `flat_set`).
  std::unordered_set<const TitledUrlNode*> matches;
  for (const auto& term_matches : matches_per_term) {
    for (const TitledUrlNode* node : term_matches) {
      matches.insert(node);
      if (matches.size() == max_nodes)
        break;
    }
    if (matches.size() == max_nodes)
      break;
  }
  base::UmaHistogramCounts10000(
      "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCountAnyTerms",
      matches.size());

  // Append all nodes that match every input term. This is necessary because
  // with the `max_nodes` threshold above, it's possible that `matches` is
  // missing some nodes that match every input term. Since `matches_per_term[i]`
  // is a superset of the intersection of `matches_per_term`s, if
  // `matches_per_term[0].size() <= max_nodes`, all of `matches_per_term[0]`,
  // and therefore the intersection matches`, are already in `matches`.
  TitledUrlNodeSet all_term_matches;
  if (!some_term_had_empty_matches && matches_per_term[0].size() > max_nodes) {
    all_term_matches = matches_per_term[0];
    for (size_t i = 1; i < matches_per_term.size() && !all_term_matches.empty();
         ++i) {
      // Compute intersection between the two sets.
      base::EraseIf(all_term_matches,
                    base::IsNotIn<TitledUrlNodeSet>(matches_per_term[i]));
    }
    // `all_term_matches` is the intersection of each term's node matches; the
    // same as `RetrieveNodesMatchingAllTerms()`. We don't call the latter as a
    // performance optimization.
    DCHECK(all_term_matches ==
           RetrieveNodesMatchingAllTerms(terms, matching_algorithm));
  }
  base::UmaHistogramCounts1000(
      "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCountAllTerms",
      all_term_matches.size());

  matches.insert(all_term_matches.begin(), all_term_matches.end());
  base::UmaHistogramCounts10000(
      "Bookmarks.GetResultsMatching.AnyTermApproach.NodeCount", matches.size());
  return TitledUrlNodeSet(matches.begin(), matches.end());
}

TitledUrlIndex::TitledUrlNodes TitledUrlIndex::RetrieveNodesMatchingTerm(
    const std::u16string& term,
    query_parser::MatchingAlgorithm matching_algorithm) const {
  Index::const_iterator i = index_.lower_bound(term);
  if (i == index_.end())
    return {};

  if (!query_parser::QueryParser::IsWordLongEnoughForPrefixSearch(
      term, matching_algorithm)) {
    // Term is too short for prefix match, compare using exact match.
    if (i->first != term)
      return {};  // No title/URL pairs with this term.
    return TitledUrlNodes(i->second.begin(), i->second.end());
  }

  // Loop through index adding all entries that start with term to
  // |prefix_matches|.
  TitledUrlNodes prefix_matches;
  while (i != index_.end() && i->first.size() >= term.size() &&
         term.compare(0, term.size(), i->first, 0, term.size()) == 0) {
    prefix_matches.insert(prefix_matches.end(), i->second.begin(),
                          i->second.end());
    ++i;
  }
  return prefix_matches;
}

// static
std::vector<std::u16string> TitledUrlIndex::ExtractQueryWords(
    const std::u16string& query) {
  std::vector<std::u16string> terms;
  if (query.empty())
    return std::vector<std::u16string>();
  query_parser::QueryParser::ParseQueryWords(
      base::i18n::ToLower(query), query_parser::MatchingAlgorithm::DEFAULT,
      &terms);
  return terms;
}

// static
std::vector<std::u16string> TitledUrlIndex::ExtractIndexTerms(
    const TitledUrlNode* node) {
  std::vector<std::u16string> terms;

  for (const std::u16string& term :
       ExtractQueryWords(Normalize(node->GetTitledUrlNodeTitle()))) {
    terms.push_back(term);
  }

  for (const std::u16string& term : ExtractQueryWords(CleanUpUrlForMatching(
           node->GetTitledUrlNodeUrl(), /*adjustments=*/nullptr))) {
    terms.push_back(term);
  }

  return terms;
}

void TitledUrlIndex::RegisterNode(const std::u16string& term,
                                  const TitledUrlNode* node) {
  index_[term].insert(node);
}

void TitledUrlIndex::UnregisterNode(const std::u16string& term,
                                    const TitledUrlNode* node) {
  auto i = index_.find(term);
  if (i == index_.end()) {
    // We can get here if the node has the same term more than once. For
    // example, a node with the title 'foo foo' would end up here.
    return;
  }
  i->second.erase(node);
  if (i->second.empty())
    index_.erase(i);
}

}  // namespace bookmarks
