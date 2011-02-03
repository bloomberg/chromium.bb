// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_test_util.h"

namespace ui {
namespace internal {

class RootViewTest : public testing::Test {
 public:
  RootViewTest() {}
  virtual ~RootViewTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RootViewTest);
};

TEST_F(RootViewTest, FocusedViewResetOnViewRemoval) {
  View v;
  v.set_parent_owned(false);
  scoped_ptr<Widget> widget(CreateWidgetWithContents(&v));

  v.RequestFocus();

  FocusManager* focus_manager = widget->GetFocusManager();
  EXPECT_TRUE(focus_manager != NULL);
  EXPECT_EQ(&v, focus_manager->focused_view());

  v.parent()->RemoveChildView(&v);

  EXPECT_NE(&v, focus_manager->focused_view());
  EXPECT_EQ(NULL, focus_manager->focused_view());
}

}  // namespace internal
}  // namespace ui
