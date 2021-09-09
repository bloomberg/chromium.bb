// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_base_view.h"

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_actions_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {

SearchResultBaseView::SearchResultBaseView() {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetInstallFocusRingOnFocus(false);
}

SearchResultBaseView::~SearchResultBaseView() {
  if (result_)
    result_->RemoveObserver(this);
  result_ = nullptr;
}

bool SearchResultBaseView::SkipDefaultKeyEventProcessing(
    const ui::KeyEvent& event) {
  // Ensure accelerators take priority in the app list. This ensures, e.g., that
  // Ctrl+Space will switch input methods rather than activate the button.
  return false;
}

const char* SearchResultBaseView::GetClassName() const {
  return "SearchResultBaseView";
}

void SearchResultBaseView::SetSelected(bool selected,
                                       absl::optional<bool> reverse_tab_order) {
  if (selected_ == selected)
    return;

  selected_ = selected;

  if (selected) {
    SelectInitialResultAction(reverse_tab_order.value_or(false));
  } else {
    ClearSelectedResultAction();
  }

  SchedulePaint();
}

bool SearchResultBaseView::SelectNextResultAction(bool reverse_tab_order) {
  if (!selected() || !actions_view_)
    return false;

  if (!actions_view_->SelectNextAction(reverse_tab_order))
    return false;

  SchedulePaint();
  return true;
}

views::View* SearchResultBaseView::GetSelectedView() {
  if (actions_view_ && actions_view_->HasSelectedAction())
    return actions_view_->GetSelectedView();
  return this;
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

std::u16string SearchResultBaseView::ComputeAccessibleName() const {
  if (!result())
    return std::u16string();

  if (!result()->accessible_name().empty())
    return result()->accessible_name();

  std::u16string accessible_name = result()->title();
  if (!result()->title().empty() && !result()->details().empty())
    accessible_name += u", ";
  accessible_name += result()->details();

  return accessible_name;
}

void SearchResultBaseView::UpdateAccessibleName() {
  SetAccessibleName(ComputeAccessibleName());
}

void SearchResultBaseView::ClearResult() {
  if (result_)
    result_->RemoveObserver(this);
  SetSelected(false, absl::nullopt);
  result_ = nullptr;
}

void SearchResultBaseView::SelectInitialResultAction(bool reverse_tab_order) {
  if (actions_view_)
    actions_view_->SelectInitialAction(reverse_tab_order);
}

void SearchResultBaseView::ClearSelectedResultAction() {
  if (actions_view_)
    actions_view_->ClearSelectedAction();
}

}  // namespace ash
