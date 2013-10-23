// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APP_LIST_VIEW_H_
#define UI_APP_LIST_VIEWS_APP_LIST_VIEW_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/views/bubble/bubble_delegate.h"

namespace base {
class FilePath;
}

namespace views {
class Widget;
}

namespace app_list {
class ApplicationDragAndDropHost;
class AppListMainView;
class AppListModel;
class AppListViewDelegate;
class PaginationModel;
class SigninDelegate;
class SigninView;

// AppListView is the top-level view and controller of app list UI. It creates
// and hosts a AppsGridView and passes AppListModel to it for display.
class APP_LIST_EXPORT AppListView : public views::BubbleDelegateView,
                                    public AppListModelObserver {
 public:
  class Observer {
  public:
    virtual void OnActivationChanged(views::Widget* widget, bool active) = 0;
  };

  // Takes ownership of |delegate|.
  explicit AppListView(AppListViewDelegate* delegate);
  virtual ~AppListView();

  // Initializes the widget and use a given |anchor| plus an |anchor_offset| for
  // positioning.
  void InitAsBubbleAttachedToAnchor(gfx::NativeView parent,
                                    PaginationModel* pagination_model,
                                    views::View* anchor,
                                    const gfx::Vector2d& anchor_offset,
                                    views::BubbleBorder::Arrow arrow,
                                    bool border_accepts_events);

  // Initializes the widget and use a fixed |anchor_point_in_screen| for
  // positioning.
  void InitAsBubbleAtFixedLocation(gfx::NativeView parent,
                                   PaginationModel* pagination_model,
                                   const gfx::Point& anchor_point_in_screen,
                                   views::BubbleBorder::Arrow arrow,
                                   bool border_accepts_events);

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

  void Close();

  void UpdateBounds();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Paint(gfx::Canvas* canvas) OVERRIDE;

  // WidgetDelegate overrides:
  virtual bool ShouldHandleSystemCommands() const OVERRIDE;

  void Prerender();

  // Invoked when the sign-in status is changed to switch on/off sign-in view.
  void OnSigninStatusChanged();

  void SetProfileByPath(const base::FilePath& profile_path);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Set a callback to be called the next time any app list paints.
  static void SetNextPaintCallback(const base::Closure& callback);

#if defined(OS_WIN)
  HWND GetHWND() const;
#endif

  AppListModel* model() { return model_.get(); }

 private:
  void InitAsBubbleInternal(gfx::NativeView parent,
                            PaginationModel* pagination_model,
                            views::BubbleBorder::Arrow arrow,
                            bool border_accepts_events,
                            const gfx::Vector2d& anchor_offset);

  // Overridden from views::WidgetDelegateView:
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual bool WidgetHasHitTestMask() const OVERRIDE;
  virtual void GetWidgetHitTestMask(gfx::Path* mask) const OVERRIDE;

  // Overridden from views::View:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual void Layout() OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;
  virtual void OnWidgetVisibilityChanged(
      views::Widget* widget, bool visible) OVERRIDE;
  virtual void OnWidgetActivationChanged(
      views::Widget* widget, bool active) OVERRIDE;

  // Overridden from AppListModelObserver:
  virtual void OnAppListModelSigninStatusChanged() OVERRIDE;
  virtual void OnAppListModelUsersChanged() OVERRIDE;

  SigninDelegate* GetSigninDelegate();

  scoped_ptr<AppListModel> model_;
  scoped_ptr<AppListViewDelegate> delegate_;

  AppListMainView*  app_list_main_view_;
  SigninView* signin_view_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APP_LIST_VIEW_H_
