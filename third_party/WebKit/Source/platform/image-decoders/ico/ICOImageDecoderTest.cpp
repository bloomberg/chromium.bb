// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-decoders/ico/ICOImageDecoder.h"

#include <memory>
#include "platform/image-decoders/ImageDecoderTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> CreateDecoder() {
  return WTF::WrapUnique(
      new ICOImageDecoder(ImageDecoder::kAlphaNotPremultiplied,
                          ColorBehavior::TransformToTargetForTesting(),
                          ImageDecoder::kNoDecodedImageByteLimit));
}
}

TEST(ICOImageDecoderTests, trunctedIco) {
  RefPtr<SharedBuffer> data =
      ReadFile("/LayoutTests/images/resources/png-in-ico.ico");
  ASSERT_FALSE(data->IsEmpty());

  RefPtr<SharedBuffer> truncated_data =
      SharedBuffer::Create(data->Data(), data->size() / 2);
  auto decoder = CreateDecoder();

  decoder->SetData(truncated_data.Get(), false);
  decoder->FrameBufferAtIndex(0);
  EXPECT_FALSE(decoder->Failed());

  decoder->SetData(truncated_data.Get(), true);
  decoder->FrameBufferAtIndex(0);
  EXPECT_TRUE(decoder->Failed());
}

TEST(ICOImageDecoderTests, errorInPngInIco) {
  RefPtr<SharedBuffer> data =
      ReadFile("/LayoutTests/images/resources/png-in-ico.ico");
  ASSERT_FALSE(data->IsEmpty());

  // Modify the file to have a broken CRC in IHDR.
  constexpr size_t kCrcOffset = 22 + 29;
  constexpr size_t kCrcSize = 4;
  RefPtr<SharedBuffer> modified_data =
      SharedBuffer::Create(data->Data(), kCrcOffset);
  Vector<char> bad_crc(kCrcSize, 0);
  modified_data->Append(bad_crc);
  modified_data->Append(data->Data() + kCrcOffset + kCrcSize,
                        data->size() - kCrcOffset - kCrcSize);

  auto decoder = CreateDecoder();
  decoder->SetData(modified_data.Get(), true);

  // ICOImageDecoder reports the frame count based on whether enough data has
  // been received according to the icon directory. So even though the
  // embedded PNG is broken, there is enough data to include it in the frame
  // count.
  EXPECT_EQ(1u, decoder->FrameCount());

  decoder->FrameBufferAtIndex(0);
  EXPECT_TRUE(decoder->Failed());
}

TEST(ICOImageDecoderTests, parseAndDecodeByteByByte) {
  TestByteByByteDecode(&CreateDecoder,
                       "/LayoutTests/images/resources/png-in-ico.ico", 1u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateDecoder,
                       "/LayoutTests/images/resources/2entries.ico", 2u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateDecoder,
                       "/LayoutTests/images/resources/greenbox-3frames.cur", 3u,
                       kAnimationNone);
  TestByteByByteDecode(
      &CreateDecoder,
      "/LayoutTests/images/resources/icon-without-and-bitmap.ico", 1u,
      kAnimationNone);
  TestByteByByteDecode(&CreateDecoder, "/LayoutTests/images/resources/1bit.ico",
                       1u, kAnimationNone);
  TestByteByByteDecode(&CreateDecoder,
                       "/LayoutTests/images/resources/bug653075.ico", 2u,
                       kAnimationNone);
}

}  // namespace blink
