// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop/desktop_root_window_event_filter.h"

#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/event.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"

namespace aura {

namespace {

aura::Window* FindFocusableWindowFor(aura::Window* window) {
  while (window && !window->CanFocus())
    window = window->parent();
  return window;
}

}  // namespace

DesktopRootWindowEventFilter::DesktopRootWindowEventFilter(
    RootWindow* root_window)
    : root_window_(root_window),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          input_method_(ui::CreateInputMethod(this))) {
  // TODO(yusukes): Check if the root window is currently focused and pass the
  // result to Init().
  input_method_->Init(true);
  root_window_->SetProperty(
      aura::client::kRootWindowInputMethodKey,
      input_method_.get());
}

DesktopRootWindowEventFilter::~DesktopRootWindowEventFilter() {}

bool DesktopRootWindowEventFilter::PreHandleKeyEvent(Window* target,
                                                     KeyEvent* event) {
  const ui::EventType type = event->type();
  if (type == ui::ET_TRANSLATED_KEY_PRESS ||
      type == ui::ET_TRANSLATED_KEY_RELEASE) {
    // TODO(erg): what is this?
    // The |event| is already handled by this object, change the type of the
    // event to ui::ET_KEY_* and pass it to the next filter.
    static_cast<TranslatedKeyEvent*>(event)->ConvertToKeyEvent();
    return false;
  } else {
    input_method_->DispatchKeyEvent(event->native_event());
    return true;
  }
}

bool DesktopRootWindowEventFilter::PreHandleMouseEvent(
    Window* target,
    MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    // Get the active window?
    Window* active = aura::client::GetActivationClient(
        root_window_)->GetActiveWindow();

    if (active != target) {
      target->GetFocusManager()->SetFocusedWindow(
          FindFocusableWindowFor(target), event);
    }
  }

  return false;
}

ui::TouchStatus DesktopRootWindowEventFilter::PreHandleTouchEvent(
    Window* target,
    TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus DesktopRootWindowEventFilter::PreHandleGestureEvent(
    Window* target,
    GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopInputMehtodEventFilter, ui::InputMethodDelegate implementation:

void DesktopRootWindowEventFilter::DispatchKeyEventPostIME(
    const base::NativeEvent& event) {
#if defined(OS_WIN)
  DCHECK(event.message != WM_CHAR);
#endif
  TranslatedKeyEvent aura_event(event, false /* is_char */);
  root_window_->DispatchKeyEvent(&aura_event);
}

void DesktopRootWindowEventFilter::DispatchFabricatedKeyEventPostIME(
    ui::EventType type,
    ui::KeyboardCode key_code,
    int flags) {
  TranslatedKeyEvent aura_event(type == ui::ET_KEY_PRESSED, key_code, flags);
  root_window_->DispatchKeyEvent(&aura_event);
}

}  // namespace aura

