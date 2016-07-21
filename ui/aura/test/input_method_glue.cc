// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/input_method_glue.h"

#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/event.h"

namespace aura {

InputMethodGlue::InputMethodGlue(aura::WindowTreeHost* host) : host_(host) {
  aura::Env::GetInstance()->AddPreTargetHandler(this);
  host_->GetInputMethod()->SetDelegate(this);
}

InputMethodGlue::~InputMethodGlue() {
  host_->GetInputMethod()->SetDelegate(host_);
  aura::Env::GetInstance()->RemovePreTargetHandler(this);
}

void InputMethodGlue::OnKeyEvent(ui::KeyEvent* key) {
  if (processing_ime_event_)
    return;
  host_->GetInputMethod()->DispatchKeyEvent(key);
  key->StopPropagation();
}

ui::EventDispatchDetails InputMethodGlue::DispatchKeyEventPostIME(
    ui::KeyEvent* key) {
  base::AutoReset<bool> reset(&processing_ime_event_, true);
  return host_->DispatchKeyEventPostIME(key);
}

}  // namespace aura
