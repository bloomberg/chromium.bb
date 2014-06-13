// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search_provider.h"

#include "ui/app_list/search_result.h"

namespace app_list {

SearchProvider::SearchProvider() {
}
SearchProvider::~SearchProvider() {
}

void SearchProvider::ReleaseResult(std::vector<SearchResult*>* results) {
  results_.release(results);
}

void SearchProvider::Add(scoped_ptr<SearchResult> result) {
  results_.push_back(result.release());
  FireResultChanged();
}

void SearchProvider::ClearResults() {
  results_.clear();
  FireResultChanged();
}

void SearchProvider::FireResultChanged() {
  if (result_changed_callback_.is_null())
    return;

  result_changed_callback_.Run();
}

}  // namespace app_list
