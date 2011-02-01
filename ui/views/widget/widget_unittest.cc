// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

class WidgetTest : public testing::Test {
 public:
  WidgetTest() {}
  virtual ~WidgetTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetTest);
};

TEST_F(WidgetTest, Init) {
}

}  // namespace ui

