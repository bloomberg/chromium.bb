// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_CONTENTS_VIEW_H_
#define ASH_APP_LIST_VIEWS_CONTENTS_VIEW_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "base/compiler_specific.h"
#include "base/observer_list_types.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace gfx {
class Rect;
}

namespace ui {
class Layer;
class ScopedLayerAnimationSettings;
}  // namespace ui

namespace ash {

class AppListPage;
class AppListView;
class ApplicationDragAndDropHost;
class AppListMainView;
class AppsContainerView;
class AssistantPageView;
class ExpandArrowView;
class SearchBoxView;
class SearchResultPageView;

// A view to manage launcher pages within the Launcher (eg. start page, apps
// grid view, search results). There can be any number of launcher pages, only
// one of which can be active at a given time. ContentsView provides the user
// interface for switching between launcher pages, and animates the transition
// between them.
class ASH_EXPORT ContentsView : public views::View,
                                public PaginationModelObserver {
 public:
  // Used to SetActiveState without animations.
  class ScopedSetActiveStateAnimationDisabler {
   public:
    explicit ScopedSetActiveStateAnimationDisabler(ContentsView* contents_view)
        : contents_view_(contents_view) {
      contents_view_->set_active_state_without_animation_ = true;
    }

    ScopedSetActiveStateAnimationDisabler(
        const ScopedSetActiveStateAnimationDisabler&) = delete;
    ScopedSetActiveStateAnimationDisabler& operator=(
        const ScopedSetActiveStateAnimationDisabler&) = delete;

    ~ScopedSetActiveStateAnimationDisabler() {
      contents_view_->set_active_state_without_animation_ = false;
    }

   private:
    ContentsView* const contents_view_;
  };

  explicit ContentsView(AppListView* app_list_view);

  ContentsView(const ContentsView&) = delete;
  ContentsView& operator=(const ContentsView&) = delete;

  ~ContentsView() override;

  // Returns the search box top margin when app list view is in peeking/half
  // state, and showing the provided `page`.
  static int GetPeekingSearchBoxTopMarginOnPage(AppListState page);

  // Initialize the pages of the launcher. Should be called after
  // set_contents_switcher_view().
  void Init();

  // Resets the state of the view so it is ready to be shown.
  void ResetForShow();

  // The app list gets closed and drag and drop operations need to be cancelled.
  void CancelDrag();

  // If |drag_and_drop| is not nullptr it will be called upon drag and drop
  // operations outside the application list.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Called when the target state of AppListView changes.
  void OnAppListViewTargetStateChanged(AppListViewState target_state);

  // Called from AppListView when the tablet mode state changes.
  void OnTabletModeChanged(bool started);

  // Shows/hides the search results. Hiding the search results will cause the
  // app list to return to the page that was displayed before
  // ShowSearchResults(true) was invoked.
  void ShowSearchResults(bool show);
  bool IsShowingSearchResults() const;

  // Shows/hides the Assistant page. Hiding the Assistant page will
  // cause the app list to return to the page that was displayed before
  // ShowSearchResults(true) was invoked.
  void ShowEmbeddedAssistantUI(bool show);
  bool IsShowingEmbeddedAssistantUI() const;

  // Sets the active launcher page and animates the pages into place.
  void SetActiveState(AppListState state);
  void SetActiveState(AppListState state, bool animate);

  // The index of the currently active launcher page.
  int GetActivePageIndex() const;

  // The currently active state.
  AppListState GetActiveState() const;

  // True if |state| is the current active laucher page.
  bool IsStateActive(AppListState state) const;

  // Gets the index of a launcher page in |view_model_|, by State. Returns
  // -1 if there is no view for |state|.
  int GetPageIndexForState(AppListState state) const;

  // Gets the state of a launcher page in |view_model_|, by index. Returns
  // INVALID_STATE if there is no state for |index|.
  AppListState GetStateForPageIndex(int index) const;

  int NumLauncherPages() const;

  SearchResultPageView* search_result_page_view() const {
    return search_result_page_view_;
  }
  AppsContainerView* apps_container_view() const {
    return apps_container_view_;
  }
  AppListPage* GetPageView(int index) const;

  SearchBoxView* GetSearchBoxView() const;

  AppListMainView* GetAppListMainView() const;

  AppListView* app_list_view() const { return app_list_view_; }

  ExpandArrowView* expand_arrow_view() const { return expand_arrow_view_; }

  AppListViewState target_view_state() const { return target_view_state_; }

  // Returns the pagination model for the ContentsView.
  const PaginationModel& pagination_model() { return pagination_model_; }

  // Returns the search box bounds to use for a given app list (pagination)
  // state (in the current app list view state).
  gfx::Rect GetSearchBoxBounds(AppListState state) const;

  // Returns the search box bounds size to use for a given app list (pagination)
  // state (in the current app list view state).
  gfx::Size GetSearchBoxSize(AppListState state) const;

  // Returns the search box bounds size to use for a given app list (pagination)
  // state and app list view state.
  gfx::Rect GetSearchBoxBoundsForViewState(AppListState state,
                                           AppListViewState view_state) const;

  // Returns the expected search box bounds based on the app list transition
  // progress.
  gfx::Rect GetSearchBoxExpectedBoundsForProgress(AppListState state,
                                                  float progress) const;

  // Performs the 'back' action for the active page. Returns whether the action
  // was handled.
  bool Back();

  // Overridden from views::View:
  void Layout() override;
  const char* GetClassName() const override;

  // Overridden from PaginationModelObserver:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override;
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionStarted() override;
  void TransitionChanged() override;

  // Updates y position and opacity of the items in this view during dragging.
  void UpdateYPositionAndOpacity();

  // Starts animated transition to |target_view_state|.
  // Manages the child view opacity, and vertically translates search box and
  // app list pages to the bounds required for the new view state.
  void AnimateToViewState(AppListViewState target_view_state,
                          const base::TimeDelta& animation_duration);

  std::unique_ptr<ui::ScopedLayerAnimationSettings>
  CreateTransitionAnimationSettings(ui::Layer* layer) const;

  // Adjusts search box view size so it fits within the contents view margins
  // (when centered).
  gfx::Size AdjustSearchBoxSizeToFitMargins(
      const gfx::Size& preferred_size) const;

 private:
  // Sets the active launcher page.
  void SetActiveStateInternal(int page_index, bool animate);

  // Invoked when active view is changed.
  void ActivePageChanged();

  void InitializeSearchBoxAnimation(AppListState current_state,
                                    AppListState target_state);
  void UpdateSearchBoxAnimation(double progress,
                                AppListState current_state,
                                AppListState target_state);

  // Updates the expand arrow's behavior based on AppListViewState.
  void UpdateExpandArrowBehavior(AppListViewState target_state);

  // Updates the expand arrow visibility depending on the selected app list page
  // and the app list view state.
  // `target_state` - the target selected app list page.
  // `target_app_list_view_state` - the target app list view state.
  // `transition_duration` - the opacity transition duration. Should be set to
  //     zero if the opacity transition should not be animated.
  void UpdateExpandArrowOpacity(AppListState target_state,
                                AppListViewState target_app_list_state,
                                base::TimeDelta transition_duration);

  // Updates search box visibility based on the current state.
  void UpdateSearchBoxVisibility(AppListState current_state);

  // Adds |view| as a new page to the end of the list of launcher pages. The
  // view is inserted as a child of the ContentsView. The page is associated
  // with the name |state|. Returns a pointer to the instance of the new page.
  template <typename T>
  T* AddLauncherPage(std::unique_ptr<T> view, AppListState state) {
    auto* result = view.get();
    AddLauncherPageInternal(std::move(view), state);
    return result;
  }

  // Internal version of the above that does the actual work.
  void AddLauncherPageInternal(std::unique_ptr<AppListPage> view,
                               AppListState state);

  // Returns true if the |page| requires layout when transitioning from
  // |current_state| to |target_state|.
  bool ShouldLayoutPage(AppListPage* page,
                        AppListState current_state,
                        AppListState target_state) const;

  // Converts rect to widget without applying transform.
  gfx::Rect ConvertRectToWidgetWithoutTransform(const gfx::Rect& rect);

  // Returns the search box origin y coordinate to use for a given app list
  // (pagination) state and app list view state.
  // NOTE: The search box will be horizontally centered in the current content
  // bounds.
  int GetSearchBoxTopForViewState(AppListState state,
                                  AppListViewState view_state) const;

  // Sub-views of the ContentsView. All owned by the views hierarchy.
  AssistantPageView* assistant_page_view_ = nullptr;
  AppsContainerView* apps_container_view_ = nullptr;
  SearchResultPageView* search_result_page_view_ = nullptr;

  // The child page views. Owned by the views hierarchy.
  std::vector<AppListPage*> app_list_pages_;

  // Owned by the views hierarchy.
  AppListView* const app_list_view_;

  AppListViewState target_view_state_ = AppListViewState::kPeeking;

  // Owned by the views hierarchy.
  ExpandArrowView* expand_arrow_view_ = nullptr;

  // Maps State onto |view_model_| indices.
  std::map<AppListState, int> state_to_view_;

  // Maps |view_model_| indices onto State.
  std::map<int, AppListState> view_to_state_;

  // The page that was showing before ShowSearchResults(true) was invoked.
  int page_before_search_ = 0;

  // Manages the pagination for the launcher pages.
  PaginationModel pagination_model_{this};

  // If true, SetActiveState immediately.
  bool set_active_state_without_animation_ = false;

  // If set, the app list page that was used to determine the search box
  // placement when the contents view layout was last updated for app list view
  // state (either using UpdateYPositionAndOpacity() or AnimateToViewState()).
  // Used primarily to determine the initial search box position when animating
  // to a new app list view state.
  absl::optional<AppListState> target_page_for_last_view_state_update_;
  absl::optional<AppListViewState> last_target_view_state_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_CONTENTS_VIEW_H_
