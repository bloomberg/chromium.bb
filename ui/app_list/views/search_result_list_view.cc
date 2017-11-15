// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_list_view.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/search_result_view.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace {

constexpr int kMaxResults = 5;
constexpr int kTimeoutIndicatorHeight = 2;
constexpr int kTimeoutFramerate = 60;
constexpr SkColor kTimeoutIndicatorColor =
    SkColorSetARGBMacro(255, 30, 144, 255);

}  // namespace

namespace app_list {

SearchResultListView::SearchResultListView(AppListMainView* main_view,
                                           AppListViewDelegate* view_delegate)
    : main_view_(main_view),
      view_delegate_(view_delegate),
      results_container_(new views::View),
      auto_launch_indicator_(new views::View) {
  results_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical));

  for (int i = 0; i < kMaxResults; ++i)
    results_container_->AddChildView(new SearchResultView(this));
  AddChildView(results_container_);

  auto_launch_indicator_->SetBackground(
      views::CreateSolidBackground(kTimeoutIndicatorColor));
  auto_launch_indicator_->SetVisible(false);

  AddChildView(auto_launch_indicator_);
}

SearchResultListView::~SearchResultListView() {}

bool SearchResultListView::IsResultViewSelected(
    const SearchResultView* result_view) const {
  if (selected_index() < 0)
    return false;

  return static_cast<const SearchResultView*>(
             results_container_->child_at(selected_index())) == result_view;
}

void SearchResultListView::UpdateAutoLaunchState() {
  SetAutoLaunchTimeout(view_delegate_->GetAutoLaunchTimeout());
}

bool SearchResultListView::OnKeyPressed(const ui::KeyEvent& event) {
  if (features::IsAppListFocusEnabled()) {
    // TODO(weidongg/766807) Remove this function when the flag is enabled by
    // default.
    return false;
  }
  if (selected_index() >= 0 &&
      results_container_->child_at(selected_index())->OnKeyPressed(event)) {
    return true;
  }

  int selection_index = -1;
  const int forward_dir = base::i18n::IsRTL() ? -1 : 1;
  switch (event.key_code()) {
    case ui::VKEY_TAB:
      if (event.IsShiftDown())
        selection_index = selected_index() - 1;
      else
        selection_index = selected_index() + 1;
      break;
    case ui::VKEY_UP:
      selection_index = selected_index() - 1;
      break;
    case ui::VKEY_DOWN:
      selection_index = selected_index() + 1;
      break;
    case ui::VKEY_LEFT:
      selection_index = selected_index() - forward_dir;
      break;
    case ui::VKEY_RIGHT:
      selection_index = selected_index() + forward_dir;
      break;
    default:
      break;
  }

  if (IsValidSelectionIndex(selection_index)) {
    SetSelectedIndex(selection_index);
    if (auto_launch_animation_)
      CancelAutoLaunchTimeout();
    return true;
  }

  return false;
}

void SearchResultListView::SetAutoLaunchTimeout(
    const base::TimeDelta& timeout) {
  if (timeout > base::TimeDelta()) {
    auto_launch_indicator_->SetVisible(true);
    auto_launch_indicator_->SetBounds(0, 0, 0, kTimeoutIndicatorHeight);
    auto_launch_animation_.reset(
        new gfx::LinearAnimation(timeout, kTimeoutFramerate, this));
    auto_launch_animation_->Start();
  } else {
    auto_launch_indicator_->SetVisible(false);
    auto_launch_animation_.reset();
  }
}

void SearchResultListView::CancelAutoLaunchTimeout() {
  SetAutoLaunchTimeout(base::TimeDelta());
  view_delegate_->AutoLaunchCanceled();
}

SearchResultView* SearchResultListView::GetResultViewAt(int index) const {
  DCHECK(index >= 0 && index < results_container_->child_count());
  return static_cast<SearchResultView*>(results_container_->child_at(index));
}

void SearchResultListView::ListItemsRemoved(size_t start, size_t count) {
  size_t last = std::min(
      start + count, static_cast<size_t>(results_container_->child_count()));
  for (size_t i = start; i < last; ++i)
    GetResultViewAt(i)->ClearResultNoRepaint();

  SearchResultContainerView::ListItemsRemoved(start, count);
}

void SearchResultListView::OnContainerSelected(bool from_bottom,
                                               bool /*directional_movement*/) {
  if (num_results() == 0)
    return;

  SetSelectedIndex(from_bottom ? num_results() - 1 : 0);
}

void SearchResultListView::NotifyFirstResultYIndex(int y_index) {
  for (size_t i = 0; i < static_cast<size_t>(num_results()); ++i)
    GetResultViewAt(i)->result()->set_distance_from_origin(i + y_index);
}

int SearchResultListView::GetYSize() {
  return num_results();
}

views::View* SearchResultListView::GetSelectedView() const {
  return IsValidSelectionIndex(selected_index())
             ? GetResultViewAt(selected_index())
             : nullptr;
}

views::View* SearchResultListView::SetFirstResultSelected(bool selected) {
  DCHECK(results_container_->has_children());
  if (num_results() <= 0)
    return nullptr;
  SearchResultView* search_result_view =
      static_cast<SearchResultView*>(results_container_->child_at(0));
  search_result_view->SetSelected(selected);
  return search_result_view;
}

int SearchResultListView::DoUpdate() {
  std::vector<SearchResult*> display_results =
      AppListModel::FilterSearchResultsByDisplayType(
          results(), SearchResult::DISPLAY_LIST,
          results_container_->child_count());

  for (size_t i = 0; i < static_cast<size_t>(results_container_->child_count());
       ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    result_view->set_is_last_result(i == display_results.size() - 1);
    if (i < display_results.size()) {
      result_view->SetResult(display_results[i]);
      result_view->SetVisible(true);
    } else {
      result_view->SetResult(NULL);
      result_view->SetVisible(false);
    }
  }
  UpdateAutoLaunchState();

  set_container_score(
      display_results.empty() ? 0 : display_results.front()->relevance());

  return display_results.size();
}

void SearchResultListView::UpdateSelectedIndex(int old_selected,
                                               int new_selected) {
  if (old_selected >= 0) {
    SearchResultView* selected_view = GetResultViewAt(old_selected);
    selected_view->ClearSelectedAction();
    selected_view->SchedulePaint();
  }

  if (new_selected >= 0) {
    SearchResultView* selected_view = GetResultViewAt(new_selected);
    ScrollRectToVisible(selected_view->bounds());
    selected_view->ClearSelectedAction();
    selected_view->SchedulePaint();
    selected_view->NotifyAccessibilityEvent(ui::AX_EVENT_SELECTION, true);
  }
}

void SearchResultListView::ForceAutoLaunchForTest() {
  if (auto_launch_animation_)
    AnimationEnded(auto_launch_animation_.get());
}

void SearchResultListView::Layout() {
  results_container_->SetBoundsRect(GetLocalBounds());
}

gfx::Size SearchResultListView::CalculatePreferredSize() const {
  return results_container_->GetPreferredSize();
}

int SearchResultListView::GetHeightForWidth(int w) const {
  return results_container_->GetHeightForWidth(w);
}

void SearchResultListView::VisibilityChanged(views::View* starting_from,
                                             bool is_visible) {
  if (is_visible)
    UpdateAutoLaunchState();
  else
    CancelAutoLaunchTimeout();
}

void SearchResultListView::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(auto_launch_animation_.get(), animation);
  if (results()->item_count() > 0) {
    view_delegate_->OpenSearchResult(results()->GetItemAt(0), true,
                                     ui::EF_NONE);
  }

  // The auto-launch has to be canceled explicitly. Think that one of searcher
  // is extremely slow. Sometimes the events would happen in the following
  // order:
  //  1. The search results arrive, auto-launch is dispatched
  //  2. Timed out and auto-launch the first search result
  //  3. Then another searcher adds search results more
  // At the step 3, we shouldn't dispatch the auto-launch again.
  CancelAutoLaunchTimeout();
}

void SearchResultListView::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(auto_launch_animation_.get(), animation);
  int indicator_width = auto_launch_animation_->CurrentValueBetween(0, width());
  auto_launch_indicator_->SetBounds(0, 0, indicator_width,
                                    kTimeoutIndicatorHeight);
}

void SearchResultListView::SearchResultActivated(SearchResultView* view,
                                                 int event_flags) {
  if (view_delegate_ && view->result())
    view_delegate_->OpenSearchResult(view->result(), false, event_flags);
}

void SearchResultListView::SearchResultActionActivated(SearchResultView* view,
                                                       size_t action_index,
                                                       int event_flags) {
  if (view_delegate_ && view->result()) {
    view_delegate_->InvokeSearchResultAction(view->result(), action_index,
                                             event_flags);
  }
}

void SearchResultListView::OnSearchResultInstalled(SearchResultView* view) {
  if (main_view_ && view->result())
    main_view_->OnResultInstalled(view->result());
}

}  // namespace app_list
