// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-decoders/ico/ICOImageDecoder.h"

#include "platform/image-decoders/ImageDecoderTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> createDecoder() {
  return WTF::wrapUnique(
      new ICOImageDecoder(ImageDecoder::AlphaNotPremultiplied,
                          ColorBehavior::transformToTargetForTesting(),
                          ImageDecoder::noDecodedImageByteLimit));
}
}

TEST(ICOImageDecoderTests, trunctedIco) {
  RefPtr<SharedBuffer> data =
      readFile("/LayoutTests/images/resources/png-in-ico.ico");
  ASSERT_FALSE(data->isEmpty());

  RefPtr<SharedBuffer> truncatedData =
      SharedBuffer::create(data->data(), data->size() / 2);
  auto decoder = createDecoder();

  decoder->setData(truncatedData.get(), false);
  decoder->frameBufferAtIndex(0);
  EXPECT_FALSE(decoder->failed());

  decoder->setData(truncatedData.get(), true);
  decoder->frameBufferAtIndex(0);
  EXPECT_TRUE(decoder->failed());
}

TEST(ICOImageDecoderTests, errorInPngInIco) {
  RefPtr<SharedBuffer> data =
      readFile("/LayoutTests/images/resources/png-in-ico.ico");
  ASSERT_FALSE(data->isEmpty());

  // Modify the file to have a broken CRC in IHDR.
  constexpr size_t crcOffset = 22 + 29;
  constexpr size_t crcSize = 4;
  RefPtr<SharedBuffer> modifiedData =
      SharedBuffer::create(data->data(), crcOffset);
  Vector<char> badCrc(crcSize, 0);
  modifiedData->append(badCrc);
  modifiedData->append(data->data() + crcOffset + crcSize,
                       data->size() - crcOffset - crcSize);

  auto decoder = createDecoder();
  decoder->setData(modifiedData.get(), true);

  // ICOImageDecoder reports the frame count based on whether enough data has
  // been received according to the icon directory. So even though the
  // embedded PNG is broken, there is enough data to include it in the frame
  // count.
  EXPECT_EQ(1u, decoder->frameCount());

  decoder->frameBufferAtIndex(0);
  EXPECT_TRUE(decoder->failed());
}

TEST(ICOImageDecoderTests, parseAndDecodeByteByByte) {
  testByteByByteDecode(&createDecoder,
                       "/LayoutTests/images/resources/png-in-ico.ico", 1u,
                       cAnimationNone);
  testByteByByteDecode(&createDecoder,
                       "/LayoutTests/images/resources/2entries.ico", 2u,
                       cAnimationNone);
  testByteByByteDecode(&createDecoder,
                       "/LayoutTests/images/resources/greenbox-3frames.cur", 3u,
                       cAnimationNone);
  testByteByByteDecode(
      &createDecoder,
      "/LayoutTests/images/resources/icon-without-and-bitmap.ico", 1u,
      cAnimationNone);
  testByteByByteDecode(&createDecoder, "/LayoutTests/images/resources/1bit.ico",
                       1u, cAnimationNone);
  testByteByByteDecode(&createDecoder,
                       "/LayoutTests/images/resources/bug653075.ico", 2u,
                       cAnimationNone);
}

}  // namespace blink
