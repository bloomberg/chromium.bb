// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_DESTKOP_ROOT_WINDOW_EVENT_FILTER_H_
#define UI_AURA_DESKTOP_DESTKOP_ROOT_WINDOW_EVENT_FILTER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/event_filter.h"
#include "ui/base/ime/input_method_delegate.h"

namespace ui {
class InputMethod;
}

namespace aura {
class KeyEvent;
class GestureEvent;
class MouseEvent;
class RootWindow;
class TouchEvent;
class Window;

// An aura::EventFilter that takes care of events just for the desktop. It is a
// mix of the ash::internal::RootWindowEventFilter and
// ash::internal::InputMethodEventFilter.
class AURA_EXPORT DesktopRootWindowEventFilter
    : public aura::EventFilter,
      public ui::internal::InputMethodDelegate {
 public:
  DesktopRootWindowEventFilter(aura::RootWindow* root_window);
  virtual ~DesktopRootWindowEventFilter();

  // EventFilter:
  virtual bool PreHandleKeyEvent(Window* target,
                                 KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(Window* target,
                                   MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(Window* target,
                                              TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      Window* target,
      GestureEvent* event) OVERRIDE;

 private:
  // Overridden from ui::internal::InputMethodDelegate.
  virtual void DispatchKeyEventPostIME(const base::NativeEvent& event) OVERRIDE;
  virtual void DispatchFabricatedKeyEventPostIME(ui::EventType type,
                                                 ui::KeyboardCode key_code,
                                                 int flags) OVERRIDE;

  RootWindow* root_window_;

  scoped_ptr<ui::InputMethod> input_method_;
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_DESTKOP_ROOT_WINDOW_EVENT_FILTER_H_
