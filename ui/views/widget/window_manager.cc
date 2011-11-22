// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/window_manager.h"

#include "base/compiler_specific.h"
#include "ui/views/events/event.h"
#include "ui/views/widget/widget.h"

namespace {

views::WindowManager* window_manager = NULL;

class NullWindowManager : public views::WindowManager {
 public:
  NullWindowManager() : mouse_capture_(NULL) {
  }

  virtual void StartMoveDrag(views::Widget* widget,
                             const gfx::Point& screen_point) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void StartResizeDrag(views::Widget* widget,
                               const gfx::Point& screen_point,
                               int hittest_code) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual bool SetMouseCapture(views::Widget* widget) OVERRIDE {
    if (mouse_capture_ == widget)
      return true;
    if (mouse_capture_)
      return false;
    mouse_capture_ = widget;
    return true;
  }

  virtual bool ReleaseMouseCapture(views::Widget* widget) OVERRIDE {
    if (widget && mouse_capture_ != widget)
      return false;
    mouse_capture_ = NULL;
    return true;
  }

  virtual bool HasMouseCapture(const views::Widget* widget) const OVERRIDE {
    return mouse_capture_ == widget;
  }

  virtual bool HandleKeyEvent(views::Widget* widget,
                              const views::KeyEvent& event) OVERRIDE {
    return false;
  }

  virtual bool HandleMouseEvent(views::Widget* widget,
                                const views::MouseEvent& event) OVERRIDE {
    if (mouse_capture_) {
      views::MouseEvent translated(event, widget->GetRootView(),
                                   mouse_capture_->GetRootView());
      mouse_capture_->OnMouseEvent(translated);
      return true;
    }
    return false;
  }

  virtual ui::TouchStatus HandleTouchEvent(views::Widget* widget,
      const views::TouchEvent& event) OVERRIDE {
    return ui::TOUCH_STATUS_UNKNOWN;
  }

  void Register(views::Widget* widget) OVERRIDE {}

 private:
  views::Widget* mouse_capture_;
};

}  // namespace

namespace views {

WindowManager::WindowManager() {
}

WindowManager::~WindowManager() {
}

// static
void WindowManager::Install(WindowManager* wm) {
  window_manager = wm;
}

// static
WindowManager* WindowManager::Get() {
  if (!window_manager)
    window_manager = new NullWindowManager();
  return window_manager;
}

}  // namespace views
