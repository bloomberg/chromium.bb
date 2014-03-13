// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/input_method_event_filter.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"

namespace wm {

////////////////////////////////////////////////////////////////////////////////
// InputMethodEventFilter, public:

InputMethodEventFilter::InputMethodEventFilter(gfx::AcceleratedWidget widget)
    : input_method_(ui::CreateInputMethod(this, widget)),
      target_dispatcher_(NULL) {
  // TODO(yusukes): Check if the root window is currently focused and pass the
  // result to Init().
  input_method_->Init(true);
}

InputMethodEventFilter::~InputMethodEventFilter() {
}

void InputMethodEventFilter::SetInputMethodPropertyInRootWindow(
    aura::Window* root_window) {
  root_window->SetProperty(aura::client::kRootWindowInputMethodKey,
                           input_method_.get());
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodEventFilter, EventFilter implementation:

void InputMethodEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  const ui::EventType type = event->type();
  if (type == ui::ET_TRANSLATED_KEY_PRESS ||
      type == ui::ET_TRANSLATED_KEY_RELEASE) {
    // The |event| is already handled by this object, change the type of the
    // event to ui::ET_KEY_* and pass it to the next filter.
    static_cast<ui::TranslatedKeyEvent*>(event)->ConvertToKeyEvent();
  } else {
    // If the focused window is changed, all requests to IME will be
    // discarded so it's safe to update the target_dispatcher_ here.
    aura::Window* target = static_cast<aura::Window*>(event->target());
    target_dispatcher_ = target->GetRootWindow()->GetHost()->event_processor();
    DCHECK(target_dispatcher_);
    if (input_method_->DispatchKeyEvent(*event))
      event->StopPropagation();
  }
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodEventFilter, ui::InputMethodDelegate implementation:

bool InputMethodEventFilter::DispatchKeyEventPostIME(
    const ui::KeyEvent& event) {
#if defined(OS_WIN)
  if (DCHECK_IS_ON() && event.HasNativeEvent())
    DCHECK_NE(event.native_event().message, static_cast<UINT>(WM_CHAR));
#endif
  ui::TranslatedKeyEvent aura_event(event);
  ui::EventDispatchDetails details =
      target_dispatcher_->OnEventFromSource(&aura_event);
  CHECK(!details.dispatcher_destroyed);
  return aura_event.handled();
}

}  // namespace wm
