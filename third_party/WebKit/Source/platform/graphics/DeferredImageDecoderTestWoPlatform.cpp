// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/DeferredImageDecoder.h"

#include <memory>
#include "platform/SharedBuffer.h"
#include "platform/image-decoders/ImageDecoderTestHelpers.h"
#include "platform/wtf/RefPtr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {
namespace {

sk_sp<SkImage> CreateFrameAtIndex(DeferredImageDecoder* decoder, size_t index) {
  return SkImage::MakeFromGenerator(base::MakeUnique<SkiaPaintImageGenerator>(
      decoder->CreateGeneratorAtIndex(index)));
}

}  // namespace

/**
 *  Used to test decoding SkImages out of order.
 *  e.g.
 *  SkImage* imageA = decoder.createFrameAtIndex(0);
 *  // supply more (but not all) data to the decoder
 *  SkImage* imageB = decoder.createFrameAtIndex(laterFrame);
 *  draw(imageB);
 *  draw(imageA);
 *
 *  This results in using the same ImageDecoder (in the ImageDecodingStore) to
 *  decode less data the second time. This test ensures that it is safe to do
 *  so.
 *
 *  @param fileName File to decode
 *  @param bytesForFirstFrame Number of bytes needed to return an SkImage
 *  @param laterFrame Frame to decode with almost complete data. Can be 0.
 */
static void MixImages(const char* file_name,
                      size_t bytes_for_first_frame,
                      size_t later_frame) {
  const Vector<char> file = ReadFile(file_name)->Copy();

  RefPtr<SharedBuffer> partial_file =
      SharedBuffer::Create(file.data(), bytes_for_first_frame);
  std::unique_ptr<DeferredImageDecoder> decoder = DeferredImageDecoder::Create(
      partial_file, false, ImageDecoder::kAlphaPremultiplied,
      ColorBehavior::Ignore());
  ASSERT_NE(decoder, nullptr);
  sk_sp<SkImage> partial_image = CreateFrameAtIndex(decoder.get(), 0);

  RefPtr<SharedBuffer> almost_complete_file =
      SharedBuffer::Create(file.data(), file.size() - 1);
  decoder->SetData(almost_complete_file, false);
  sk_sp<SkImage> image_with_more_data =
      CreateFrameAtIndex(decoder.get(), later_frame);

  // we now want to ensure we don't crash if we access these in this order
  SkImageInfo info = SkImageInfo::MakeN32Premul(10, 10);
  sk_sp<SkSurface> surf = SkSurface::MakeRaster(info);
  surf->getCanvas()->drawImage(image_with_more_data, 0, 0);
  surf->getCanvas()->drawImage(partial_image, 0, 0);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesGif) {
  MixImages("/LayoutTests/images/resources/animated.gif", 818u, 1u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesPng) {
  MixImages("/LayoutTests/images/resources/mu.png", 910u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesJpg) {
  MixImages("/LayoutTests/images/resources/2-dht.jpg", 177u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesWebp) {
  MixImages("/LayoutTests/images/resources/webp-animated.webp", 142u, 1u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesBmp) {
  MixImages("/LayoutTests/images/resources/lenna.bmp", 122u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesIco) {
  MixImages("/LayoutTests/images/resources/wrong-frame-dimensions.ico", 1376u,
            1u);
}

TEST(DeferredImageDecoderTestWoPlatform, fragmentedSignature) {
  const char* test_files[] = {
      "/LayoutTests/images/resources/animated.gif",
      "/LayoutTests/images/resources/mu.png",
      "/LayoutTests/images/resources/2-dht.jpg",
      "/LayoutTests/images/resources/webp-animated.webp",
      "/LayoutTests/images/resources/lenna.bmp",
      "/LayoutTests/images/resources/wrong-frame-dimensions.ico",
  };

  for (size_t i = 0; i < SK_ARRAY_COUNT(test_files); ++i) {
    RefPtr<SharedBuffer> file_buffer = ReadFile(test_files[i]);
    ASSERT_NE(file_buffer, nullptr);
    // We need contiguous data, which SharedBuffer doesn't guarantee.
    sk_sp<SkData> sk_data = file_buffer->GetAsSkData();
    EXPECT_EQ(sk_data->size(), file_buffer->size());
    const char* data = reinterpret_cast<const char*>(sk_data->bytes());

    // Truncated signature (only 1 byte).  Decoder instantiation should fail.
    RefPtr<SharedBuffer> buffer = SharedBuffer::Create<size_t>(data, 1u);
    EXPECT_FALSE(ImageDecoder::HasSufficientDataToSniffImageType(*buffer));
    EXPECT_EQ(nullptr, DeferredImageDecoder::Create(
                           buffer, false, ImageDecoder::kAlphaPremultiplied,
                           ColorBehavior::Ignore()));

    // Append the rest of the data.  We should be able to sniff the signature
    // now, even if segmented.
    buffer->Append<size_t>(data + 1, sk_data->size() - 1);
    EXPECT_TRUE(ImageDecoder::HasSufficientDataToSniffImageType(*buffer));
    std::unique_ptr<DeferredImageDecoder> decoder =
        DeferredImageDecoder::Create(buffer, false,
                                     ImageDecoder::kAlphaPremultiplied,
                                     ColorBehavior::Ignore());
    ASSERT_NE(decoder, nullptr);
    EXPECT_TRUE(String(test_files[i]).EndsWith(decoder->FilenameExtension()));
  }
}

}  // namespace blink
