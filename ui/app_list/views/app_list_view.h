// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APP_LIST_VIEW_H_
#define UI_APP_LIST_VIEWS_APP_LIST_VIEW_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#include "ui/app_list/speech_ui_model_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/widget/widget.h"

namespace base {
class FilePath;
}

namespace test {
class AppListViewTestApi;
}

namespace views {
class ImageView;
}

namespace app_list {
class ApplicationDragAndDropHost;
class AppListMainView;
class AppListModel;
class AppListViewDelegate;
class AppListViewObserver;
class HideViewAnimationObserver;
class PaginationModel;
class SearchBoxView;
class SpeechView;

// AppListView is the top-level view and controller of app list UI. It creates
// and hosts a AppsGridView and passes AppListModel to it for display.
class APP_LIST_EXPORT AppListView : public views::BubbleDialogDelegateView,
                                    public AppListViewDelegateObserver,
                                    public SpeechUIModelObserver {
 public:
  // Does not take ownership of |delegate|.
  explicit AppListView(AppListViewDelegate* delegate);
  ~AppListView() override;

  // Initializes the widget and use a given |anchor| plus an |anchor_offset| for
  // positioning.
  void InitAsBubbleAttachedToAnchor(gfx::NativeView parent,
                                    int initial_apps_page,
                                    views::View* anchor,
                                    const gfx::Vector2d& anchor_offset,
                                    views::BubbleBorder::Arrow arrow,
                                    bool border_accepts_events);

  // Initializes the widget and use a fixed |anchor_point_in_screen| for
  // positioning.
  void InitAsBubbleAtFixedLocation(gfx::NativeView parent,
                                   int initial_apps_page,
                                   const gfx::Point& anchor_point_in_screen,
                                   views::BubbleBorder::Arrow arrow,
                                   bool border_accepts_events);

  // Initializes the widget as a frameless window, not a bubble.
  void InitAsFramelessWindow(gfx::NativeView parent,
                             int initial_apps_page,
                             gfx::Rect bounds);

  void SetBubbleArrow(views::BubbleBorder::Arrow arrow);

  void SetAnchorPoint(const gfx::Point& anchor_point);

  // If |drag_and_drop_host| is not NULL it will be called upon drag and drop
  // operations outside the application list. This has to be called after
  // InitAsBubble was called since the app list object needs to exist so that
  // it can set the host.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Shows the UI when there are no pending icon loads. Otherwise, starts a
  // timer to show the UI when a maximum allowed wait time has expired.
  void ShowWhenReady();

  void CloseAppList();

  void UpdateBounds();

  // Enables/disables a semi-transparent overlay over the app list (good for
  // hiding the app list when a modal dialog is being shown).
  void SetAppListOverlayVisible(bool visible);

  // Returns true if the app list should be centered and in landscape mode.
  bool ShouldCenterWindow() const;

  views::Widget* search_box_widget() const { return search_box_widget_; }

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // WidgetDelegate overrides:
  bool ShouldHandleSystemCommands() const override;
  bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) override;

  // Overridden from AppListViewDelegateObserver:
  void OnProfilesChanged() override;
  void OnShutdown() override;

  void Prerender();

  void SetProfileByPath(const base::FilePath& profile_path);

  void AddObserver(AppListViewObserver* observer);
  void RemoveObserver(AppListViewObserver* observer);

  // Set a callback to be called the next time any app list paints.
  void SetNextPaintCallback(const base::Closure& callback);

#if defined(OS_WIN)
  HWND GetHWND() const;
#endif

  AppListMainView* app_list_main_view() { return app_list_main_view_; }

  // Gets the PaginationModel owned by this view's apps grid.
  PaginationModel* GetAppsPaginationModel();

  // Overridden from views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void Layout() override;
  void SchedulePaintInRect(const gfx::Rect& rect) override;

 private:
  friend class ::test::AppListViewTestApi;

  void InitContents(gfx::NativeView parent, int initial_apps_page);

  void InitChildWidgets();

  void InitAsBubbleInternal(gfx::NativeView parent,
                            int initial_apps_page,
                            views::BubbleBorder::Arrow arrow,
                            bool border_accepts_events,
                            const gfx::Vector2d& anchor_offset);

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
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // Overridden from SpeechUIModelObserver:
  void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) override;

  AppListViewDelegate* delegate_;  // Weak. Owned by AppListService.

  AppListMainView* app_list_main_view_;
  SpeechView* speech_view_;

  views::View* search_box_focus_host_;  // Owned by the views hierarchy.
  views::Widget* search_box_widget_;  // Owned by the app list's widget.
  SearchBoxView* search_box_view_;    // Owned by |search_box_widget_|.

  // A semi-transparent white overlay that covers the app list while dialogs are
  // open.
  views::View* overlay_view_;

  base::ObserverList<AppListViewObserver> observers_;
  std::unique_ptr<HideViewAnimationObserver> animation_observer_;

  // For UMA and testing. If non-null, triggered when the app list is painted.
  base::Closure next_paint_callback_;

  DISALLOW_COPY_AND_ASSIGN(AppListView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APP_LIST_VIEW_H_
