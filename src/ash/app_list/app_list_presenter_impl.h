// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_PRESENTER_IMPL_H_
#define ASH_APP_LIST_APP_LIST_PRESENTER_IMPL_H_

#include <stdint.h>

#include <memory>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
class AppListControllerImpl;
class AppListPresenterEventFilter;
class AppListView;
enum class AppListViewState;

// Manages app list UI. Creates AppListView and schedules showing/hiding
// animation. While the UI is visible, it monitors things such as app list
// activation state and mouse/touch events to dismiss the UI. Updates the shelf
// launcher icon state.
class ASH_EXPORT AppListPresenterImpl
    : public PaginationModelObserver,
      public aura::client::FocusChangeObserver,
      public ui::ImplicitAnimationObserver,
      public views::WidgetObserver,
      public display::DisplayObserver,
      public ShelfObserver {
 public:
  static constexpr std::array<int, 7> kIdsOfContainersThatWontHideAppList = {
      kShellWindowId_AppListContainer,
      kShellWindowId_HomeScreenContainer,
      kShellWindowId_MenuContainer,
      kShellWindowId_PowerMenuContainer,
      kShellWindowId_SettingBubbleContainer,
      kShellWindowId_ShelfBubbleContainer,
      kShellWindowId_ShelfContainer};

  // Callback which fills out the passed settings object. Used by
  // UpdateYPositionAndOpacityForHomeLauncher so different callers can do
  // similar animations with different settings.
  using UpdateHomeLauncherAnimationSettingsCallback =
      base::RepeatingCallback<void(ui::ScopedLayerAnimationSettings* settings)>;

  // |controller| must outlive |this|.
  explicit AppListPresenterImpl(AppListControllerImpl* controller);
  ~AppListPresenterImpl() override;

  // Returns app list window or nullptr if it is not visible.
  aura::Window* GetWindow() const;

  // Returns app list view if one exists, or nullptr otherwise.
  AppListView* GetView() { return view_; }
  const AppListView* GetView() const { return view_; }

  // Show the app list window on the display with the given id. If
  // |event_time_stamp| is not 0, it means |Show()| was triggered by one of the
  // AppListShowSources: kSearchKey, kShelfButton, or kSwipeFromShelf.
  void Show(AppListViewState preferred_state,
            int64_t display_id,
            base::TimeTicks event_time_stamp,
            absl::optional<AppListShowSource> show_source);

  // Hide the open app list window. This may leave the view open but hidden.
  // If |event_time_stamp| is not 0, it means |Dismiss()| was triggered by
  // one AppListShowSource or focusing out side of the launcher.
  void Dismiss(base::TimeTicks event_time_stamp);

  // Sets the app list view visibility (without updating the app list window
  // visibility). No-op if the app list view does not exist.
  void SetViewVisibility(bool visible);

  // If app list has an opened folder, close it. Returns whether an opened
  // folder was closed.
  bool HandleCloseOpenFolder();

  // Show the app list if it is visible, hide it if it is hidden. If
  // |event_time_stamp| is not 0, it means |ToggleAppList()| was triggered by
  // one of the AppListShowSources: kSearchKey or kShelfButton.
  ShelfAction ToggleAppList(int64_t display_id,
                            AppListShowSource show_source,
                            base::TimeTicks event_time_stamp);

  // Returns current visibility of the app list. Deprecated, use
  // |IsAtLeastPartiallyVisible| instead.
  bool IsVisibleDeprecated() const;

  // Returns whether the app list is visible. This will only return false if
  // the app list is entirely occluded.
  bool IsAtLeastPartiallyVisible() const;

  // Returns target visibility. This may differ from IsVisible() if a visibility
  // transition is in progress.
  bool GetTargetVisibility() const;

  // Updates y position and opacity of app list.
  void UpdateYPositionAndOpacity(float y_position_in_screen,
                                 float background_opacity);

  // Ends the drag of app list from shelf.
  void EndDragFromShelf(AppListViewState app_list_state);

  // Passes data from a Scroll event from the shelf to the
  // AppListView.
  void ProcessScrollOffset(const gfx::Point& location,
                           const gfx::Vector2d& scroll_offset_vector);

  // Passes data from a MouseWheelEvent event from the shelf to the
  // AppListView.
  void ProcessMouseWheelOffset(const gfx::Point& location,
                               const gfx::Vector2d& scroll_offset_vector);

  // Scales the home launcher view maintaining the view center point, and
  // updates its opacity. If |callback| is non-null, the update should be
  // animated, and the |callback| should be called with the animation settings.
  // |transition| - The tablet mode animation type. Used to report animation
  // metrics if the home launcher change is animated. Should be set only if
  // |callback| is non-null. If not set, the animation smoothness metrics will
  // not be reported.
  void UpdateScaleAndOpacityForHomeLauncher(
      float scale,
      float opacity,
      absl::optional<TabletModeAnimationTransition> transition,
      UpdateHomeLauncherAnimationSettingsCallback callback);

  // Shows or hides the Assistant page.
  // |show| is true to show and false to hide.
  void ShowEmbeddedAssistantUI(bool show);

  // Returns current visibility of the Assistant page.
  bool IsShowingEmbeddedAssistantUI() const;

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

 private:
  // Sets the app list view and attempts to show it.
  void SetView(AppListView* view);

  // Forgets the view.
  void ResetView();

  // Returns the id of the display containing the app list, if visible. If not
  // visible returns kInvalidDisplayId.
  int64_t GetDisplayId() const;

  void OnVisibilityChanged(bool visible, int64_t display_id);
  void OnVisibilityWillChange(bool visible, int64_t display_id);

  // Called when the widget is hidden or destroyed.
  void OnClosed();

  // aura::client::FocusChangeObserver overrides:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

  // views::WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // PaginationModelObserver overrides:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override;
  void SelectedPageChanged(int old_selected, int new_selected) override;

  // DisplayObserver overrides:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ShelfObserver overrides:
  void OnBackgroundTypeChanged(ShelfBackgroundType background_type,
                               AnimationChangeType change_type) override;

  // Registers a callback that is run when the next frame successfully makes it
  // to the screen.
  void RequestPresentationTime(int64_t display_id,
                               base::TimeTicks event_time_stamp);

  // Snaps the app list window bounds to fit the screen size. (See
  // https://crbug.com/884889).
  void SnapAppListBoundsToDisplayEdge();

  // Owns |this|.
  AppListControllerImpl* const controller_;

  // Closes the app list when the user clicks outside its bounds.
  std::unique_ptr<AppListPresenterEventFilter> event_filter_;

  // An observer that notifies AppListView when the display has changed.
  display::ScopedDisplayObserver display_observer_{this};

  // An observer that notifies AppListView when the shelf state has changed.
  base::ScopedMultiSourceObservation<Shelf, ShelfObserver> shelf_observation_{
      this};

  // The target visibility of the AppListView, true if the target visibility is
  // shown.
  bool is_target_visibility_show_ = false;

  // The AppListView this class manages, owned by its widget.
  AppListView* view_ = nullptr;

  // The current page of the AppsGridView of |view_|. This is stored outside of
  // the view's PaginationModel, so that it persists when the view is destroyed.
  int current_apps_page_ = -1;

  // Cached bounds of |view_| for snapping back animation after over-scroll.
  gfx::Rect view_bounds_;

  // Data we need to store for metrics.
  absl::optional<base::Time> last_open_time_;
  absl::optional<AppListShowSource> last_open_source_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterImpl);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_PRESENTER_IMPL_H_
