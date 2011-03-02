// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_test_utils.h"

#include "views/view.h"
#include "views/widget/widget_win.h"

namespace views {
namespace internal {

NativeWidget* CreateNativeWidget() {
  return CreateNativeWidgetWithContents(new View);
}

NativeWidget* CreateNativeWidgetWithContents(View* contents_view) {
  WidgetWin* widget = new WidgetWin;
  widget->set_delete_on_destroy(false);
  widget->Init(NULL, gfx::Rect(10, 10, 200, 200));
  return widget;
}

NativeWidget* CreateNativeWidgetWithParent(NativeWidget* parent) {
  WidgetWin* widget = new WidgetWin;
  widget->set_delete_on_destroy(false);
  widget->Init(parent ? parent->GetWidget()->GetNativeView() : NULL,
               gfx::Rect(10, 10, 200, 200));
  return widget;
}

}  // namespace internal
}  // namespace ui
