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
  Widget::CreateParams params(Widget::CreateParams::TYPE_WINDOW);
  params.delete_on_destroy = false;
  widget->SetCreateParams(params);
  widget->Init(NULL, gfx::Rect(10, 10, 200, 200));
  return widget;
}

NativeWidget* CreateNativeWidgetWithParent(NativeWidget* parent) {
  WidgetWin* widget = new WidgetWin;
  Widget::CreateParams params(Widget::CreateParams::TYPE_CONTROL);
  params.delete_on_destroy = false;
  params.child = false; // Implicitly set to true by ctor with TYPE_CONTROL.
  widget->SetCreateParams(params);
  widget->Init(parent ? parent->GetWidget()->GetNativeView() : NULL,
               gfx::Rect(10, 10, 200, 200));
  return widget;
}

}  // namespace internal
}  // namespace ui
