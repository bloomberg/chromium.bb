// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SEARCH_PROVIDER_H_
#define UI_APP_LIST_SEARCH_PROVIDER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "ui/app_list/app_list_export.h"

namespace app_list {

class SearchResult;

class APP_LIST_EXPORT SearchProvider {
 public:
  typedef ScopedVector<SearchResult> Results;
  typedef base::Closure ResultChangedCallback;

  SearchProvider();
  virtual ~SearchProvider();

  // Invoked to start a query.
  virtual void Start(const base::string16& query) = 0;

  // Invoked to stop the current query and no more results changes.
  virtual void Stop() = 0;

  void set_result_changed_callback(const ResultChangedCallback& callback) {
    result_changed_callback_ = callback;
  }

  // TODO(mukai): Fix the ownership and copying of the results.
  void ReleaseResult(std::vector<SearchResult*>* results);

  const Results& results() const { return results_; }

 protected:
  // Interface for the derived class to generate search results.
  void Add(scoped_ptr<SearchResult> result);
  void ClearResults();

 private:
  void FireResultChanged();

  ResultChangedCallback result_changed_callback_;
  Results results_;

  DISALLOW_COPY_AND_ASSIGN(SearchProvider);
};

}  // namespace app_list

#endif  // UI_APP_LIST_SEARCH_PROVIDER_H_
