// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/hwnd_util.h"

#include "ui/aura/root_window.h"
#include "ui/views/widget/widget.h"

namespace views {

HWND HWNDForView(View* view) {
  return view->GetWidget() ? HWNDForWidget(view->GetWidget()) : NULL;
}

HWND HWNDForWidget(Widget* widget) {
  return HWNDForNativeWindow(widget->GetNativeWindow());
}

HWND HWNDForNativeWindow(gfx::NativeWindow window) {
  return window && window->GetRootWindow() ?
      window->GetRootWindow()->GetAcceleratedWidget() : NULL;
}

}
