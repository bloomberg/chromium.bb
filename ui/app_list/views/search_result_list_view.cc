// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_list_view.h"

#include <algorithm>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/views/search_result_list_view_delegate.h"
#include "ui/app_list/views/search_result_view.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace {

const int kMaxResults = 6;
const int kExperimentAppListMaxResults = 3;
const int kTimeoutIndicatorHeight = 2;
const int kTimeoutFramerate = 60;
const SkColor kTimeoutIndicatorColor = SkColorSetRGB(30, 144, 255);

}  // namespace

namespace app_list {

SearchResultListView::SearchResultListView(
    SearchResultListViewDelegate* delegate,
    AppListViewDelegate* view_delegate)
    : delegate_(delegate),
      view_delegate_(view_delegate),
      results_(NULL),
      results_container_(new views::View),
      auto_launch_indicator_(new views::View),
      last_visible_index_(0),
      selected_index_(-1),
      update_factory_(this) {
  results_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  int max_results = kMaxResults;
  if (app_list::switches::IsExperimentalAppListEnabled())
    max_results = kExperimentAppListMaxResults;

  for (int i = 0; i < max_results; ++i)
    results_container_->AddChildView(new SearchResultView(this));
  AddChildView(results_container_);

  auto_launch_indicator_->set_background(
      views::Background::CreateSolidBackground(kTimeoutIndicatorColor));
  auto_launch_indicator_->SetVisible(false);

  AddChildView(auto_launch_indicator_);
}

SearchResultListView::~SearchResultListView() {
  if (results_)
    results_->RemoveObserver(this);
}

void SearchResultListView::SetResults(AppListModel::SearchResults* results) {
  if (results_)
    results_->RemoveObserver(this);

  results_ = results;
  if (results_)
    results_->AddObserver(this);

  Update();
}

void SearchResultListView::SetSelectedIndex(int selected_index) {
  if (selected_index_ == selected_index)
    return;

  if (selected_index_ >= 0) {
    SearchResultView* selected_view  = GetResultViewAt(selected_index_);
    selected_view->ClearSelectedAction();
    selected_view->SchedulePaint();
  }

  selected_index_ = selected_index;

  if (selected_index_ >= 0) {
    SearchResultView* selected_view  = GetResultViewAt(selected_index_);
    selected_view->ClearSelectedAction();
    selected_view->SchedulePaint();
    selected_view->NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS,
                                            true);
  }
  if (auto_launch_animation_)
    CancelAutoLaunchTimeout();
}

bool SearchResultListView::IsResultViewSelected(
    const SearchResultView* result_view) const {
  if (selected_index_ < 0)
    return false;

  return static_cast<const SearchResultView*>(
      results_container_->child_at(selected_index_)) == result_view;
}

void SearchResultListView::UpdateAutoLaunchState() {
  SetAutoLaunchTimeout(view_delegate_->GetAutoLaunchTimeout());
}

bool SearchResultListView::OnKeyPressed(const ui::KeyEvent& event) {
  if (selected_index_ >= 0 &&
      results_container_->child_at(selected_index_)->OnKeyPressed(event)) {
    return true;
  }

  switch (event.key_code()) {
    case ui::VKEY_TAB:
      if (event.IsShiftDown())
        SetSelectedIndex(std::max(selected_index_ - 1, 0));
      else
        SetSelectedIndex(std::min(selected_index_ + 1, last_visible_index_));
      return true;
    case ui::VKEY_UP:
      SetSelectedIndex(std::max(selected_index_ - 1, 0));
      return true;
    case ui::VKEY_DOWN:
      SetSelectedIndex(std::min(selected_index_ + 1, last_visible_index_));
      return true;
    default:
      break;
  }

  return false;
}

void SearchResultListView::SetAutoLaunchTimeout(
    const base::TimeDelta& timeout) {
  if (timeout > base::TimeDelta()) {
    auto_launch_indicator_->SetVisible(true);
    auto_launch_indicator_->SetBounds(0, 0, 0, kTimeoutIndicatorHeight);
    auto_launch_animation_.reset(new gfx::LinearAnimation(
        timeout.InMilliseconds(), kTimeoutFramerate, this));
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

SearchResultView* SearchResultListView::GetResultViewAt(int index) {
  DCHECK(index >= 0 && index < results_container_->child_count());
  return static_cast<SearchResultView*>(results_container_->child_at(index));
}

void SearchResultListView::Update() {
  std::vector<SearchResult*> display_results =
      AppListModel::FilterSearchResultsByDisplayType(
          results_,
          SearchResult::DISPLAY_LIST,
          results_container_->child_count());
  last_visible_index_ = display_results.size() - 1;

  for (size_t i = 0; i < static_cast<size_t>(results_container_->child_count());
       ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    if (i < display_results.size()) {
      result_view->SetResult(display_results[i]);
      result_view->SetVisible(true);
    } else {
      result_view->SetResult(NULL);
      result_view->SetVisible(false);
    }
  }
  if (selected_index_ > last_visible_index_)
    SetSelectedIndex(last_visible_index_);

  Layout();
  update_factory_.InvalidateWeakPtrs();
  UpdateAutoLaunchState();
}

void SearchResultListView::ScheduleUpdate() {
  // When search results are added one by one, each addition generates an update
  // request. Consolidates those update requests into one Update call.
  if (!update_factory_.HasWeakPtrs()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&SearchResultListView::Update,
                   update_factory_.GetWeakPtr()));
  }
}

void SearchResultListView::ForceAutoLaunchForTest() {
  if (auto_launch_animation_)
    AnimationEnded(auto_launch_animation_.get());
}

void SearchResultListView::Layout() {
  results_container_->SetBoundsRect(GetLocalBounds());
}

gfx::Size SearchResultListView::GetPreferredSize() const {
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
  view_delegate_->OpenSearchResult(results_->GetItemAt(0), true, ui::EF_NONE);

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
  auto_launch_indicator_->SetBounds(
      0, 0, indicator_width, kTimeoutIndicatorHeight);
}

void SearchResultListView::ListItemsAdded(size_t start, size_t count) {
  ScheduleUpdate();
}

void SearchResultListView::ListItemsRemoved(size_t start, size_t count) {
  size_t last = std::min(
      start + count,
      static_cast<size_t>(results_container_->child_count()));
  for (size_t i = start; i < last; ++i)
    GetResultViewAt(i)->ClearResultNoRepaint();

  ScheduleUpdate();
}

void SearchResultListView::ListItemMoved(size_t index, size_t target_index) {
  NOTREACHED();
}

void SearchResultListView::ListItemsChanged(size_t start, size_t count) {
  NOTREACHED();
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
    view_delegate_->InvokeSearchResultAction(
        view->result(), action_index, event_flags);
  }
}

void SearchResultListView::OnSearchResultInstalled(SearchResultView* view) {
  if (delegate_ && view->result())
    delegate_->OnResultInstalled(view->result());
}

void SearchResultListView::OnSearchResultUninstalled(SearchResultView* view) {
  if (delegate_ && view->result())
    delegate_->OnResultUninstalled(view->result());
}

}  // namespace app_list
