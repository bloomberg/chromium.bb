// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/ImageData.h"

#include "core/dom/ExceptionCode.h"
#include "platform/geometry/IntSize.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class ImageDataTest : public ::testing::Test {
 protected:
  ImageDataTest(){};
  void TearDown(){};
};

TEST_F(ImageDataTest, NegativeAndZeroIntSizeTest) {
  ImageData* imageData;

  imageData = ImageData::create(IntSize(0, 10));
  EXPECT_EQ(imageData, nullptr);

  imageData = ImageData::create(IntSize(10, 0));
  EXPECT_EQ(imageData, nullptr);

  imageData = ImageData::create(IntSize(0, 0));
  EXPECT_EQ(imageData, nullptr);

  imageData = ImageData::create(IntSize(-1, 10));
  EXPECT_EQ(imageData, nullptr);

  imageData = ImageData::create(IntSize(10, -1));
  EXPECT_EQ(imageData, nullptr);

  imageData = ImageData::create(IntSize(-1, -1));
  EXPECT_EQ(imageData, nullptr);
}

// This test passes if it does not crash. If the required memory is not
// allocated to the ImageData, then an exception must raise.
TEST_F(ImageDataTest, CreateImageDataTooBig) {
  DummyExceptionStateForTesting exceptionState;
  ImageData* tooBigImageData = ImageData::create(32767, 32767, exceptionState);
  if (!tooBigImageData) {
    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(exceptionState.code(), V8RangeError);
  }
}

}  // namspace
}  // namespace blink
