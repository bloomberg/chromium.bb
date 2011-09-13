// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_DESKTOP_DESKTOP_WINDOW_H_
#define VIEWS_DESKTOP_DESKTOP_WINDOW_H_

#include "views/view.h"
#include "views/widget/widget_delegate.h"

namespace views {
class NativeWidgetViews;
class Widget;

namespace desktop {

class DesktopWindowView : public WidgetDelegateView {
 public:
  // The look and feel will be slightly different for different kinds of
  // desktop.
  enum DesktopType {
    DESKTOP_DEFAULT,
    DESKTOP_NETBOOK,
    DESKTOP_OTHER
  };

  static DesktopWindowView* desktop_window_view;

  DesktopWindowView(DesktopType type);
  virtual ~DesktopWindowView();

  static void CreateDesktopWindow(DesktopType type);

  void CreateTestWindow(const std::wstring& title,
                        SkColor color,
                        gfx::Rect initial_bounds,
                        bool rotate);

  DesktopType type() const { return type_; }

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
  virtual std::wstring GetWindowTitle() const OVERRIDE;
  virtual SkBitmap GetWindowAppIcon() OVERRIDE;
  virtual SkBitmap GetWindowIcon() OVERRIDE;
  virtual bool ShouldShowWindowIcon() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  DesktopType type_;
  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowView);
};

}  // namespace desktop
}  // namespace views

#endif  // VIEWS_DESKTOP_DESKTOP_WINDOW_H_
