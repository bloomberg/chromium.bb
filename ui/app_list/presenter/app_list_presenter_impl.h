// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_PRESENTER_APP_LIST_PRESENTER_IMPL_H_
#define UI_APP_LIST_PRESENTER_APP_LIST_PRESENTER_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/app_list/pagination_model_observer.h"
#include "ui/app_list/presenter/app_list_presenter.mojom.h"
#include "ui/app_list/presenter/app_list_presenter_delegate.h"
#include "ui/app_list/presenter/app_list_presenter_export.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class AnimationMetricsReporter;
}

namespace app_list {
class AppListView;
class AppListViewDelegate;
class AppListPresenterDelegateFactory;

namespace test {
class AppListPresenterImplTestApi;
}

class AppListViewDelegate;

// Manages app list UI. Creates AppListView and schedules showing/hiding
// animation. While the UI is visible, it monitors things such as app list
// activation state to auto dismiss the UI. Delegates the responsibility
// for laying out the app list UI to ash::AppListLayoutDelegate.
class APP_LIST_PRESENTER_EXPORT AppListPresenterImpl
    : public aura::client::FocusChangeObserver,
      public aura::WindowObserver,
      public ui::ImplicitAnimationObserver,
      public views::WidgetObserver,
      public PaginationModelObserver {
 public:
  explicit AppListPresenterImpl(
      std::unique_ptr<AppListPresenterDelegateFactory> factory);
  ~AppListPresenterImpl() override;

  // Returns app list window or NULL if it is not visible.
  aura::Window* GetWindow();

  // Returns app list view if one exists, or NULL otherwise.
  AppListView* GetView() { return view_; }

  // Show the app list window on the display with the given id.
  void Show(int64_t display_id);

  // Hide the open app list window. This may leave the view open but hidden.
  void Dismiss();

  // Show the app list if it is visible, hide it if it is hidden.
  void ToggleAppList(int64_t display_id);

  // Returns current visibility of the app list.
  bool IsVisible() const;

  // Returns target visibility. This may differ from IsVisible() if a visibility
  // transition is in progress.
  bool GetTargetVisibility() const;

  // Sets the app list interface pointer; used to report visibility changes.
  void SetAppList(mojom::AppListPtr app_list);

  // Updates y position and opacity of app list.
  void UpdateYPositionAndOpacity(int y_position_in_screen,
                                 float background_opacity);

  // Ends the drag of app list from shelf.
  void EndDragFromShelf(mojom::AppListState app_list_state);

  // Passes a MouseWheelEvent from the shelf to the AppListView.
  void ProcessMouseWheelOffset(int y_scroll_offset);

 private:
  friend class test::AppListPresenterImplTestApi;

  // Sets the app list view and attempts to show it.
  void SetView(AppListView* view);

  // Forgets the view.
  void ResetView();

  // Starts show/hide animation.
  void ScheduleAnimation();

  // Returns the id of the display containing the app list, if visible. If not
  // visible returns kInvalidDisplayId.
  int64_t GetDisplayId();

  // aura::client::FocusChangeObserver overrides:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // aura::WindowObserver overrides:
  void OnWindowBoundsChanged(aura::Window* root,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

  // views::WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // PaginationModelObserver overrides:
  void TotalPagesChanged() override;
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionStarted() override;
  void TransitionChanged() override;
  void TransitionEnded() override;

  // The factory for the presenter's delegate.
  std::unique_ptr<AppListPresenterDelegateFactory> factory_;

  // Responsible for laying out the app list UI.
  std::unique_ptr<AppListPresenterDelegate> presenter_delegate_;

  // Whether we should show or hide app list widget.
  bool is_visible_ = false;

  // The AppListView this class manages, owned by its widget.
  AppListView* view_ = nullptr;

  // The current page of the AppsGridView of |view_|. This is stored outside of
  // the view's PaginationModel, so that it persists when the view is destroyed.
  int current_apps_page_ = -1;

  // Cached bounds of |view_| for snapping back animation after over-scroll.
  gfx::Rect view_bounds_;

  // Whether should schedule snap back animation.
  bool should_snap_back_ = false;

  // The app list interface pointer; used for reporting visibility changes.
  mojom::AppListPtr app_list_;

  // Metric reporter for state change animations.
  const std::unique_ptr<ui::AnimationMetricsReporter>
      state_animation_metrics_reporter_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterImpl);
};

}  // namespace app_list

#endif  // UI_APP_LIST_PRESENTER_APP_LIST_PRESENTER_IMPL_H_
