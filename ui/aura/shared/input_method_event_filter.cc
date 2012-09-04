// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/shared/input_method_event_filter.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"

namespace aura {
namespace shared {

////////////////////////////////////////////////////////////////////////////////
// InputMethodEventFilter, public:

InputMethodEventFilter::InputMethodEventFilter()
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          input_method_(ui::CreateInputMethod(this))),
      target_root_window_(NULL) {
  // TODO(yusukes): Check if the root window is currently focused and pass the
  // result to Init().
  input_method_->Init(true);
}

InputMethodEventFilter::~InputMethodEventFilter() {
}

void InputMethodEventFilter::SetInputMethodPropertyInRootWindow(
    aura::RootWindow* root_window) {
  root_window->SetProperty(aura::client::kRootWindowInputMethodKey,
                           input_method_.get());
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodEventFilter, EventFilter implementation:

bool InputMethodEventFilter::PreHandleKeyEvent(aura::Window* target,
                                               ui::KeyEvent* event) {
  const ui::EventType type = event->type();
  if (type == ui::ET_TRANSLATED_KEY_PRESS ||
      type == ui::ET_TRANSLATED_KEY_RELEASE) {
    // The |event| is already handled by this object, change the type of the
    // event to ui::ET_KEY_* and pass it to the next filter.
    static_cast<ui::TranslatedKeyEvent*>(event)->ConvertToKeyEvent();
    return false;
  } else {
    // If the focused window is changed, all requests to IME will be
    // discarded so it's safe to update the target_root_window_ here.
    target_root_window_ = target->GetRootWindow();
    DCHECK(target_root_window_);
    input_method_->DispatchKeyEvent(event->native_event());
    return true;
  }
}

bool InputMethodEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                 ui::MouseEvent* event) {
  return false;
}

ui::TouchStatus InputMethodEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    ui::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult InputMethodEventFilter::PreHandleGestureEvent(
    aura::Window* target,
    ui::GestureEvent* event) {
  return ui::ER_UNHANDLED;
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodEventFilter, ui::InputMethodDelegate implementation:

void InputMethodEventFilter::DispatchKeyEventPostIME(
    const base::NativeEvent& event) {
#if defined(OS_WIN)
  DCHECK(event.message != WM_CHAR);
#endif
  ui::TranslatedKeyEvent aura_event(event, false /* is_char */);
  target_root_window_->AsRootWindowHostDelegate()->OnHostKeyEvent(&aura_event);
}

void InputMethodEventFilter::DispatchFabricatedKeyEventPostIME(
    ui::EventType type,
    ui::KeyboardCode key_code,
    int flags) {
  ui::TranslatedKeyEvent aura_event(type == ui::ET_KEY_PRESSED, key_code,
                                    flags);
  target_root_window_->AsRootWindowHostDelegate()->OnHostKeyEvent(&aura_event);
}

}  // namespace shared
}  // namespace aura
