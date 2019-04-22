// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/model/search/search_result.h"

namespace app_list {

SearchResultBaseView::SearchResultBaseView() : Button(this) {
  SetInstallFocusRingOnFocus(false);
}

SearchResultBaseView::~SearchResultBaseView() = default;

bool SearchResultBaseView::SkipDefaultKeyEventProcessing(
    const ui::KeyEvent& event) {
  // Ensure accelerators take priority in the app list. This ensures, e.g., that
  // Ctrl+Space will switch input methods rather than activate the button.
  return false;
}

void SearchResultBaseView::SetBackgroundHighlighted(bool enabled) {
  background_highlighted_ = enabled;
  SchedulePaint();
}

void SearchResultBaseView::SetResult(SearchResult* result) {
  OnResultChanging(result);
  ClearResult();
  result_ = result;
  if (result_)
    result_->AddObserver(this);
  OnResultChanged();
}

void SearchResultBaseView::OnResultDestroying() {
  // Uses |SetResult| to ensure that the |OnResultChanging()| and
  // |OnResultChanged()| logic gets run.
  SetResult(nullptr);
}

void SearchResultBaseView::ClearResult() {
  if (result_)
    result_->RemoveObserver(this);
  result_ = nullptr;
}

}  // namespace app_list
