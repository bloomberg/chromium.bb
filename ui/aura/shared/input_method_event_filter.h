// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHARED_INPUT_METHOD_EVENT_FILTER_H_
#define UI_AURA_SHARED_INPUT_METHOD_EVENT_FILTER_H_

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
    : public EventFilter,
      public ui::internal::InputMethodDelegate {
 public:
  InputMethodEventFilter();
  virtual ~InputMethodEventFilter();

  void SetInputMethodPropertyInRootWindow(RootWindow* root_window);

 private:
  // Overridden from EventFilter:
  virtual bool PreHandleKeyEvent(Window* target, ui::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(Window* target,
                                   ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      Window* target,
      ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult PreHandleGestureEvent(
      Window* target,
      ui::GestureEvent* event) OVERRIDE;

  // Overridden from ui::internal::InputMethodDelegate.
  virtual void DispatchKeyEventPostIME(const base::NativeEvent& event) OVERRIDE;
  virtual void DispatchFabricatedKeyEventPostIME(ui::EventType type,
                                                 ui::KeyboardCode key_code,
                                                 int flags) OVERRIDE;

  scoped_ptr<ui::InputMethod> input_method_;

  // The target root window to which the key event translated by IME will
  // be dispatched.
  RootWindow* target_root_window_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodEventFilter);
};

}  // namespace shared
}  // namespace aura

#endif  // UI_AURA_IME_INPUT_METHOD_EVENT_FILTER_H_
