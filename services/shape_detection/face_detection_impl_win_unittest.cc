// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_win.h"

#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shape_detection {

class FaceDetectionImplWinTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>(
        base::win::ScopedCOMInitializer::kMTA);
    ASSERT_TRUE(scoped_com_initializer_->Succeeded());
  }

 private:
  std::unique_ptr<base::win::ScopedCOMInitializer> scoped_com_initializer_;
};

TEST_F(FaceDetectionImplWinTest, CreateAndDestroy) {
  auto impl = FaceDetectionImplWin::Create();
}

}  // namespace shape_detection
