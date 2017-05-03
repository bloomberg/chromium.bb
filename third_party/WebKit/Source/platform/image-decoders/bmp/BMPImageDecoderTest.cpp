// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-decoders/bmp/BMPImageDecoder.h"

#include <memory>
#include "platform/SharedBuffer.h"
#include "platform/image-decoders/ImageDecoderTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> CreateDecoder() {
  return WTF::WrapUnique(
      new BMPImageDecoder(ImageDecoder::kAlphaNotPremultiplied,
                          ColorBehavior::TransformToTargetForTesting(),
                          ImageDecoder::kNoDecodedImageByteLimit));
}

}  // anonymous namespace

TEST(BMPImageDecoderTest, isSizeAvailable) {
  const char* bmp_file = "/LayoutTests/images/resources/lenna.bmp";  // 256x256
  RefPtr<SharedBuffer> data = ReadFile(bmp_file);
  ASSERT_TRUE(data.Get());

  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();
  decoder->SetData(data.Get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(256, decoder->Size().Width());
  EXPECT_EQ(256, decoder->Size().Height());
}

TEST(BMPImageDecoderTest, parseAndDecode) {
  const char* bmp_file = "/LayoutTests/images/resources/lenna.bmp";  // 256x256
  RefPtr<SharedBuffer> data = ReadFile(bmp_file);
  ASSERT_TRUE(data.Get());

  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();
  decoder->SetData(data.Get(), true);

  ImageFrame* frame = decoder->FrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_EQ(256, frame->Bitmap().width());
  EXPECT_EQ(256, frame->Bitmap().height());
  EXPECT_FALSE(decoder->Failed());
}

// Test if a BMP decoder returns a proper error while decoding an empty image.
TEST(BMPImageDecoderTest, emptyImage) {
  const char* bmp_file = "/LayoutTests/images/resources/0x0.bmp";  // 0x0
  RefPtr<SharedBuffer> data = ReadFile(bmp_file);
  ASSERT_TRUE(data.Get());

  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();
  decoder->SetData(data.Get(), true);

  ImageFrame* frame = decoder->FrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameEmpty, frame->GetStatus());
  EXPECT_TRUE(decoder->Failed());
}

TEST(BMPImageDecoderTest, int32MinHeight) {
  const char* bmp_file =
      "/LayoutTests/images/resources/1xint32_min.bmp";  // 0xINT32_MIN
  RefPtr<SharedBuffer> data = ReadFile(bmp_file);
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();
  // Test when not all data is received.
  decoder->SetData(data.Get(), false);
  EXPECT_FALSE(decoder->IsSizeAvailable());
  EXPECT_TRUE(decoder->Failed());
}

// This test verifies that calling SharedBuffer::MergeSegmentsIntoBuffer() does
// not break BMP decoding at a critical point: in between a call to decode the
// size (when BMPImageDecoder stops while it may still have input data to
// read) and a call to do a full decode.
TEST(BMPImageDecoderTest, mergeBuffer) {
  const char* bmp_file = "/LayoutTests/images/resources/lenna.bmp";
  TestMergeBuffer(&CreateDecoder, bmp_file);
}

}  // namespace blink
