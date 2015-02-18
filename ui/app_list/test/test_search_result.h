// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_TEST_TEST_SEARCH_RESULT_H_
#define UI_APP_LIST_TEST_TEST_SEARCH_RESULT_H_

#include "ui/app_list/search_result.h"

namespace app_list {

// A test search result which does nothing.
class TestSearchResult : public SearchResult {
 public:
  TestSearchResult();
  ~TestSearchResult() override;

  // SearchResult:
  scoped_ptr<SearchResult> Duplicate() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSearchResult);
};

}  // namespace app_list

#endif  // UI_APP_LIST_TEST_TEST_SEARCH_RESULT_H_
