// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_base.h"

#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkFlattenableSerialization.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkPixelSerializer.h"
#include "third_party/skia/include/core/SkWriteBuffer.h"
#include "third_party/skia/include/effects/SkImageSource.h"
#include "third_party/skia/include/effects/SkPaintImageFilter.h"

namespace skia {
namespace {

// Raw data for a PNG file with 1x1 white pixels.
const unsigned char kWhitePNG[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xde, 0x00, 0x00, 0x00,
    0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00,
    0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00,
    0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x0c, 0x49,
    0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff, 0xff, 0x3f, 0x00, 0x05,
    0xfe, 0x02, 0xfe, 0xdc, 0xcc, 0x59, 0xe7, 0x00, 0x00, 0x00, 0x00, 0x49,
    0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};

class FakeImageGenerator : public SkImageGenerator {
 public:
  FakeImageGenerator() : SkImageGenerator(SkImageInfo::MakeN32Premul(10, 10)) {}
  ~FakeImageGenerator() override { EXPECT_TRUE(decoded_); }

  SkData* onRefEncodedData() override {
    // Use a generator that lets the caller encode it.
    return SkData::MakeWithCString("evilimage").release();
  }

  bool onGetPixels(const SkImageInfo& info,
                   void* pixels,
                   size_t,
                   const Options&) override {
    decoded_ = true;
    memset(pixels, 0, info.computeMinByteSize());
    return true;
  }

 private:
  bool decoded_ = false;
};

class PixelSerializer : public SkPixelSerializer {
 public:
  bool onUseEncodedData(const void* data, size_t len) override {
    CHECK(false);
    return false;
  }

  SkData* onEncode(const SkPixmap&) override {
    EXPECT_TRUE(has_decoded_images_);
    return nullptr;
  }

  bool has_decoded_images_ = false;
};

TEST(SkiaUtilsBaseTest, ImageSerializationDecodesImage) {
  base::TestDiscardableMemoryAllocator allocator;
  base::DiscardableMemoryAllocator::SetInstance(&allocator);

  auto image =
      SkImage::MakeFromGenerator(base::MakeUnique<FakeImageGenerator>());
  auto filter = SkImageSource::Make(image);

  // Serialize the filter. This decodes the image.
  sk_sp<SkData> data = ValidatingSerializeFlattenable(filter.get());

  // Deserialize.
  auto deserialized_filter =
      sk_sp<SkImageFilter>((SkImageFilter*)ValidatingDeserializeFlattenable(
          data->data(), data->size(), SkImageFilter::GetFlattenableType()));

  // Now serialize again to ensure that the deserialized filter did not have any
  // encoded images. Serialization is the only way to deep inspect the filter.
  SkBinaryWriteBuffer writer;
  auto pixel_serializer = sk_make_sp<PixelSerializer>();
  pixel_serializer->has_decoded_images_ = true;
  writer.setPixelSerializer(pixel_serializer);
  writer.writeFlattenable(deserialized_filter.get());
  EXPECT_GT(writer.bytesWritten(), 0u);

  base::DiscardableMemoryAllocator::SetInstance(nullptr);
}

TEST(SkiaUtilsBaseTest, DeserializationWithEncodedImagesFails) {
  base::TestDiscardableMemoryAllocator allocator;
  base::DiscardableMemoryAllocator::SetInstance(&allocator);

  auto image = SkImage::MakeFromGenerator(SkImageGenerator::MakeFromEncoded(
      SkData::MakeWithoutCopy(kWhitePNG, sizeof(kWhitePNG))));
  auto filter = SkImageSource::Make(image);

  // Serialize the filter using default serialization.
  sk_sp<SkData> data(SkValidatingSerializeFlattenable(filter.get()));

  // Deserialize with images disabled.
  auto deserialized_filter =
      sk_sp<SkImageFilter>((SkImageFilter*)ValidatingDeserializeFlattenable(
          data->data(), data->size(), SkImageFilter::GetFlattenableType()));

  // Now serialize again to make sure that all encoded images were rejected
  // during serialization.
  SkBinaryWriteBuffer writer;
  auto pixel_serializer = sk_make_sp<PixelSerializer>();
  writer.setPixelSerializer(pixel_serializer);
  writer.writeFlattenable(deserialized_filter.get());
  EXPECT_GT(writer.bytesWritten(), 0u);

  base::DiscardableMemoryAllocator::SetInstance(nullptr);
}

}  // namespace
}  // namespace skia
