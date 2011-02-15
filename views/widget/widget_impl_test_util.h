// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_WIDGET_IMPL_TEST_UTIL_H_
#define VIEWS_WIDGET_WIDGET_IMPL_TEST_UTIL_H_
#pragma once

namespace views {
class View;
class WidgetImpl;
namespace internal {

// Create dummy WidgetImpls for use in testing.
WidgetImpl* CreateWidgetImpl();
WidgetImpl* CreateWidgetImplWithContents(View* contents_view);
WidgetImpl* CreateWidgetImplWithParent(WidgetImpl* parent);

}  // namespace internal
}  // namespace views

#endif  // VIEWS_WIDGET_WIDGET_IMPL_TEST_UTIL_H_
