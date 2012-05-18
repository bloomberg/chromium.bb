// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_NATIVE_WIDGET_HELPER_AURA_H_
#define UI_VIEWS_WIDGET_NATIVE_WIDGET_HELPER_AURA_H_
#pragma once

#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"

namespace views {

// A special delegate that encapsulates all logic for use of NativeWidgetAura
// on the desktop.
class VIEWS_EXPORT NativeWidgetHelperAura {
 public:
  virtual ~NativeWidgetHelperAura() {}

  // Called at the start of InitNativeWidget; determines whether we should
  // set up a root_window_ for this widget/window pair.
  virtual void PreInitialize(aura::Window* window,
                             const Widget::InitParams& params) = 0;

  // Called at the end of InitNativeWidget; i.e. after the NativeWidgetAura's
  // aura::Window has been initialized.
  virtual void PostInitialize() = 0;

  // Passes through a message to show the RootWindow, if it exists.
  virtual void ShowRootWindow() = 0;

  // If we own a RootWindow, return it. Otherwise NULL.
  virtual aura::RootWindow* GetRootWindow() = 0;

  // If this NativeWidgetAura has its own RootWindow, sets the position at the
  // |root_window_|, and returns modified bounds to set the origin to
  // zero. Otherwise, pass through in_bounds.
  virtual gfx::Rect ModifyAndSetBounds(const gfx::Rect& bounds) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_NATIVE_WIDGET_HELPER_AURA_H_
