// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/ime/fake_input_method_context_ozone.h"

namespace ui {

FakeInputMethodContextOzone::FakeInputMethodContextOzone() {
}

FakeInputMethodContextOzone::~FakeInputMethodContextOzone() {
}

bool FakeInputMethodContextOzone::DispatchKeyEvent(
    const ui::KeyEvent& /* key_event */) {
  return false;
}

void FakeInputMethodContextOzone::Reset() {
}

void FakeInputMethodContextOzone::OnTextInputTypeChanged(
    ui::TextInputType /* text_input_type */) {
}

void FakeInputMethodContextOzone::OnCaretBoundsChanged(
    const gfx::Rect& /* caret_bounds */) {
}

}  // namespace ui
