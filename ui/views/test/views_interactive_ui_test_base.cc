// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_interactive_ui_test_base.h"

namespace views {

ViewsInteractiveUITestBase::ViewsInteractiveUITestBase() = default;
ViewsInteractiveUITestBase::~ViewsInteractiveUITestBase() = default;

void ViewsInteractiveUITestBase::SetUp() {
  set_native_widget_type(NativeWidgetType::kDesktop);
  SetUpForInteractiveTests();
  ViewsTestBase::SetUp();
}

}  // namespace views
