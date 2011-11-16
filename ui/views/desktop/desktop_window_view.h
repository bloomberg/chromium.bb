// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DESKTOP_DESKTOP_WINDOW_VIEW_H_
#define UI_VIEWS_DESKTOP_DESKTOP_WINDOW_VIEW_H_

#include "base/observer_list.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "views/view.h"
#include "views/widget/widget_delegate.h"

namespace ui {
class LayerAnimationSequence;
}  // namespace ui

namespace views {

class Widget;

namespace desktop {

class DesktopWindowView : public WidgetDelegateView,
                          public ui::LayerAnimationObserver {
 public:
  // Observers can listen to various events on the desktop.
  class Observer {
   public:
    virtual void OnDesktopBoundsChanged(const gfx::Rect& previous_bounds) = 0;

   protected:
    virtual ~Observer() {}
  };

  // The look and feel will be slightly different for different kinds of
  // desktop.
  enum DesktopType {
    DESKTOP_DEFAULT,
    DESKTOP_NETBOOK,
    DESKTOP_OTHER
  };

  static DesktopWindowView* desktop_window_view;

  explicit DesktopWindowView(DesktopType type);
  virtual ~DesktopWindowView();

  static void CreateDesktopWindow(DesktopType type);

  void CreateTestWindow(const string16& title,
                        SkColor color,
                        gfx::Rect initial_bounds,
                        bool rotate);

  DesktopType type() const { return type_; }

  // Add/remove observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer);

 private:
  // Overridden from View:
  virtual void Layout() OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void ViewHierarchyChanged(
      bool is_add, View* parent, View* child) OVERRIDE;

  // Overridden from WidgetDelegate:
  virtual Widget* GetWidget() OVERRIDE;
  virtual const Widget* GetWidget() const OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual SkBitmap GetWindowAppIcon() OVERRIDE;
  virtual SkBitmap GetWindowIcon() OVERRIDE;
  virtual bool ShouldShowWindowIcon() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  // Implementation of ui::LayerAnimationObserver:
  virtual void OnLayerAnimationEnded(
      const ui::LayerAnimationSequence* animation) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      const ui::LayerAnimationSequence* animation) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      const ui::LayerAnimationSequence* animation) OVERRIDE;

  ObserverList<Observer> observers_;
  DesktopType type_;
  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowView);
};

}  // namespace desktop
}  // namespace views

#endif  // UI_VIEWS_DESKTOP_DESKTOP_WINDOW_VIEW_H_
