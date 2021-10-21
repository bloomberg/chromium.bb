// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_TEST_VIEW_DELEGATE_H_
#define ASH_APP_LIST_APP_LIST_TEST_VIEW_DELEGATE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {
namespace test {

class AppListTestModel;

// A concrete AppListViewDelegate for unit tests.
class AppListTestViewDelegate : public AppListViewDelegate,
                                public ui::SimpleMenuModel::Delegate {
 public:
  AppListTestViewDelegate();

  AppListTestViewDelegate(const AppListTestViewDelegate&) = delete;
  AppListTestViewDelegate& operator=(const AppListTestViewDelegate&) = delete;

  ~AppListTestViewDelegate() override;

  int dismiss_count() const { return dismiss_count_; }
  int open_search_result_count() const { return open_search_result_count_; }
  int open_assistant_ui_count() const { return open_assistant_ui_count_; }
  std::map<size_t, int>& open_search_result_counts() {
    return open_search_result_counts_;
  }
  int show_wallpaper_context_menu_count() const {
    return show_wallpaper_context_menu_count_;
  }

  // Sets the number of apps that the model will be created with the next time
  // SetProfileByPath() is called.
  void set_next_profile_app_count(int apps) { next_profile_app_count_ = apps; }

  // Sets whether the search engine is Google or not.
  void SetSearchEngineIsGoogle(bool is_google);

  // Set whether tablet mode is enabled.
  void SetIsTabletModeEnabled(bool is_tablet_mode);

  // Set whether the suggested content info should be shown.
  void SetShouldShowSuggestedContentInfo(bool should_show);

  // AppListViewDelegate overrides:
  AppListModel* GetModel() override;
  SearchModel* GetSearchModel() override;
  bool KeyboardTraversalEngaged() override;
  void StartAssistant() override {}
  void StartSearch(const std::u16string& raw_query) override {}
  void OpenSearchResult(const std::string& result_id,
                        ash::AppListSearchResultType result_type,
                        int event_flags,
                        ash::AppListLaunchedFrom launched_from,
                        ash::AppListLaunchType launch_type,
                        int suggestion_index,
                        bool launch_as_default) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index) override {}
  void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetContextMenuModelCallback callback) override;
  void ViewShown(int64_t display_id) override {}
  void DismissAppList() override;
  void ViewClosing() override {}
  const std::vector<SkColor>& GetWallpaperProminentColors() override;
  void ActivateItem(const std::string& id,
                    int event_flags,
                    ash::AppListLaunchedFrom launched_from) override;
  void GetContextMenuModel(const std::string& id,
                           GetContextMenuModelCallback callback) override;
  void SortAppList(AppListSortOrder order) override {}
  ui::ImplicitAnimationObserver* GetAnimationObserver(
      ash::AppListViewState target_state) override;
  void ShowWallpaperContextMenu(const gfx::Point& onscreen_location,
                                ui::MenuSourceType source_type) override;
  bool CanProcessEventsOnApplistViews() override;
  bool ShouldDismissImmediately() override;
  int GetTargetYForAppListHide(aura::Window* root_window) override;
  ash::AssistantViewDelegate* GetAssistantViewDelegate() override;
  void OnSearchResultVisibilityChanged(const std::string& id,
                                       bool visibility) override;
  void NotifySearchResultsForLogging(
      const std::u16string& raw_query,
      const ash::SearchResultIdWithPositionIndices& results,
      int position_index) override;
  void MaybeIncreaseSuggestedContentInfoShownCount() override;
  bool IsAssistantAllowedAndEnabled() const override;
  bool ShouldShowSuggestedContentInfo() const override;
  void MarkSuggestedContentInfoDismissed() override;
  void OnStateTransitionAnimationCompleted(
      AppListViewState state,
      bool was_animation_interrupted) override;
  void OnViewStateChanged(AppListViewState state) override;
  void GetAppLaunchedMetricParams(
      AppLaunchedMetricParams* metric_params) override;
  gfx::Rect SnapBoundsToDisplayEdge(const gfx::Rect& bounds) override;
  int GetShelfSize() override;
  bool AppListTargetVisibility() const override;
  bool IsInTabletMode() override;
  AppListNotifier* GetNotifier() override;
  int AdjustAppListViewScrollOffset(int offset, ui::EventType type) override;
  void LoadIcon(const std::string& app_id) override {}

  // Do a bulk replacement of the items in the model.
  void ReplaceTestModel(int item_count);

  AppListTestModel* ReleaseTestModel() { return model_.release(); }
  AppListTestModel* GetTestModel() { return model_.get(); }

  SearchModel* ReleaseTestSearchModel() { return search_model_.release(); }

 private:
  void RecordAppLaunched(ash::AppListLaunchedFrom launched_from);

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  int dismiss_count_ = 0;
  int open_search_result_count_ = 0;
  int open_assistant_ui_count_ = 0;
  int next_profile_app_count_ = 0;
  int show_wallpaper_context_menu_count_ = 0;
  bool is_tablet_mode_ = false;
  bool should_show_suggested_content_info_ = false;
  std::map<size_t, int> open_search_result_counts_;
  std::unique_ptr<AppListTestModel> model_;
  std::unique_ptr<SearchModel> search_model_;
  std::vector<SkColor> wallpaper_prominent_colors_;
};

}  // namespace test
}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_TEST_VIEW_DELEGATE_H_
