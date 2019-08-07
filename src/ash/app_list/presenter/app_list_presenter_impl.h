// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_PRESENTER_APP_LIST_PRESENTER_IMPL_H_
#define ASH_APP_LIST_PRESENTER_APP_LIST_PRESENTER_IMPL_H_

#include <stdint.h>

#include <memory>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/presenter/app_list_presenter_delegate.h"
#include "ash/app_list/presenter/app_list_presenter_export.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
enum class AppListViewState;
}

namespace app_list {
class AppListView;

// Manages app list UI. Creates AppListView and schedules showing/hiding
// animation. While the UI is visible, it monitors things such as app list
// activation state to auto dismiss the UI.
class APP_LIST_PRESENTER_EXPORT AppListPresenterImpl
    : public aura::client::FocusChangeObserver,
      public ui::ImplicitAnimationObserver,
      public views::WidgetObserver,
      public ash::PaginationModelObserver {
 public:
  // Callback which fills out the passed settings object. Used by
  // UpdateYPositionAndOpacityForHomeLauncher so different callers can do
  // similar animations with different settings.
  using UpdateHomeLauncherAnimationSettingsCallback =
      base::RepeatingCallback<void(ui::ScopedLayerAnimationSettings* settings)>;

  // Used to dismiss the app list without animations.
  class ScopedDismissAnimationDisabler {
   public:
    explicit ScopedDismissAnimationDisabler(AppListPresenterImpl* presenter)
        : presenter_(presenter) {
      presenter_->dismiss_without_animation_ = true;
    }
    ~ScopedDismissAnimationDisabler() {
      presenter_->dismiss_without_animation_ = false;
    }

   private:
    AppListPresenterImpl* const presenter_;

    DISALLOW_COPY_AND_ASSIGN(ScopedDismissAnimationDisabler);
  };

  explicit AppListPresenterImpl(
      std::unique_ptr<AppListPresenterDelegate> delegate);
  ~AppListPresenterImpl() override;

  // Returns app list window or NULL if it is not visible.
  aura::Window* GetWindow();

  // Returns app list view if one exists, or NULL otherwise.
  AppListView* GetView() { return view_; }
  const AppListView* GetView() const { return view_; }

  // Show the app list window on the display with the given id. If
  // |event_time_stamp| is not 0, it means |Show()| was triggered by one of the
  // AppListShowSources: kSearchKey, kShelfButton, or kSwipeFromShelf.
  void Show(int64_t display_id, base::TimeTicks event_time_stamp);

  // Hide the open app list window. This may leave the view open but hidden.
  // If |event_time_stamp| is not 0, it means |Dismiss()| was triggered by
  // one AppListShowSource or focusing out side of the launcher.
  void Dismiss(base::TimeTicks event_time_stamp);

  // If app list has an opened folder, close it. Returns whether an opened
  // folder was closed.
  bool HandleCloseOpenFolder();

  // If app list has an open search box, close it. Returns whether an open
  // search box was closed.
  bool HandleCloseOpenSearchBox();

  // Show the app list if it is visible, hide it if it is hidden. If
  // |event_time_stamp| is not 0, it means |ToggleAppList()| was triggered by
  // one of the AppListShowSources: kSearchKey or kShelfButton.
  ash::ShelfAction ToggleAppList(int64_t display_id,
                                 app_list::AppListShowSource show_source,
                                 base::TimeTicks event_time_stamp);

  // Returns current visibility of the app list.
  bool IsVisible() const;

  // Returns target visibility. This may differ from IsVisible() if a visibility
  // transition is in progress.
  bool GetTargetVisibility() const;

  // Updates y position and opacity of app list.
  void UpdateYPositionAndOpacity(int y_position_in_screen,
                                 float background_opacity);

  // Ends the drag of app list from shelf.
  void EndDragFromShelf(ash::AppListViewState app_list_state);

  // Passes a MouseWheelEvent from the shelf to the AppListView.
  void ProcessMouseWheelOffset(const gfx::Vector2d& scroll_offset_vector);

  // Updates the y position and opacity of the full screen app list. The changes
  // are slightly different than UpdateYPositionAndOpacity. If |callback| is non
  // null the this will animate using the animation settings in |callback|.
  void UpdateYPositionAndOpacityForHomeLauncher(
      int y_position_in_screen,
      float opacity,
      UpdateHomeLauncherAnimationSettingsCallback callback);

  // Shows or hides the Assistant page.
  // |show| is true to show and false to hide.
  void ShowEmbeddedAssistantUI(bool show);

  // Returns current visibility of the Assistant page.
  bool IsShowingEmbeddedAssistantUI() const;

  // Show/hide the expand arrow view button.
  void SetExpandArrowViewVisibility(bool show);

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

  // Returns whether home launcher is currently shown.
  bool home_launcher_shown() const { return home_launcher_shown_; }

 private:
  // Sets the app list view and attempts to show it.
  void SetView(AppListView* view);

  // Forgets the view.
  void ResetView();

  // Starts dismiss animation.
  void ScheduleDismissAnimation();

  // Returns the id of the display containing the app list, if visible. If not
  // visible returns kInvalidDisplayId.
  int64_t GetDisplayId();

  void NotifyVisibilityChanged(bool visible, int64_t display_id);
  void NotifyTargetVisibilityChanged(bool visible);
  void NotifyHomeLauncherTargetPositionChanged(bool showing,
                                               int64_t display_id);
  void NotifyHomeLauncherAnimationComplete(bool shown, int64_t display_id);

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
  void TotalPagesChanged() override;
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionStarted() override;
  void TransitionChanged() override;
  void TransitionEnded() override;

  // Registers a callback that is run when the next frame successfully makes it
  // to the screen.
  void RequestPresentationTime(int64_t display_id,
                               base::TimeTicks event_time_stamp);

  // Responsible for laying out the app list UI.
  std::unique_ptr<AppListPresenterDelegate> delegate_;

  // Whether we should show or hide app list widget.
  bool is_visible_ = false;

  // The AppListView this class manages, owned by its widget.
  AppListView* view_ = nullptr;

  // The current page of the AppsGridView of |view_|. This is stored outside of
  // the view's PaginationModel, so that it persists when the view is destroyed.
  int current_apps_page_ = -1;

  // Cached bounds of |view_| for snapping back animation after over-scroll.
  gfx::Rect view_bounds_;

  // The last target visibility change.
  bool last_target_visible_ = false;

  // The last visibility change and its display id.
  bool last_visible_ = false;
  int64_t last_display_id_ = display::kInvalidDisplayId;

  // If true, dismiss the app list immediately.
  bool dismiss_without_animation_ = false;

  // Whether the home launcher is currently shown.
  bool home_launcher_shown_ = false;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterImpl);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_PRESENTER_APP_LIST_PRESENTER_IMPL_H_
