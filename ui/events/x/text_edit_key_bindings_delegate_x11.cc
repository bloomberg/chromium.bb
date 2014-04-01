// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/text_edit_key_bindings_delegate_x11.h"

namespace ui {

namespace {
// Optional delegate. Unowned pointer.
TextEditKeyBindingsDelegateX11* text_edit_keybinding_delegate_ = 0;
}

void SetTextEditKeyBindingsDelegate(TextEditKeyBindingsDelegateX11* delegate) {
  text_edit_keybinding_delegate_ = delegate;
}

TextEditKeyBindingsDelegateX11* GetTextEditKeyBindingsDelegate() {
  return text_edit_keybinding_delegate_;
}

}  // namespace ui
