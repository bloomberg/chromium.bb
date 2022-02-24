// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_

#include <stddef.h>
#include <set>
#include <string>
#include <vector>

#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

namespace test {
class SearchResultListViewTest;
}

class AppListMainView;
class AppListViewDelegate;
class SearchResultPageDialogController;

// SearchResultListView displays SearchResultList with a list of
// SearchResultView.
class ASH_EXPORT SearchResultListView : public SearchResultContainerView {
 public:
  enum class SearchResultListType {
    // kUnified list view contains all search results with the display type
    // SearchResultDisplayType::kList. No category labels are shown. This should
    // be used when productivity launcher is disabled.
    kUnified,
    // kAnswerCard list view contains a single result that has an extremely high
    // chance of being exactly what the user is looking for.
    kAnswerCard,
    // kBestMatch list view contains the results that are the best match for the
    // current query. This category should be used when productivity launcher is
    // enabled. All search results will show up under this category until search
    // metadata is updated with the other category labels.
    kBestMatch,
    // kApps list view contains existing non-game ARC and PWA apps that are
    // installed and are relevant to but not the best match for the current
    // query.
    kApps,
    // kAppShortcuts list view contains shortcuts to actions for existing apps.
    kAppShortcuts,
    // kWeb list view contains links to relevant websites.
    kWeb,
    // kFiles list view contains relevant local and Google Drive files.
    kFiles,
    // kSettings list view contains relevant system settings.
    kSettings,
    // kHelp list view contains help articles from Showoff and Keyboard
    // Shortcuts.
    kHelp,
    // kPlayStore contains suggested apps from the playstore that are not
    // currently installed.
    kPlayStore,
    // kSearchAndAssistant contain suggestions from Search and Google Assistant.
    kSearchAndAssistant,
    kMaxValue = kSearchAndAssistant,
  };

  SearchResultListView(
      AppListMainView* main_view,
      AppListViewDelegate* view_delegate,
      SearchResultPageDialogController* dialog_controller,
      SearchResultView::SearchResultViewType search_result_view_type,
      bool animates_result_updates,
      absl::optional<size_t> productivity_launcher_index);

  SearchResultListView(const SearchResultListView&) = delete;
  SearchResultListView& operator=(const SearchResultListView&) = delete;
  ~SearchResultListView() override;

  // Updates the type of search results the list view shows.
  void SetListType(SearchResultListType list_type);

  void SearchResultActivated(SearchResultView* view,
                             int event_flags,
                             bool by_button_press);

  void SearchResultActionActivated(SearchResultView* view,
                                   SearchResultActionType action);

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Overridden from SearchResultContainerView:
  SearchResultView* GetResultViewAt(size_t index) override;
  absl::optional<ResultsAnimationInfo> ScheduleResultAnimations(
      const ResultsAnimationInfo& aggregate_animation_info) override;

  // Fades the view in and animates a vertical transform based on the view's
  // position in the overall search container view.
  void ShowViewWithAnimation(views::View* view, int position);

  AppListMainView* app_list_main_view() const { return main_view_; }

  // Gets all the SearchResultListTypes that should be used when categorical
  // search is enabled.
  static std::vector<SearchResultListType>
  GetAllListTypesForCategoricalSearch();

  // This should not be called on a disabled list view as list_type_ will be
  // reset.
  SearchResultListType list_type_for_test() { return list_type_.value(); }

  views::Label* title_label_for_test() { return title_label_; }

 private:
  friend class test::SearchResultListViewTest;

  // Overridden from SearchResultContainerView:
  int DoUpdate() override;

  // Overridden from views::View:
  void Layout() override;
  int GetHeightForWidth(int w) const override;
  void OnThemeChanged() override;

  // Logs the set of recommendations (impressions) that were shown to the user
  // after a period of time.
  void LogImpressions();

  // Returns search results specific to Assistant if any are available.
  std::vector<SearchResult*> GetAssistantResults();

  // Returns regular search results with Assistant search results appended.
  std::vector<SearchResult*> GetUnifiedSearchResults();

  // Fetches the category of results this view should show.
  SearchResult::Category GetSearchCategory();

  // Returns search results for the class's current list_type_.
  std::vector<SearchResult*> GetCategorizedSearchResults();

  // Updates the set of results shown in this result list.
  std::vector<SearchResult*> UpdateResultViews();

  // A filter that returns whether a search result should be shown in the
  // unified search result list.
  // `for_assistant_results` - When true only assistant results will be
  // included. When false assistant results are not included.
  bool FilterResultsForUnifiedList(bool for_assistant_results,
                                   const SearchResult& result) const;

  // A filter that returns whether a search result should be shown in the best
  // matches container.
  bool FilterBestMatches(const SearchResult& result) const;

  // A filter that returns whether a search results should be shown in the
  // categorized flavour of search result list.
  bool FilterSearchResultsByCategory(const SearchResult::Category& category,
                                     const SearchResult& result) const;

  AppListMainView* main_view_;          // Owned by views hierarchy.
  AppListViewDelegate* view_delegate_;  // Not owned.

  // Whether the result updates will be animated. If set,
  // `ScheduleResultAnimations()` is expected to be called whenever list of
  // results shown in the list changes.
  const bool animates_result_updates_;

  views::View* results_container_;

  std::vector<SearchResultView*> search_result_views_;  // Not owned.

  // The SearchResultListViewType dictates what kinds of results will be shown.
  absl::optional<SearchResultListType> list_type_ =
      SearchResultListType::kUnified;
  views::Label* title_label_ = nullptr;  // Owned by view hierarchy.
  // Used for logging impressions shown to users.
  base::OneShotTimer impression_timer_;

  // The search result list view's location in the
  // productivity_launcher_search_view_'s list of 'search_result_list_view_'.
  // Not set if productivity_launcher is disabled or if the position of the
  // category is const as for kBestMatch.
  const absl::optional<size_t> productivity_launcher_index_;

  // A search result list view may be disabled if there are fewer search result
  // categories than there are search result list views in the
  // 'productivity_launcher_search_view_'. A disabled view does not query the
  // search model.
  bool enabled_ = true;

  const SearchResultView::SearchResultViewType search_result_view_type_;

  // Set of results that have been removed from the result list using remove
  // search result actions. Used to filter those results out from the list of
  // shown results until results in the search model get refreshed.
  std::set<std::string> removed_results_;

  // The number of results shown by the list view.
  size_t num_results_ = 0;

  // The most recent container's index within the search UI - the index
  // indicates the number of result and title views that appear before this
  // container.
  int last_container_start_index_ = -1;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_
