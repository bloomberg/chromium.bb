// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_DATA_H_
#define CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_DATA_H_

#include <string>

// This struct contains all the data needed to inject search suggestions into a
// page.
struct SearchSuggestData {
  SearchSuggestData();
  SearchSuggestData(const SearchSuggestData&);
  SearchSuggestData(SearchSuggestData&&);
  ~SearchSuggestData();

  SearchSuggestData& operator=(const SearchSuggestData&);
  SearchSuggestData& operator=(SearchSuggestData&&);

  // The HTML for the search suggestions.
  std::string suggestions_html;

  // Javascript for search suggestion that should be appended at the end of the
  // New Tab Page <body>.
  std::string end_of_body_script;

  // Parameters that control impression capping and freezing.
  int impression_cap_expire_time_ms;
  int request_freeze_time_ms;
  int max_impressions;
};

bool operator==(const SearchSuggestData& lhs, const SearchSuggestData& rhs);
bool operator!=(const SearchSuggestData& lhs, const SearchSuggestData& rhs);

#endif  // CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_DATA_H_
