// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APP_LIST_VIEW_H_
#define UI_APP_LIST_VIEWS_APP_LIST_VIEW_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/speech_ui_model_observer.h"
#include "ui/display/display_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/widget/widget.h"

namespace display {
class Screen;
}

namespace app_list {
class ApplicationDragAndDropHost;
class AppListMainView;
class AppListModel;
class AppListViewDelegate;
class HideViewAnimationObserver;
class PaginationModel;
class SearchBoxView;
class SpeechView;

namespace test {
class AppListViewTestApi;
}

// AppListView is the top-level view and controller of app list UI. It creates
// and hosts a AppsGridView and passes AppListModel to it for display.
class APP_LIST_EXPORT AppListView : public views::BubbleDialogDelegateView,
                                    public SpeechUIModelObserver,
                                    public display::DisplayObserver {
 public:
  enum AppListState {
    // Closes |app_list_main_view_| and dismisses the delegate.
    CLOSED = 0,
    // The initial state for the app list when neither maximize or side shelf
    // modes are active. If set, the widget will peek over the shelf by
    // kPeekingAppListHeight DIPs.
    PEEKING,
    // Entered when text is entered into the search box from peeking mode.
    HALF,
    // Default app list state in maximize and side shelf modes. Entered from an
    // upward swipe from |PEEKING| or from clicking the chevron.
    FULLSCREEN_ALL_APPS,
    // Entered from an upward swipe from |HALF| or by entering text in the
    // search box from |FULLSCREEN_ALL_APPS|.
    FULLSCREEN_SEARCH,
  };

  // Does not take ownership of |delegate|.
  explicit AppListView(AppListViewDelegate* delegate);
  ~AppListView() override;

  // Initializes the widget as a bubble or fullscreen view depending on if the
  // fullscreen app list feature is set. |parent| and |initial_apps_page| are
  // used for both the bubble and fullscreen initialization. |is_maximize_mode|
  // is whether the display is in maximize mode and |is_side_shelf| is whether
  // the shelf is oriented on the side, and both are used for fullscreen
  // app list initialization.
  void Initialize(gfx::NativeView parent,
                  int initial_apps_page,
                  bool is_maximize_mode,
                  bool is_side_shelf);

  void SetBubbleArrow(views::BubbleBorder::Arrow arrow);

  void MaybeSetAnchorPoint(const gfx::Point& anchor_point);

  // If |drag_and_drop_host| is not NULL it will be called upon drag and drop
  // operations outside the application list. This has to be called after
  // Initialize was called since the app list object needs to exist so that
  // it can set the host.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Shows the UI when there are no pending icon loads. Otherwise, starts a
  // timer to show the UI when a maximum allowed wait time has expired.
  void ShowWhenReady();

  void UpdateBounds();

  // Enables/disables a semi-transparent overlay over the app list (good for
  // hiding the app list when a modal dialog is being shown).
  void SetAppListOverlayVisible(bool visible);

  views::Widget* search_box_widget() const { return search_box_widget_; }

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

  // WidgetDelegate overrides:
  bool ShouldHandleSystemCommands() const override;
  bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) override;

  AppListMainView* app_list_main_view() { return app_list_main_view_; }

  // Gets the PaginationModel owned by this view's apps grid.
  PaginationModel* GetAppsPaginationModel();

  // Overridden from views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void Layout() override;
  void SchedulePaintInRect(const gfx::Rect& rect) override;

  // Changes the app list state.
  void SetState(AppListState new_state);

  // Changes the app list state depending on the current |app_list_state_| and
  // whether the search box is empty.
  void SetStateFromSearchBoxView(bool search_box_is_empty);

  bool is_fullscreen() const {
    return app_list_state_ == FULLSCREEN_ALL_APPS ||
           app_list_state_ == FULLSCREEN_SEARCH;
  }

  AppListState app_list_state() const { return app_list_state_; }

  // Called when maximize mode starts and ends.
  void OnMaximizeModeChanged(bool started);

 private:
  friend class test::AppListViewTestApi;

  void InitContents(gfx::NativeView parent, int initial_apps_page);

  void InitChildWidgets();

  // Initializes the widget for fullscreen mode.
  void InitializeFullscreen(gfx::NativeView parent, int initial_apps_page);

  // Initializes the widget as a bubble.
  void InitializeBubble(gfx::NativeView parent, int initial_apps_page);

  // Initializes |initial_drag_point_|.
  void StartDrag(const gfx::Point& location);

  // Updates the bounds of the widget while maintaining the relative position
  // of the top of the widget and the gesture.
  void UpdateDrag(const gfx::Point& location);

  // Handles app list state transfers. If the drag was fast enough, ignore the
  // release position and snap to the next state.
  void EndDrag(const gfx::Point& location);

  // Overridden from views::BubbleDialogDelegateView:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  int GetDialogButtons() const override;

  // Overridden from views::WidgetDelegateView:
  views::View* GetInitiallyFocusedView() override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(gfx::Path* mask) const override;

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // Overridden from SpeechUIModelObserver:
  void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) override;

  // Overridden from DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  AppListViewDelegate* delegate_;  // Weak. Owned by AppListService.

  AppListMainView* app_list_main_view_;
  SpeechView* speech_view_;
  views::Widget* fullscreen_widget_ = nullptr;  // Owned by AppListView.

  views::View* search_box_focus_host_;  // Owned by the views hierarchy.
  views::Widget* search_box_widget_;    // Owned by the app list's widget.
  SearchBoxView* search_box_view_;      // Owned by |search_box_widget_|.
  // Owned by the app list's widget. Null if the fullscreen app list is not
  // enabled.
  views::View* app_list_background_shield_ = nullptr;
  // Whether maximize mode is active.
  bool is_maximize_mode_ = false;
  // Whether the shelf is oriented on the side.
  bool is_side_shelf_ = false;

  // The gap between the initial gesture event and the top of the window.
  gfx::Point initial_drag_point_;
  // The velocity of the gesture event.
  float last_fling_velocity_ = 0;
  // Whether the fullscreen app list feature is enabled.
  const bool is_fullscreen_app_list_enabled_;
  // The state of the app list, controlled via SetState().
  AppListState app_list_state_;

  // An observer that notifies AppListView when the display has changed.
  ScopedObserver<display::Screen, display::DisplayObserver> display_observer_;

  // A semi-transparent white overlay that covers the app list while dialogs are
  // open.
  views::View* overlay_view_;

  std::unique_ptr<HideViewAnimationObserver> animation_observer_;

  // For UMA and testing. If non-null, triggered when the app list is painted.
  base::Closure next_paint_callback_;

  DISALLOW_COPY_AND_ASSIGN(AppListView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APP_LIST_VIEW_H_
