// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

class MockWidget {
 public:
  MockWidget() = default;
  ~MockWidget() = default;
  MockWidget(const MockWidget&) = delete;
  MockWidget& operator=(const MockWidget&) = delete;

  MOCK_METHOD1(Frob, void(int));
};

}  // namespace

using HelloTest = ::testing::Test;

TEST_F(HelloTest, Expect) {
  EXPECT_TRUE(true);
}

TEST_F(HelloTest, Mocks) {
  MockWidget mock_widget;

  EXPECT_CALL(mock_widget, Frob(3));
  mock_widget.Frob(3);
}
