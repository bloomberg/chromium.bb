// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_NATIVE_WINDOW_WIN_H_
#define VIEWS_WINDOW_NATIVE_WINDOW_WIN_H_
#pragma once

#include "views/widget/native_widget_win.h"
#include "views/window/native_window.h"

namespace views {
namespace internal {
class NativeWindowDelegate;

// This is exposed only for testing
// Adjusts the value of |child_rect| if necessary to ensure that it is
// completely visible within |parent_rect|.
void EnsureRectIsVisibleInRect(const gfx::Rect& parent_rect,
                               gfx::Rect* child_rect,
                               int padding);

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
//
// NativeWindowWin
//
//  A NativeWindowWin is a NativeWidgetWin that encapsulates a window with a
//  frame. The frame may or may not be rendered by the operating system. The
//  window may or may not be top level.
//
////////////////////////////////////////////////////////////////////////////////
class NativeWindowWin : public NativeWidgetWin,
                        public NativeWindow {
 public:
  explicit NativeWindowWin(internal::NativeWindowDelegate* delegate);
  virtual ~NativeWindowWin();

  // Overridden from NativeWindow:
  virtual Window* GetWindow() OVERRIDE;
  virtual const Window* GetWindow() const OVERRIDE;

 protected:
  friend Window;

  // Overridden from NativeWindow:
  virtual NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const NativeWidget* AsNativeWidget() const OVERRIDE;

 private:
  // A delegate implementation that handles events received here.
  internal::NativeWindowDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeWindowWin);
};

}  // namespace views

#endif  // VIEWS_WINDOW_NATIVE_WINDOW_WIN_H_
