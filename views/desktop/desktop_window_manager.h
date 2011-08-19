// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_DESKTOP_DESKTOP_WINDOW_MANAGER_H_
#define VIEWS_DESKTOP_DESKTOP_WINDOW_MANAGER_H_

#include "base/compiler_specific.h"
#include "base/scoped_ptr.h"
#include "views/widget/window_manager.h"

namespace gfx {
class Point;
}

namespace views {
class Widget;

namespace desktop {
class WindowController;

// A tentative window manager for views destktop until we have *right*
// implementation based on aura/layer API. This is minimum
// implmenetation and complicated actio like moving transformed window
// doesn't work.  TODO(oshima): move active widget to WindowManager.
class DesktopWindowManager : public views::WindowManager {
 public:
  DesktopWindowManager(Widget* desktop);
  virtual ~DesktopWindowManager();

  // views::WindowManager implementations:
  virtual void StartMoveDrag(views::Widget* widget,
                             const gfx::Point& point) OVERRIDE;
  virtual void StartResizeDrag(views::Widget* widget,
                               const gfx::Point& point,
                               int hittest_code);
  virtual bool SetMouseCapture(views::Widget* widget) OVERRIDE;
  virtual bool ReleaseMouseCapture(views::Widget* widget) OVERRIDE;
  virtual bool HasMouseCapture(const views::Widget* widget) const OVERRIDE;
  virtual bool HandleMouseEvent(views::Widget* widget,
                                const views::MouseEvent& event) OVERRIDE;

 private:
  void SetMouseCapture();
  void ReleaseMouseCapture();
  bool HasMouseCapture() const;

  views::Widget* desktop_;
  views::Widget* mouse_capture_;
  scoped_ptr<WindowController> window_controller_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowManager);
};

// An behavioral interface for objects implements window resize/movement.
class WindowController {
 public:
  WindowController();
  virtual ~WindowController();
  virtual bool OnMouseEvent(const views::MouseEvent& event) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowController);
};

}  // namespace desktop
}  // namespace views

#endif  // VIEWS_DESKTOP_DESKTOP_WINDOW_MANAGER_H_
