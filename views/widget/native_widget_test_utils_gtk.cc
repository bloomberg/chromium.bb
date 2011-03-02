// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_test_utils.h"

#include "views/view.h"
#include "views/widget/widget_gtk.h"

namespace views {
namespace internal {

NativeWidget* CreateNativeWidget() {
  return CreateNativeWidgetWithContents(new View);
}

NativeWidget* CreateNativeWidgetWithContents(View* contents_view) {
  WidgetGtk* widget = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  widget->set_delete_on_destroy(false);
  widget->Init(NULL, gfx::Rect(10, 10, 200, 200));
  return widget;
}

NativeWidget* CreateNativeWidgetWithParent(NativeWidget* parent) {
  WidgetGtk* widget = new WidgetGtk(WidgetGtk::TYPE_CHILD);
  widget->set_delete_on_destroy(false);
  widget->Init(parent ? parent->GetWidget()->GetNativeView() : NULL,
               gfx::Rect(10, 10, 200, 200));
  return widget;
}

}  // namespace internal
}  // namespace ui
