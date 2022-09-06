// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/test/search_results_changed_waiter.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

namespace app_list {

SearchResultsChangedWaiter::SearchResultsChangedWaiter(
    SearchController* search_controller,
    const std::set<ash::AppListSearchResultType>& types)
    : search_controller_(search_controller), types_(types) {
  search_controller_->set_results_changed_callback_for_test(base::BindRepeating(
      &SearchResultsChangedWaiter::OnResultsChanged, base::Unretained(this)));
}

SearchResultsChangedWaiter::~SearchResultsChangedWaiter() {
  if (active_) {
    search_controller_->set_results_changed_callback_for_test(
        base::NullCallback());
  }
}

void SearchResultsChangedWaiter::Wait() {
  if (!active_)
    return;
  run_loop_.Run();
}

void SearchResultsChangedWaiter::OnResultsChanged(
    ash::AppListSearchResultType result_type) {
  types_.erase(result_type);
  if (!types_.empty())
    return;

  active_ = false;
  search_controller_->set_results_changed_callback_for_test(
      base::NullCallback());
  run_loop_.Quit();
}

}  // namespace app_list
