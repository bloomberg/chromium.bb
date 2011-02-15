// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget_impl_test_util.h"

#include "views/view.h"
#include "views/widget/native_widget.h"
#include "views/widget/widget_impl.h"

namespace views {
namespace internal {

WidgetImpl* CreateWidgetImpl() {
  return CreateWidgetImplWithContents(new View);
}

WidgetImpl* CreateWidgetImplWithContents(View* contents_view) {
  WidgetImpl* widget = new WidgetImpl(contents_view);
  widget->set_delete_on_destroy(false);
  widget->InitWithNativeViewParent(NULL, gfx::Rect(10, 10, 200, 200));
  return widget;
}

WidgetImpl* CreateWidgetImplWithParent(WidgetImpl* parent) {
  WidgetImpl* widget = new WidgetImpl(new View);
  widget->set_delete_on_destroy(false);
  widget->InitWithNativeViewParent(parent->native_widget()->GetNativeView(),
                                   gfx::Rect(10, 10, 200, 200));
  return widget;
}

}  // namespace internal
}  // namespace views
