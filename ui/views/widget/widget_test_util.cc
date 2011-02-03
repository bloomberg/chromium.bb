// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_test_util.h"

#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ui {
namespace internal {

Widget* CreateWidget() {
  return CreateWidgetWithContents(new View);
}

Widget* CreateWidgetWithContents(View* contents_view) {
  Widget* widget = new Widget(contents_view);
  widget->set_delete_on_destroy(false);
  widget->InitWithNativeViewParent(NULL, gfx::Rect(10, 10, 200, 200));
  return widget;
}

Widget* CreateWidgetWithParent(Widget* parent) {
  Widget* widget = new Widget(new View);
  widget->set_delete_on_destroy(false);
  widget->InitWithWidgetParent(parent, gfx::Rect(10, 10, 200, 200));
  return widget;
}

}  // namespace internal
}  // namespace ui
