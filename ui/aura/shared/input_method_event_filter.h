// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHARED_INPUT_METHOD_EVENT_FILTER_H_
#define UI_AURA_SHARED_INPUT_METHOD_EVENT_FILTER_H_
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
class RootWindow;

namespace shared {

// An event filter that forwards a KeyEvent to a system IME, and dispatches a
// TranslatedKeyEvent to the root window as needed.
class AURA_EXPORT InputMethodEventFilter
    : public aura::EventFilter,
      public ui::internal::InputMethodDelegate {
 public:
  explicit InputMethodEventFilter(RootWindow* root_window);
  virtual ~InputMethodEventFilter();

 private:
  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      aura::Window* target,
      aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

  // Overridden from ui::internal::InputMethodDelegate.
  virtual void DispatchKeyEventPostIME(const base::NativeEvent& event) OVERRIDE;
  virtual void DispatchFabricatedKeyEventPostIME(ui::EventType type,
                                                 ui::KeyboardCode key_code,
                                                 int flags) OVERRIDE;

  scoped_ptr<ui::InputMethod> input_method_;

  RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodEventFilter);
};

}  // namespace shared
}  // namespace aura

#endif  // UI_AURA_IME_INPUT_METHOD_EVENT_FILTER_H_
