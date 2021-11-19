// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/recent_apps_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_config_provider.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr size_t kMinRecommendedApps = 4;
constexpr size_t kMaxRecommendedApps = 5;

// Sorts increasing by display index, then decreasing by position priority.
struct CompareByDisplayIndexAndPositionPriority {
  bool operator()(const SearchResult* result1,
                  const SearchResult* result2) const {
    SearchResultDisplayIndex index1 = result1->display_index();
    SearchResultDisplayIndex index2 = result2->display_index();
    if (index1 != index2)
      return index1 < index2;
    return result1->position_priority() > result2->position_priority();
  }
};

// Converts a search result app ID to an app list item ID.
std::string ItemIdFromAppId(const std::string& app_id) {
  // Convert chrome-extension://<id> to just <id>.
  if (base::StartsWith(app_id, extensions::kExtensionScheme)) {
    GURL url(app_id);
    return url.host();
  }
  return app_id;
}

// Returns true if `type` is an application result.
bool IsAppType(AppListSearchResultType type) {
  return type == AppListSearchResultType::kInstalledApp ||
         type == AppListSearchResultType::kPlayStoreApp ||
         type == AppListSearchResultType::kInstantApp ||
         type == AppListSearchResultType::kInternalApp;
}

// Returns a list of recent apps by filtering suggestion chip data.
// TODO(crbug.com/1216662): Replace with a real implementation after the ML team
// gives us a way to query directly for recent apps.
std::vector<std::string> GetRecentAppIdsFromSuggestionChips(
    SearchModel* search_model) {
  SearchModel::SearchResults* results = search_model->results();
  auto is_app_suggestion = [](const SearchResult& r) -> bool {
    return IsAppType(r.result_type()) &&
           r.display_type() == SearchResultDisplayType::kList;
  };
  std::vector<SearchResult*> app_suggestion_results =
      SearchModel::FilterSearchResultsByFunction(
          results, base::BindRepeating(is_app_suggestion),
          /*max_results=*/kMaxRecommendedApps);

  std::sort(app_suggestion_results.begin(), app_suggestion_results.end(),
            CompareByDisplayIndexAndPositionPriority());

  std::vector<std::string> app_ids;
  for (SearchResult* result : app_suggestion_results)
    app_ids.push_back(result->id());
  return app_ids;
}

}  // namespace

// The grid delegate for each AppListItemView. Recent app icons cannot be
// dragged, so this implementation is mostly a stub.
class RecentAppsView::GridDelegateImpl : public AppListItemView::GridDelegate {
 public:
  explicit GridDelegateImpl(AppListViewDelegate* view_delegate)
      : view_delegate_(view_delegate) {}
  GridDelegateImpl(const GridDelegateImpl&) = delete;
  GridDelegateImpl& operator=(const GridDelegateImpl&) = delete;
  ~GridDelegateImpl() override = default;

  // AppListItemView::GridDelegate:
  bool IsInFolder() const override { return false; }
  void SetSelectedView(AppListItemView* view) override {
    DCHECK(view);
    if (view == selected_view_)
      return;
    // Ensure the translucent background of the previous selection goes away.
    if (selected_view_)
      selected_view_->SchedulePaint();
    selected_view_ = view;
    // Ensure the translucent background of this selection is painted.
    selected_view_->SchedulePaint();
  }
  void ClearSelectedView() override { selected_view_ = nullptr; }
  bool IsSelectedView(const AppListItemView* view) const override {
    return view == selected_view_;
  }
  bool InitiateDrag(AppListItemView* view,
                    const gfx::Point& location,
                    const gfx::Point& root_location,
                    base::OnceClosure drag_start_callback,
                    base::OnceClosure drag_end_callback) override {
    return false;
  }
  void StartDragAndDropHostDragAfterLongPress() override {}
  bool UpdateDragFromItem(bool is_touch,
                          const ui::LocatedEvent& event) override {
    return false;
  }
  void EndDrag(bool cancel) override {}
  void OnAppListItemViewActivated(AppListItemView* pressed_item_view,
                                  const ui::Event& event) override {
    // TODO(crbug.com/1216594): Add a new launch type for "recent apps".
    // NOTE: Avoid using |item->id()| as the parameter. In some rare situations,
    // activating the item may destruct it. Using the reference to an object
    // which may be destroyed during the procedure as the function parameter
    // may bring the crash like https://crbug.com/990282.
    const std::string id = pressed_item_view->item()->id();
    view_delegate_->ActivateItem(
        id, event.flags(), AppListLaunchedFrom::kLaunchedFromSuggestionChip);
    // `this` may be deleted.
  }

 private:
  AppListViewDelegate* const view_delegate_;
  AppListItemView* selected_view_ = nullptr;
};

RecentAppsView::RecentAppsView(Delegate* delegate,
                               AppListViewDelegate* view_delegate)
    : delegate_(delegate),
      view_delegate_(view_delegate),
      grid_delegate_(std::make_unique<GridDelegateImpl>(view_delegate_)) {
  DCHECK(delegate_);
  DCHECK(view_delegate_);
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  layout_->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  SetVisible(false);
}

RecentAppsView::~RecentAppsView() = default;

void RecentAppsView::UpdateAppListConfig(const AppListConfig* app_list_config) {
  app_list_config_ = app_list_config;

  for (auto* item_view : item_views_)
    item_view->UpdateAppListConfig(app_list_config);
}

void RecentAppsView::ShowResults(SearchModel* search_model,
                                 AppListModel* model) {
  DCHECK(app_list_config_);
  item_views_.clear();
  RemoveAllChildViews();

  std::vector<std::string> app_ids =
      GetRecentAppIdsFromSuggestionChips(search_model);
  std::vector<AppListItem*> items;

  for (const std::string& app_id : app_ids) {
    std::string item_id = ItemIdFromAppId(app_id);
    AppListItem* item = model->FindItem(item_id);
    if (item)
      items.push_back(item);
  }

  if (items.size() < kMinRecommendedApps) {
    SetVisible(false);
    return;
  }

  SetVisible(true);

  for (AppListItem* item : items) {
    auto* item_view = AddChildView(std::make_unique<AppListItemView>(
        app_list_config_, grid_delegate_.get(), item, view_delegate_,
        AppListItemView::Context::kRecentAppsView));
    item_view->UpdateAppListConfig(app_list_config_);
    item_views_.push_back(item_view);
  }
}

int RecentAppsView::GetItemViewCount() const {
  return item_views_.size();
}

AppListItemView* RecentAppsView::GetItemViewAt(int index) const {
  if (static_cast<int>(item_views_.size()) <= index)
    return nullptr;
  return item_views_[index];
}

void RecentAppsView::DisableFocusForShowingActiveFolder(bool disabled) {
  for (views::View* child : children())
    child->SetEnabled(!disabled);

  // Prevent items from being accessed by ChromeVox.
  SetViewIgnoredForAccessibility(this, disabled);
}

bool RecentAppsView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_UP) {
    MoveFocusUp();
    return true;
  }
  if (event.key_code() == ui::VKEY_DOWN) {
    MoveFocusDown();
    return true;
  }
  return false;
}

void RecentAppsView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // The AppsGridView's space between items is the sum of the padding on left
  // and on right of the individual tiles. Because of rounding errors, there can
  // be an actual difference of 1px over the actual distribution of space
  // needed, and because this is not compensated on the other columns, the grid
  // carries over the error making it progressively more significant for each
  // column. For the RecentAppsView tiles to match the grid we need to calculate
  // padding as the AppsGridView does to account for the rounding errors and
  // then double it, so it is exactly the same spacing as the AppsGridView.
  layout_->set_between_child_spacing(2 * CalculateTilePadding());
}

void RecentAppsView::MoveFocusUp() {
  DVLOG(1) << __FUNCTION__;
  // This function should only run when a child has focus.
  DCHECK(Contains(GetFocusManager()->GetFocusedView()));
  DCHECK(!children().empty());
  delegate_->MoveFocusUpFromRecents();
}

void RecentAppsView::MoveFocusDown() {
  DVLOG(1) << __FUNCTION__;
  // This function should only run when a child has focus.
  DCHECK(Contains(GetFocusManager()->GetFocusedView()));
  int column = GetColumnOfFocusedChild();
  DCHECK_GE(column, 0);
  delegate_->MoveFocusDownFromRecents(column);
}

int RecentAppsView::GetColumnOfFocusedChild() const {
  int column = 0;
  for (views::View* child : children()) {
    if (!views::IsViewClass<AppListItemView>(child))
      continue;
    if (child->HasFocus())
      return column;
    ++column;
  }
  return -1;
}

int RecentAppsView::CalculateTilePadding() const {
  int content_width = GetContentsBounds().width();
  int tile_width = app_list_config_->grid_tile_width();
  int width_to_distribute = content_width - kMaxRecommendedApps * tile_width;

  return width_to_distribute / ((kMaxRecommendedApps - 1) * 2);
}

BEGIN_METADATA(RecentAppsView, views::View)
END_METADATA

}  // namespace ash
