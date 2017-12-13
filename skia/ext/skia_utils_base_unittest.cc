// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_base.h"

#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "build/build_config.h"
#include "skia/ext/skia_utils_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/effects/SkImageSource.h"
#include "ui/gfx/codec/png_codec.h"

namespace skia {
namespace {

TEST(SkiaUtilsBaseTest, DeserializationWithEncodedImages) {
// codecs are not available on iOS, so skip this test crbug.com/794298
#if !defined(OS_IOS)
  base::TestDiscardableMemoryAllocator allocator;
  base::DiscardableMemoryAllocator::SetInstance(&allocator);

  SkPMColor pixel = 0xFFFFFFFF;  // white;
  SkImageInfo info = SkImageInfo::MakeN32Premul(1, 1);
  SkPixmap pm = {info, &pixel, sizeof(SkPMColor)};
  auto image = SkImage::MakeRasterCopy(pm);
  EXPECT_TRUE(image);
  auto jpeg_data = image->encodeToData(SkEncodedImageFormat::kJPEG, 100);
  EXPECT_TRUE(jpeg_data);
  auto filter = SkImageSource::Make(SkImage::MakeFromEncoded(jpeg_data));
  EXPECT_TRUE(filter);

  sk_sp<SkData> data(ValidatingSerializeFlattenable(filter.get()));

  // Now we install a proc to see that all embedded images have been converted
  // to png.
  bool was_called = false;
  SkDeserialProcs procs;
  procs.fImageProc = [](const void* data, size_t length,
                        void* was_called) -> sk_sp<SkImage> {
    *(bool*)was_called = true;
    SkBitmap bitmap;
    EXPECT_TRUE(gfx::PNGCodec::Decode(static_cast<const uint8_t*>(data), length,
                                      &bitmap));
    return nullptr;  // allow for normal deserialization
  };
  procs.fImageCtx = &was_called;
  auto flat = SkFlattenable::Deserialize(SkImageFilter::GetFlattenableType(),
                                         data->data(), data->size(), &procs);
  EXPECT_TRUE(flat);
  EXPECT_TRUE(was_called);

  base::DiscardableMemoryAllocator::SetInstance(nullptr);
#endif
}

}  // namespace
}  // namespace skia
