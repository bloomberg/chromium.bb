// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_test_util.h"

namespace ui {

class WidgetTest : public testing::Test {
 public:
  WidgetTest() {}
  virtual ~WidgetTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetTest);
};

TEST_F(WidgetTest, FocusManagerInit_Basic) {
  scoped_ptr<Widget> widget(internal::CreateWidget());
  EXPECT_TRUE(widget->GetFocusManager() != NULL);
}

TEST_F(WidgetTest, FocusManagerInit_Nested) {
  scoped_ptr<Widget> parent(internal::CreateWidget());
  scoped_ptr<Widget> child(internal::CreateWidgetWithParent(parent.get()));

  EXPECT_EQ(parent->GetFocusManager(), child->GetFocusManager());
}

}  // namespace ui

