// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/productivity_launcher_search_view.h"

#include <limits>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {

namespace {

// The amount of time by which notifications to accessibility framework about
// result page changes are delayed.
constexpr base::TimeDelta kNotifyA11yDelay = base::Milliseconds(1500);

}  // namespace

ProductivityLauncherSearchView::ProductivityLauncherSearchView(
    AppListViewDelegate* view_delegate,
    SearchResultPageDialogController* dialog_controller,
    SearchBoxView* search_box_view)
    : dialog_controller_(dialog_controller), search_box_view_(search_box_view) {
  DCHECK(view_delegate);
  DCHECK(search_box_view_);
  SetUseDefaultFillLayout(true);

  // The entire page scrolls. Use layer scrolling so that the contents will
  // paint on top of the parent, which uses SetPaintToLayer().
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  // Don't paint a background. The bubble already has one.
  scroll_view_->SetBackgroundColor(absl::nullopt);

  auto scroll_contents = std::make_unique<views::View>();
  scroll_contents->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));

  result_selection_controller_ = std::make_unique<ResultSelectionController>(
      &result_container_views_,
      base::BindRepeating(
          &ProductivityLauncherSearchView::OnSelectedResultChanged,
          base::Unretained(this)));
  search_box_view_->SetResultSelectionController(
      result_selection_controller_.get());

  auto add_result_container = [&](SearchResultListView* new_container) {
    new_container->SetResults(
        AppListModelProvider::Get()->search_model()->results());
    new_container->set_delegate(this);
    result_container_views_.push_back(new_container);
  };

  // kAnswerCard is always the first list view shown.
  auto* answer_card_container =
      scroll_contents->AddChildView(std::make_unique<SearchResultListView>(
          /*main_view=*/nullptr, view_delegate, dialog_controller_,
          SearchResultView::SearchResultViewType::kAnswerCard,
          /*animates_result_updates=*/true, absl::nullopt));
  answer_card_container->SetListType(
      SearchResultListView::SearchResultListType::kAnswerCard);
  add_result_container(answer_card_container);

  // kBestMatch is always the second list view shown.
  auto* best_match_container =
      scroll_contents->AddChildView(std::make_unique<SearchResultListView>(
          /*main_view=*/nullptr, view_delegate, dialog_controller_,
          SearchResultView::SearchResultViewType::kDefault,
          /*animated_result_updates=*/true, absl::nullopt));
  best_match_container->SetListType(
      SearchResultListView::SearchResultListType::kBestMatch);
  add_result_container(best_match_container);

  // SearchResultListViews are aware of their relative position in the
  // Productivity launcher search view. SearchResultListViews with mutable
  // positions are passed their productivity_launcher_search_view_position to
  // update their own category type. kAnswerCard and kBestMatch have already
  // been constructed.
  const size_t category_count =
      SearchResultListView::GetAllListTypesForCategoricalSearch().size() -
      result_container_views_.size();
  for (size_t i = 0; i < category_count; ++i) {
    auto* result_container =
        scroll_contents->AddChildView(std::make_unique<SearchResultListView>(
            /*main_view=*/nullptr, view_delegate, dialog_controller_,
            SearchResultView::SearchResultViewType::kDefault,
            /*animates_result_updates=*/true, i));
    add_result_container(result_container);
  }

  scroll_view_->SetContents(std::move(scroll_contents));

  AppListModelProvider::Get()->AddObserver(this);
}

ProductivityLauncherSearchView::~ProductivityLauncherSearchView() {
  AppListModelProvider::Get()->RemoveObserver(this);
}

void ProductivityLauncherSearchView::OnSearchResultContainerResultsChanging() {
  // Block any result selection changes while result updates are in flight.
  // The selection will be reset once the results are all updated.
  result_selection_controller_->set_block_selection_changes(true);

  notify_a11y_results_changed_timer_.Stop();
  SetIgnoreResultChangesForA11y(true);
}

void ProductivityLauncherSearchView::OnSearchResultContainerResultsChanged() {
  DCHECK(!result_container_views_.empty());

  int result_count = 0;
  // Only sort and layout the containers when they have all updated.
  for (SearchResultContainerView* view : result_container_views_) {
    if (view->UpdateScheduled())
      return;
    result_count += view->num_results();
  }

  // If the user cleared the search box text, skip animating the views. The
  // visible views will animate out and the whole search page will be hidden.
  // See AppListBubbleSearchPage::AnimateHidePage().
  if (search_box_view_->HasSearch()) {
    using AnimationInfo = SearchResultContainerView::ResultsAnimationInfo;
    AnimationInfo aggregate_animation_info;
    for (SearchResultContainerView* view : result_container_views_) {
      absl::optional<AnimationInfo> container_animation_info =
          view->ScheduleResultAnimations(aggregate_animation_info);
      DCHECK(container_animation_info);
      aggregate_animation_info.total_views +=
          container_animation_info->total_views;
      aggregate_animation_info.animating_views +=
          container_animation_info->animating_views;
    }
  }
  Layout();

  last_search_result_count_ = result_count;

  ScheduleResultsChangedA11yNotification();
  // Find the first result view.
  DCHECK(!result_container_views_.empty());
  SearchResultBaseView* first_result_view =
      result_container_views_.front()->GetFirstResultView();

  // Reset selection to first when things change. The first result is set as
  // as the default result.
  result_selection_controller_->set_block_selection_changes(false);
  result_selection_controller_->ResetSelection(/*key_event=*/nullptr,
                                               /*default_selection=*/true);
  // Update SearchBoxView search box autocomplete as necessary based on new
  // first result view.
  search_box_view_->ProcessAutocomplete(first_result_view);
}

void ProductivityLauncherSearchView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  if (!GetVisible())
    return;

  node_data->role = ax::mojom::Role::kListBox;

  std::u16string value;
  std::u16string query =
      AppListModelProvider::Get()->search_model()->search_box()->text();
  if (!query.empty()) {
    if (last_search_result_count_ == 1) {
      value = l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCHBOX_RESULTS_ACCESSIBILITY_ANNOUNCEMENT_SINGLE_RESULT,
          query);
    } else {
      value = l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCHBOX_RESULTS_ACCESSIBILITY_ANNOUNCEMENT,
          base::NumberToString16(last_search_result_count_), query);
    }
  } else {
    // TODO(crbug.com/1204551): New(?) accessibility announcement. We used to
    // have a zero state A11Y announcement but zero state is removed for the
    // bubble launcher.
    value = std::u16string();
  }

  node_data->SetValue(value);
}

void ProductivityLauncherSearchView::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  for (auto* container : result_container_views_)
    container->SetResults(search_model->results());
}

void ProductivityLauncherSearchView::OnSelectedResultChanged() {
  if (!result_selection_controller_->selected_result()) {
    return;
  }

  views::View* selected_row = result_selection_controller_->selected_result();
  selected_row->ScrollViewToVisible();

  MaybeNotifySelectedResultChanged();
}

void ProductivityLauncherSearchView::SetIgnoreResultChangesForA11y(
    bool ignore) {
  if (ignore_result_changes_for_a11y_ == ignore)
    return;
  ignore_result_changes_for_a11y_ = ignore;
  SetViewIgnoredForAccessibility(this, ignore);
}

void ProductivityLauncherSearchView::ScheduleResultsChangedA11yNotification() {
  if (!ignore_result_changes_for_a11y_) {
    NotifyA11yResultsChanged();
    return;
  }

  notify_a11y_results_changed_timer_.Start(
      FROM_HERE, kNotifyA11yDelay,
      base::BindOnce(&ProductivityLauncherSearchView::NotifyA11yResultsChanged,
                     base::Unretained(this)));
}

void ProductivityLauncherSearchView::NotifyA11yResultsChanged() {
  SetIgnoreResultChangesForA11y(false);

  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
  MaybeNotifySelectedResultChanged();
}

void ProductivityLauncherSearchView::MaybeNotifySelectedResultChanged() {
  if (ignore_result_changes_for_a11y_)
    return;

  if (!result_selection_controller_->selected_result()) {
    search_box_view_->SetA11yActiveDescendant(absl::nullopt);
    return;
  }

  views::View* selected_view =
      result_selection_controller_->selected_result()->GetSelectedView();
  if (!selected_view) {
    search_box_view_->SetA11yActiveDescendant(absl::nullopt);
    return;
  }

  search_box_view_->SetA11yActiveDescendant(
      selected_view->GetViewAccessibility().GetUniqueId().Get());
}

bool ProductivityLauncherSearchView::CanSelectSearchResults() {
  DCHECK(!result_container_views_.empty());
  return last_search_result_count_ > 0;
}

int ProductivityLauncherSearchView::TabletModePreferredHeight() {
  int max_height = 0;
  for (SearchResultContainerView* view : result_container_views_) {
    if (view->GetVisible()) {
      max_height += view->GetPreferredSize().height();
    }
  }
  return max_height;
}

ui::Layer* ProductivityLauncherSearchView::GetPageAnimationLayer() const {
  // The scroll view has a layer containing all the visible contents, so use
  // that for "whole page" animations.
  return scroll_view_->contents()->layer();
}

BEGIN_METADATA(ProductivityLauncherSearchView, views::View)
END_METADATA

}  // namespace ash
