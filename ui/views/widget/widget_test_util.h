// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_TEST_UTIL_H_
#define UI_VIEWS_WIDGET_WIDGET_TEST_UTIL_H_
#pragma once

namespace ui {
class View;
class Widget;
namespace internal {

// Create dummy Widgets for use in testing.
Widget* CreateWidget();
Widget* CreateWidgetWithContents(View* contents_view);
Widget* CreateWidgetWithParent(Widget* parent);

}  // namespace internal
}  // namespace ui

#endif  // UI_VIEWS_WIDGET_WIDGET_TEST_UTIL_H_
