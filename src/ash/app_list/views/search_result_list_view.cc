// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_list_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr base::TimeDelta kImpressionThreshold =
    base::TimeDelta::FromSeconds(3);

SearchResultIdWithPositionIndices GetSearchResultsForLogging(
    std::vector<SearchResultView*> search_result_views) {
  SearchResultIdWithPositionIndices results;
  for (const auto* item : search_result_views) {
    if (item->result()) {
      results.emplace_back(SearchResultIdWithPositionIndex(
          item->result()->id(), item->index_in_container()));
    }
  }
  return results;
}

}  // namespace

SearchResultListView::SearchResultListView(AppListMainView* main_view,
                                           AppListViewDelegate* view_delegate)
    : SearchResultContainerView(view_delegate),
      main_view_(main_view),
      view_delegate_(view_delegate),
      results_container_(new views::View) {
  results_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  size_t result_count =
      SharedAppListConfig::instance().max_search_result_list_items() +
      SharedAppListConfig::instance().max_assistant_search_result_list_items();

  for (size_t i = 0; i < result_count; ++i) {
    search_result_views_.emplace_back(
        new SearchResultView(this, view_delegate_));
    search_result_views_.back()->set_index_in_container(i);
    results_container_->AddChildView(search_result_views_.back());
    AddObservedResultView(search_result_views_.back());
  }
  AddChildView(results_container_);
}

SearchResultListView::~SearchResultListView() = default;

void SearchResultListView::ListItemsRemoved(size_t start, size_t count) {
  size_t last = std::min(start + count, search_result_views_.size());
  for (size_t i = start; i < last; ++i)
    GetResultViewAt(i)->ClearResult();

  SearchResultContainerView::ListItemsRemoved(start, count);
}

SearchResultView* SearchResultListView::GetResultViewAt(size_t index) {
  DCHECK(index >= 0 && index < search_result_views_.size());
  return search_result_views_[index];
}

int SearchResultListView::DoUpdate() {
  if (!GetWidget() || !GetWidget()->IsVisible()) {
    for (auto* result_view : search_result_views_) {
      result_view->SetResult(nullptr);
      result_view->SetVisible(false);
    }
    return 0;
  }

  std::vector<SearchResult*> display_results = GetSearchResults();

  for (size_t i = 0; i < search_result_views_.size(); ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    if (i < display_results.size()) {
      result_view->SetResult(display_results[i]);
      result_view->SetVisible(true);
    } else {
      result_view->SetResult(nullptr);
      result_view->SetVisible(false);
    }
  }

  auto* notifier = view_delegate_->GetNotifier();
  if (notifier) {
    std::vector<AppListNotifier::Result> notifier_results;
    for (const auto* result : display_results)
      notifier_results.emplace_back(result->id(), result->metrics_type());
    notifier->NotifyResultsUpdated(SearchResultDisplayType::kList,
                                   notifier_results);
  }

  // Logic for logging impression of items that were shown to user.
  // Each time DoUpdate() called, start a timer that will be fired after a
  // certain amount of time |kImpressionThreshold|. If during the waiting time,
  // there's another DoUpdate() called, reset the timer and start a new timer
  // with updated result list.
  if (impression_timer_.IsRunning())
    impression_timer_.Stop();
  impression_timer_.Start(FROM_HERE, kImpressionThreshold, this,
                          &SearchResultListView::LogImpressions);
  return display_results.size();
}

void SearchResultListView::LogImpressions() {
  // Since no items is actually clicked, send the position index of clicked item
  // as -1.
  if (main_view_->search_box_view()->is_search_box_active()) {
    view_delegate_->NotifySearchResultsForLogging(
        view_delegate_->GetSearchModel()->search_box()->text(),
        GetSearchResultsForLogging(search_result_views_),
        -1 /* position_index */);
  }
}

void SearchResultListView::Layout() {
  results_container_->SetBoundsRect(GetLocalBounds());
}

gfx::Size SearchResultListView::CalculatePreferredSize() const {
  return results_container_->GetPreferredSize();
}

const char* SearchResultListView::GetClassName() const {
  return "SearchResultListView";
}

int SearchResultListView::GetHeightForWidth(int w) const {
  return results_container_->GetHeightForWidth(w);
}

void SearchResultListView::SearchResultActivated(SearchResultView* view,
                                                 int event_flags,
                                                 bool by_button_press) {
  if (!view_delegate_ || !view || !view->result())
    return;

  auto* result = view->result();

  RecordSearchResultOpenSource(result, view_delegate_->GetModel(),
                               view_delegate_->GetSearchModel());
  view_delegate_->NotifySearchResultsForLogging(
      view_delegate_->GetSearchModel()->search_box()->text(),
      GetSearchResultsForLogging(search_result_views_),
      view->index_in_container());

  view_delegate_->OpenSearchResult(
      result->id(), event_flags, AppListLaunchedFrom::kLaunchedFromSearchBox,
      AppListLaunchType::kSearchResult, -1 /* suggestion_index */,
      !by_button_press && view->is_default_result() /* launch_as_default */);
}

void SearchResultListView::SearchResultActionActivated(SearchResultView* view,
                                                       size_t action_index) {
  if (view_delegate_ && view->result()) {
    OmniBoxZeroStateAction action = GetOmniBoxZeroStateAction(action_index);
    if (action == OmniBoxZeroStateAction::kRemoveSuggestion) {
      view_delegate_->InvokeSearchResultAction(view->result()->id(),
                                               action_index);
    } else if (action == OmniBoxZeroStateAction::kAppendSuggestion) {
      main_view_->search_box_view()->UpdateQuery(view->result()->title());
    }
  }
}

void SearchResultListView::OnSearchResultInstalled(SearchResultView* view) {
  if (main_view_ && view->result())
    main_view_->OnResultInstalled(view->result());
}

void SearchResultListView::VisibilityChanged(View* starting_from,
                                             bool is_visible) {
  SearchResultContainerView::VisibilityChanged(starting_from, is_visible);
  // We only do this work when is_visible is false.
  if (is_visible)
    return;
}

std::vector<SearchResult*> SearchResultListView::GetAssistantResults() {
  // Only show Assistant results if there are no tiles. There is not enough
  // room in launcher to display Assistant results if there are tiles visible.
  bool visible_tiles = !SearchModel::FilterSearchResultsByDisplayType(
                            results(), SearchResult::DisplayType::kTile,
                            /*excludes=*/{}, /*max_results=*/1)
                            .empty();

  if (visible_tiles)
    return std::vector<SearchResult*>();

  return SearchModel::FilterSearchResultsByFunction(
      results(), base::BindRepeating([](const SearchResult& search_result) {
        return search_result.display_type() == SearchResultDisplayType::kList &&
               search_result.result_type() ==
                   AppListSearchResultType::kAssistantText;
      }),
      /*max_results=*/
      SharedAppListConfig::instance().max_assistant_search_result_list_items());
}

std::vector<SearchResult*> SearchResultListView::GetSearchResults() {
  std::vector<SearchResult*> search_results =
      SearchModel::FilterSearchResultsByFunction(
          results(), base::BindRepeating([](const SearchResult& result) {
            return result.display_type() == SearchResultDisplayType::kList &&
                   result.result_type() !=
                       AppListSearchResultType::kAssistantText;
          }),
          /*max_results=*/
          SharedAppListConfig::instance().max_search_result_list_items());

  std::vector<SearchResult*> assistant_results = GetAssistantResults();

  search_results.insert(search_results.end(), assistant_results.begin(),
                        assistant_results.end());

  return search_results;
}

}  // namespace ash
