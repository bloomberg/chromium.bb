// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/ImageBuffer.h"

#include "platform/wtf/typed_arrays/ArrayBufferContents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

class ImageBufferTest : public ::testing::Test {};

// This test verifies if requesting a large ImageData that cannot be handled by
// V8 is denied by ImageBuffer. This prevents V8 from crashing the renderer if
// the user asks to get back the ImageData.
TEST_F(ImageBufferTest, GetImageDataTooBigToAllocateDoesNotCrash) {
  std::unique_ptr<ImageBuffer> image_buffer =
      ImageBuffer::Create(IntSize(1, 1), kDoNotInitializeImagePixels);
  EXPECT_TRUE(image_buffer->IsSurfaceValid());

  IntRect too_big_rect(IntPoint(0, 0),
                       IntSize(1, (v8::TypedArray::kMaxLength / 4) + 1));
  WTF::ArrayBufferContents contents;
  EXPECT_FALSE(
      image_buffer->GetImageData(kUnmultiplied, too_big_rect, contents));
}

}  // namespace blink
