// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_utils.h"

#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace views {

gfx::NativeWindow GetRootWindow(const Widget* widget) {
  gfx::NativeWindow window = widget->GetNativeWindow();
#if defined(USE_AURA)
  window = window->GetRootWindow();
#endif
  return window;
}

}  // namespace views
