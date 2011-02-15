// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/focus/focus_manager.h"
#include "views/widget/native_widget.h"
#include "views/widget/widget_impl.h"
#include "views/widget/widget_impl_test_util.h"

namespace views {

class WidgetTest : public testing::Test {
 public:
  WidgetTest() {}
  virtual ~WidgetTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetTest);
};

TEST_F(WidgetTest, FocusManagerInit_Basic) {
  scoped_ptr<WidgetImpl> widget(internal::CreateWidgetImpl());
  EXPECT_TRUE(widget->GetFocusManager() != NULL);
}

TEST_F(WidgetTest, FocusManagerInit_Nested) {
  scoped_ptr<WidgetImpl> parent(internal::CreateWidgetImpl());
  scoped_ptr<WidgetImpl> child(
      internal::CreateWidgetImplWithParent(parent.get()));

  EXPECT_EQ(parent->GetFocusManager(), child->GetFocusManager());
}

}  // namespace views

