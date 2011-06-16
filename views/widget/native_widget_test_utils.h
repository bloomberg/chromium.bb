// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WIDGET_TEST_UTILS_H_
#define VIEWS_WIDGET_NATIVE_WIDGET_TEST_UTILS_H_
#pragma once

namespace views {
class View;
namespace internal {
class NativeWidgetPrivate;

// Create dummy Widgets for use in testing.
NativeWidgetPrivate* CreateNativeWidget();
NativeWidgetPrivate* CreateNativeWidgetWithContents(View* contents_view);
NativeWidgetPrivate* CreateNativeWidgetWithParent(NativeWidgetPrivate* parent);

}  // namespace internal
}  // namespace views

#endif  // VIEWS_WIDGET_NATIVE_WIDGET_TEST_UTILS_H_
