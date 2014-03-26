// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/cursor_delegate.h"

#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(wm::CursorDelegate*);

namespace wm {

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(CursorDelegate*, kCursorDelegateKey, NULL);

void SetCursorDelegate(aura::Window* window, CursorDelegate* delegate) {
  window->SetProperty(kCursorDelegateKey, delegate);
}

CursorDelegate* GetCursorDelegate(aura::Window* window) {
  return window ? window->GetProperty(kCursorDelegateKey) : NULL;
}

}  // namespace wm
