// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ksv/views/keyboard_shortcut_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace keyboard_shortcut_viewer {

using KeyboardShortcutViewTest = views::ViewsTestBase;

// Shows and closes the widget for KeyboardShortcutViewer.
TEST_F(KeyboardShortcutViewTest, ShowAndClose) {
  // Showing the widget.
  views::Widget* widget = KeyboardShortcutView::Show(GetContext());
  EXPECT_TRUE(widget);

  // Cleaning up.
  widget->CloseNow();
}

}  // namespace keyboard_shortcut_viewer
