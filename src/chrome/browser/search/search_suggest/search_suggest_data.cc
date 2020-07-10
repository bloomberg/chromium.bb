// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search_suggest/search_suggest_data.h"

SearchSuggestData::SearchSuggestData()
    : impression_cap_expire_time_ms(0),
      request_freeze_time_ms(0),
      max_impressions(0) {}
SearchSuggestData::SearchSuggestData(const SearchSuggestData&) = default;
SearchSuggestData::SearchSuggestData(SearchSuggestData&&) = default;
SearchSuggestData::~SearchSuggestData() = default;

SearchSuggestData& SearchSuggestData::operator=(const SearchSuggestData&) =
    default;
SearchSuggestData& SearchSuggestData::operator=(SearchSuggestData&&) = default;

bool operator==(const SearchSuggestData& lhs, const SearchSuggestData& rhs) {
  return lhs.suggestions_html == rhs.suggestions_html &&
         lhs.end_of_body_script == rhs.end_of_body_script &&
         lhs.impression_cap_expire_time_ms ==
             rhs.impression_cap_expire_time_ms &&
         lhs.request_freeze_time_ms == rhs.request_freeze_time_ms &&
         lhs.max_impressions == rhs.max_impressions;
}

bool operator!=(const SearchSuggestData& lhs, const SearchSuggestData& rhs) {
  return !(lhs == rhs);
}
