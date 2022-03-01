// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_BOOKMARK_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_BOOKMARK_PROVIDER_H_

#include <stddef.h>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;

namespace bookmarks {
class BookmarkModel;
struct TitledUrlMatch;
}

// This class is an autocomplete provider which quickly (and synchronously)
// provides autocomplete suggestions based on the titles of bookmarks. Page
// titles, URLs, and typed and visit counts of the bookmarks are not currently
// taken into consideration as those factors will have been used by the
// HistoryQuickProvider (HQP) while identifying suggestions.
//
// TODO(mrossetti): Propose a way to coordinate with the HQP the taking of typed
// and visit counts and last visit dates, etc. into consideration while scoring.
class BookmarkProvider : public AutocompleteProvider {
 public:
  explicit BookmarkProvider(AutocompleteProviderClient* client);

  BookmarkProvider(const BookmarkProvider&) = delete;
  BookmarkProvider& operator=(const BookmarkProvider&) = delete;

  // When |minimal_changes| is true short circuit any additional searching and
  // leave the previous matches for this provider unchanged, otherwise perform
  // a complete search for |input| across all bookmark titles.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  // Sets the BookmarkModel for unit tests.
  void set_bookmark_model_for_testing(
      bookmarks::BookmarkModel* bookmark_model) {
    bookmark_model_ = bookmark_model;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(BookmarkProviderTest, InlineAutocompletion);

  ~BookmarkProvider() override;

  // Performs the actual matching of |input| over the bookmarks and fills in
  // |matches_|.
  void DoAutocomplete(const AutocompleteInput& input);

  // Get the matches from |bookmark_model_| using the appropriate matching
  // algorithm, determined by |GetMatchingAlgorithm()|, and path matching
  // algorithm, determined by the |kBookmarkPaths| base::feature.
  std::vector<bookmarks::TitledUrlMatch> GetMatchesWithBookmarkPaths(
      const AutocompleteInput& input,
      size_t kMaxBookmarkMatches);

  // There are 2 short bookmark features that determine the matching algorithm
  // used, i.e. whether input words shorter than 3 chars can prefix match.
  // 1) |IsShortBookmarkSuggestionsEnabled()| always allows short input word
  //    prefix matching.
  // 2) |IsShortBookmarkSuggestionsByTotalInputLengthEnabled()| allows short
  //    input word prefix matching only if the input is longer than a threshold
  //    param. This feature also has a counterfactual param.
  // 3) Otherwise, if both features are disabled (or if the counterfactual param
  //    is true), short input word matching won't be allowed.
  query_parser::MatchingAlgorithm GetMatchingAlgorithm(AutocompleteInput input);

  // Calculates the relevance score for |match|.
  int CalculateBookmarkMatchRelevance(
      const bookmarks::TitledUrlMatch& match) const;

  // Removes any URL matches for query parameter keys (if the matching word
  // starts immediately after a '?' or '&').
  void RemoveQueryParamKeyMatches(bookmarks::TitledUrlMatch& match);

  raw_ptr<AutocompleteProviderClient> client_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_BOOKMARK_PROVIDER_H_
