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
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#include "ui/app_list/speech_ui_model_observer.h"
#include "ui/display/display_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}

namespace display {
class Screen;
}

namespace app_list {
class ApplicationDragAndDropHost;
class AppListMainView;
class AppListModel;
class AppListViewDelegate;
class AppsGridView;
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
                                    public display::DisplayObserver,
                                    public AppListViewDelegateObserver {
 public:
  // Number of the size of shelf. Used to determine the opacity of items in the
  // app list during dragging.
  static constexpr float kNumOfShelfSize = 2.0;

  // The opacity of the app list background.
  static constexpr float kAppListOpacity = 0.95;

  // The opacity of the app list background with blur.
  static constexpr float kAppListOpacityWithBlur = 0.7;

  // The preferred blend alpha with wallpaper color for background.
  static constexpr int kAppListColorDarkenAlpha = 178;

  // The defualt color of the app list background.
  static constexpr SkColor kDefaultBackgroundColor = SK_ColorBLACK;

  enum AppListState {
    // Closes |app_list_main_view_| and dismisses the delegate.
    CLOSED = 0,
    // The initial state for the app list when neither maximize or side shelf
    // modes are active. If set, the widget will peek over the shelf by
    // kPeekingAppListHeight DIPs.
    PEEKING = 1,
    // Entered when text is entered into the search box from peeking mode.
    HALF = 2,
    // Default app list state in maximize and side shelf modes. Entered from an
    // upward swipe from |PEEKING| or from clicking the chevron.
    FULLSCREEN_ALL_APPS = 3,
    // Entered from an upward swipe from |HALF| or by entering text in the
    // search box from |FULLSCREEN_ALL_APPS|.
    FULLSCREEN_SEARCH = 4,
  };

  // Does not take ownership of |delegate|.
  explicit AppListView(AppListViewDelegate* delegate);
  ~AppListView() override;

  // Prevents handling input events for the |window| in context of handling in
  // app list.
  static void ExcludeWindowFromEventHandling(aura::Window* window);

  // Initializes the widget as a bubble or fullscreen view depending on if the
  // fullscreen app list feature is set. |parent| and |initial_apps_page| are
  // used for both the bubble and fullscreen initialization. |is_tablet_mode|
  // is whether the display is in tablet mode and |is_side_shelf| is whether
  // the shelf is oriented on the side, and both are used for fullscreen
  // app list initialization.
  void Initialize(gfx::NativeView parent,
                  int initial_apps_page,
                  bool is_tablet_mode,
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

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

  // WidgetDelegate overrides:
  bool ShouldHandleSystemCommands() const override;
  bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) override;

  // Overridden from views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void Layout() override;
  void SchedulePaintInRect(const gfx::Rect& rect) override;

  // Overridden from ui::EventHandler:
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

  // Handles scroll events from various sources.
  bool HandleScroll(int offset, ui::EventType type);

  // Changes the app list state.
  void SetState(AppListState new_state);

  // Kicks off the proper animation for the state change. If an animation is
  // in progress it will be interrupted.
  void StartAnimationForState(AppListState new_state);

  // Starts the close animation.
  void StartCloseAnimation(base::TimeDelta animation_duration);

  // Changes the app list state depending on the current |app_list_state_| and
  // whether the search box is empty.
  void SetStateFromSearchBoxView(bool search_box_is_empty);

  // Updates y position and opacity of app list.
  void UpdateYPositionAndOpacity(int y_position_in_screen,
                                 float background_opacity);

  // Gets the PaginationModel owned by this view's apps grid.
  PaginationModel* GetAppsPaginationModel() const;

  // Gets the content bounds of the app info dialog of the app list in the
  // screen coordinates.
  gfx::Rect GetAppInfoDialogBounds() const;

  // Sets |is_in_drag_| and updates the visibility of app list items.
  void SetIsInDrag(bool is_in_drag);

  // Gets current work area bottom.
  int GetWorkAreaBottom();

  // Layouts the app list during dragging.
  void DraggingLayout();

  views::Widget* get_fullscreen_widget_for_test() const {
    return fullscreen_widget_;
  }

  AppListState app_list_state() const { return app_list_state_; }

  views::Widget* search_box_widget() const { return search_box_widget_; }

  SearchBoxView* search_box_view() const { return search_box_view_; }

  AppListMainView* app_list_main_view() const { return app_list_main_view_; }

  bool is_fullscreen() const {
    return app_list_state_ == FULLSCREEN_ALL_APPS ||
           app_list_state_ == FULLSCREEN_SEARCH;
  }

  bool is_tablet_mode() const { return is_tablet_mode_; }

  bool is_in_drag() const { return is_in_drag_; }

  int app_list_y_position_in_screen() const {
    return app_list_y_position_in_screen_;
  }

  void set_app_list_animation_duration_ms_for_testing(
      int app_list_animation_duration_ms) {
    app_list_animation_duration_ms_ = app_list_animation_duration_ms;
  }

  bool drag_started_from_peeking() const { return drag_started_from_peeking_; }

 private:
  // A widget observer that is responsible for keeping the AppListView state up
  // to date on closing.
  // TODO(newcomer): Merge this class into AppListView once the old app list
  // view code is removed.
  class FullscreenWidgetObserver;
  friend class test::AppListViewTestApi;

  void InitContents(gfx::NativeView parent, int initial_apps_page);

  void InitChildWidgets();

  // Initializes the widget for fullscreen mode.
  void InitializeFullscreen(gfx::NativeView parent, int initial_apps_page);

  // Initializes the widget as a bubble.
  void InitializeBubble(gfx::NativeView parent, int initial_apps_page);

  // Closes the AppListView when a click or tap event propogates to the
  // AppListView.
  void HandleClickOrTap(ui::LocatedEvent* event);

  // Initializes |initial_drag_point_|.
  void StartDrag(const gfx::Point& location);

  // Updates the bounds of the widget while maintaining the relative position
  // of the top of the widget and the gesture.
  void UpdateDrag(const gfx::Point& location);

  // Handles app list state transfers. If the drag was fast enough, ignore the
  // release position and snap to the next state.
  void EndDrag(const gfx::Point& location);

  // Records the state transition for UMA.
  void RecordStateTransitionForUma(AppListState new_state);

  // Gets the display nearest to the parent window.
  display::Display GetDisplayNearestView() const;

  // Gets the apps grid view owned by this view.
  AppsGridView* GetAppsGridView() const;

  // Gets the AppListStateTransitionSource for |app_list_state_| to
  // |target_state|. If we are not interested in recording a state transition
  // (ie. PEEKING->PEEKING) then return kMaxAppListStateTransition. If this is
  // modified, histograms will be affected.
  AppListStateTransitionSource GetAppListStateTransitionSource(
      AppListState target_state) const;

  // Overridden from views::BubbleDialogDelegateView:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  int GetDialogButtons() const override;

  // Overridden from views::WidgetDelegateView:
  views::View* GetInitiallyFocusedView() override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(gfx::Path* mask) const override;

  // Overridden from views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // Overridden from SpeechUIModelObserver:
  void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) override;

  // Overridden from DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Gets app list background opacity during dragging.
  float GetAppListBackgroundOpacityDuringDragging();

  // Overridden from AppListViewDelegateObserver:
  void OnWallpaperColorsChanged() override;

  void GetWallpaperProminentColors(std::vector<SkColor>* colors);
  void SetBackgroundShieldColor();

  AppListViewDelegate* delegate_;  // Weak. Owned by AppListService.
  AppListModel* const model_;      // Not Owned.

  AppListMainView* app_list_main_view_ = nullptr;
  SpeechView* speech_view_ = nullptr;
  views::Widget* fullscreen_widget_ = nullptr;  // Owned by AppListView.

  views::View* search_box_focus_host_ =
      nullptr;  // Owned by the views hierarchy.
  views::Widget* search_box_widget_ =
      nullptr;                                // Owned by the app list's widget.
  SearchBoxView* search_box_view_ = nullptr;  // Owned by |search_box_widget_|.
  // Owned by the app list's widget. Null if the fullscreen app list is not
  // enabled.
  views::View* app_list_background_shield_ = nullptr;
  // Whether tablet mode is active.
  bool is_tablet_mode_ = false;
  // Whether the shelf is oriented on the side.
  bool is_side_shelf_ = false;

  // True if the user is in the process of gesture-dragging on opened app list,
  // or dragging the app list from shelf.
  bool is_in_drag_ = false;

  // Y position of the app list in screen space coordinate during dragging.
  int app_list_y_position_in_screen_ = 0;

  // The opacity of app list background during dragging.
  float background_opacity_ = 0.f;

  // The location of initial gesture event in screen coordinates.
  gfx::Point initial_drag_point_;

  // The rectangle of initial widget's window in screen coordinates.
  gfx::Rect initial_window_bounds_;

  // The velocity of the gesture event.
  float last_fling_velocity_ = 0;
  // Whether the fullscreen app list feature is enabled.
  const bool is_fullscreen_app_list_enabled_;
  // Whether the background blur is enabled.
  const bool is_background_blur_enabled_;
  // The state of the app list, controlled via SetState().
  AppListState app_list_state_ = PEEKING;
  // An observer that notifies AppListView when the display has changed.
  ScopedObserver<display::Screen, display::DisplayObserver> display_observer_;

  // A widget observer that sets the AppListView state when the widget is
  // closed.
  std::unique_ptr<FullscreenWidgetObserver> widget_observer_;

  // A semi-transparent white overlay that covers the app list while dialogs
  // are open.
  views::View* overlay_view_ = nullptr;

  std::unique_ptr<HideViewAnimationObserver> animation_observer_;

  // For UMA and testing. If non-null, triggered when the app list is painted.
  base::Closure next_paint_callback_;

  // Animation duration in milliseconds.
  int app_list_animation_duration_ms_;

  // True if the dragging started from PEEKING state.
  bool drag_started_from_peeking_ = false;

  DISALLOW_COPY_AND_ASSIGN(AppListView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APP_LIST_VIEW_H_
