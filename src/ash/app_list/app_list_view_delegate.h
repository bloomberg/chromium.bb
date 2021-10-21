// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#define ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class ImplicitAnimationObserver;
class SimpleMenuModel;
}  // namespace ui

namespace ash {

class AppListModel;
class AppListNotifier;
enum class AppListViewState;
struct AppLaunchedMetricParams;
class SearchModel;

class ASH_PUBLIC_EXPORT AppListViewDelegate {
 public:
  virtual ~AppListViewDelegate() = default;

  // Gets the model associated with the view delegate. The model may be owned
  // by the delegate, or owned elsewhere (e.g. a profile keyed service).
  virtual AppListModel* GetModel() = 0;

  // Gets the search model associated with the view delegate. The model may be
  // owned by the delegate, or owned elsewhere (e.g. a profile keyed service).
  virtual SearchModel* GetSearchModel() = 0;

  // Returns the AppListNotifier instance. The notifier is owned by the
  // AppListClient, and may be nullptr if no client has been set for the
  // delegate.
  virtual AppListNotifier* GetNotifier() = 0;

  // Invoked to start a new Google Assistant session.
  virtual void StartAssistant() = 0;

  // Invoked to start a new search. This collects a list of search results
  // matching the raw query, which is an unhandled string typed into the search
  // box by the user.
  virtual void StartSearch(const std::u16string& raw_query) = 0;

  // Invoked to open the search result and log a click. If the result is
  // represented by a SuggestedChipView or is a zero state result,
  // |suggested_index| is the index of the view in the list of suggestions.
  // |launched_from| values must match the LaunchedFrom enum in
  // chrome/browser/ui/app_list/app_launch_event_logger.proto. |launch_type| is
  // either kAppSearchResult or kSearchResult and is used to determine which
  // histograms to log to.
  // |launch_as_default|: True if the result is launched as the default result
  // by user pressing ENTER key.
  virtual void OpenSearchResult(const std::string& result_id,
                                AppListSearchResultType result_type,
                                int event_flags,
                                AppListLaunchedFrom launched_from,
                                AppListLaunchType launch_type,
                                int suggestion_index,
                                bool launch_as_default) = 0;

  // Called to invoke a custom action on a result with |result_id|.
  // |action_index| corresponds to the index of an icon in
  // |result.action_icons()|.
  virtual void InvokeSearchResultAction(const std::string& result_id,
                                        int action_index) = 0;

  // Returns the context menu model for a ChromeSearchResult with |result_id|,
  // or nullptr if there is currently no menu for the result.
  // Note the returned menu model is owned by that result.
  using GetContextMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetContextMenuModelCallback callback) = 0;

  // Invoked when the app list is shown.
  virtual void ViewShown(int64_t display_id) = 0;

  // Invoked to dismiss app list. This may leave the view open but hidden from
  // the user.
  virtual void DismissAppList() = 0;

  // Invoked when the app list is closing.
  virtual void ViewClosing() = 0;

  // Gets the wallpaper prominent colors.
  virtual const std::vector<SkColor>& GetWallpaperProminentColors() = 0;

  // Activates (opens) the item.
  virtual void ActivateItem(const std::string& id,
                            int event_flags,
                            AppListLaunchedFrom launched_from) = 0;

  // Returns the context menu model for a ChromeAppListItem with |id|, or
  // nullptr if there is currently no menu for the item (e.g. during install).
  // Note the returned menu model is owned by that item.
  virtual void GetContextMenuModel(const std::string& id,
                                   GetContextMenuModelCallback callback) = 0;

  // Sorts app list items (including apps and folders) with the given order.
  virtual void SortAppList(AppListSortOrder order) = 0;

  // Returns an animation observer if the |target_state| is interesting to the
  // delegate.
  virtual ui::ImplicitAnimationObserver* GetAnimationObserver(
      AppListViewState target_state) = 0;

  // Show wallpaper context menu from the specified onscreen location.
  virtual void ShowWallpaperContextMenu(const gfx::Point& onscreen_location,
                                        ui::MenuSourceType source_type) = 0;

  // Returns True if the last event passing through app list was a key event.
  // This is stored in the controller and managed by the presenter.
  virtual bool KeyboardTraversalEngaged() = 0;

  // Checks if we are allowed to process events on the app list main view and
  // its descendants.
  virtual bool CanProcessEventsOnApplistViews() = 0;

  // Returns whether the AppListView should dismiss immediately.
  virtual bool ShouldDismissImmediately() = 0;

  // Gets the ideal y position for the close animation, which depends on
  // autohide state.
  virtual int GetTargetYForAppListHide(aura::Window* root_window) = 0;

  // Returns the AssistantViewDelegate.
  virtual AssistantViewDelegate* GetAssistantViewDelegate() = 0;

  // Called if a search result has its visibility updated and wants to
  // be notified (i.e. its notify_visibility_change() returns true).
  virtual void OnSearchResultVisibilityChanged(const std::string& id,
                                               bool visibility) = 0;

  // Called if a search result item got clicked, or a list of search result has
  // been shown to the user after a certain amount of time. |raw_query| is the
  // raw query that produced the results, |results| is a list of items that were
  // being shown to the users and their corresponding position indices of them
  // (see |SearchResultIdWithPositionIndex| for more details),
  // |position_index| is the position index of the clicked item (if no item got
  // clicked, |position_index| will be -1).
  virtual void NotifySearchResultsForLogging(
      const std::u16string& raw_query,
      const SearchResultIdWithPositionIndices& results,
      int position_index) = 0;

  // If the |prefs::kSuggestedContentInfoShownInLauncher| value is in the range
  // of allowed values, we will increment it.
  virtual void MaybeIncreaseSuggestedContentInfoShownCount() = 0;

  // Returns true if the Assistant feature is allowed and enabled.
  virtual bool IsAssistantAllowedAndEnabled() const = 0;

  // Returns true if the Suggested Content privacy info view should be shown.
  virtual bool ShouldShowSuggestedContentInfo() const = 0;

  // Called when close button in the Suggested Content privacy info view is
  // pressed to indicate not to show the view any more.
  virtual void MarkSuggestedContentInfoDismissed() = 0;

  // Called when the app list view state is updated.
  virtual void OnViewStateChanged(AppListViewState state) = 0;

  // Called when the app list state transition animation is completed.
  virtual void OnStateTransitionAnimationCompleted(
      AppListViewState state,
      bool was_animation_interrupted) = 0;

  // Fills the given AppLaunchedMetricParams with info known by the delegate.
  virtual void GetAppLaunchedMetricParams(
      AppLaunchedMetricParams* metric_params) = 0;

  // Adjusts the bounds by snapping it to the edge of the display in pixel
  // space. This prevents 1px gaps on displays with non-integer scale factors.
  virtual gfx::Rect SnapBoundsToDisplayEdge(const gfx::Rect& bounds) = 0;

  // Returns whether the app list is visible on the display. This checks that
  // the app list windows is open and not obstructed by another window.
  virtual bool AppListTargetVisibility() const = 0;

  // Gets the current shelf height (or width for side-shelf) from the
  // ShelfConfig.
  virtual int GetShelfSize() = 0;

  // Returns whether tablet mode is currently enabled.
  virtual bool IsInTabletMode() = 0;

  // Adjust scrolls that happen in the view. This needs to be delegated because
  // it depends on the active user's preferences.
  virtual int AdjustAppListViewScrollOffset(int offset, ui::EventType type) = 0;

  // Loads the icon of an app item identified by `app_id`.
  virtual void LoadIcon(const std::string& app_id) = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
