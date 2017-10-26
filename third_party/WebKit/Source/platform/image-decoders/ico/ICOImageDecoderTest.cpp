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

std::unique_ptr<ImageDecoder> CreateICODecoder() {
  return WTF::WrapUnique(new ICOImageDecoder(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::TransformToSRGB(),
      ImageDecoder::kNoDecodedImageByteLimit));
}
}

TEST(ICOImageDecoderTests, trunctedIco) {
  const Vector<char> data =
      ReadFile("/LayoutTests/images/resources/png-in-ico.ico")->Copy();
  ASSERT_FALSE(data.IsEmpty());

  scoped_refptr<SharedBuffer> truncated_data =
      SharedBuffer::Create(data.data(), data.size() / 2);
  auto decoder = CreateICODecoder();

  decoder->SetData(truncated_data.get(), false);
  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_FALSE(decoder->Failed());

  decoder->SetData(truncated_data.get(), true);
  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(decoder->Failed());
}

TEST(ICOImageDecoderTests, errorInPngInIco) {
  const Vector<char> data =
      ReadFile("/LayoutTests/images/resources/png-in-ico.ico")->Copy();
  ASSERT_FALSE(data.IsEmpty());

  // Modify the file to have a broken CRC in IHDR.
  constexpr size_t kCrcOffset = 22 + 29;
  constexpr size_t kCrcSize = 4;
  scoped_refptr<SharedBuffer> modified_data =
      SharedBuffer::Create(data.data(), kCrcOffset);
  Vector<char> bad_crc(kCrcSize, 0);
  modified_data->Append(bad_crc);
  modified_data->Append(data.data() + kCrcOffset + kCrcSize,
                        data.size() - kCrcOffset - kCrcSize);

  auto decoder = CreateICODecoder();
  decoder->SetData(modified_data.get(), true);

  // ICOImageDecoder reports the frame count based on whether enough data has
  // been received according to the icon directory. So even though the
  // embedded PNG is broken, there is enough data to include it in the frame
  // count.
  EXPECT_EQ(1u, decoder->FrameCount());

  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(decoder->Failed());
}

TEST(ICOImageDecoderTests, parseAndDecodeByteByByte) {
  TestByteByByteDecode(&CreateICODecoder,
                       "/LayoutTests/images/resources/png-in-ico.ico", 1u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder,
                       "/LayoutTests/images/resources/2entries.ico", 2u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder,
                       "/LayoutTests/images/resources/greenbox-3frames.cur", 3u,
                       kAnimationNone);
  TestByteByByteDecode(
      &CreateICODecoder,
      "/LayoutTests/images/resources/icon-without-and-bitmap.ico", 1u,
      kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder,
                       "/LayoutTests/images/resources/1bit.ico", 1u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder,
                       "/LayoutTests/images/resources/bug653075.ico", 2u,
                       kAnimationNone);
}

TEST(ICOImageDecoderTests, NullData) {
  static constexpr size_t kSizeOfBadBlock = 6 + 16 + 1;

  scoped_refptr<SharedBuffer> ico_file_data =
      ReadFile("/LayoutTests/images/resources/png-in-ico.ico");
  ASSERT_FALSE(ico_file_data->IsEmpty());
  ASSERT_LT(kSizeOfBadBlock, ico_file_data->size());

  scoped_refptr<SharedBuffer> truncated_data =
      SharedBuffer::Create(ico_file_data->Data(), kSizeOfBadBlock);
  auto decoder = CreateICODecoder();

  decoder->SetData(truncated_data.get(), false);
  decoder->SetMemoryAllocator(nullptr);
  EXPECT_FALSE(decoder->Failed());

  auto* frame = decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_EQ(nullptr, frame);

  decoder->SetData(scoped_refptr<SegmentReader>(nullptr), false);
  decoder->ClearCacheExceptFrame(0);
  decoder->SetMemoryAllocator(nullptr);
  EXPECT_FALSE(decoder->Failed());
}

}  // namespace blink
