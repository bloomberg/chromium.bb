// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include <va/va.h>

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/test/gtest_util.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {

constexpr VAImageFormat kImageFormatI420 = {
    .fourcc = VA_FOURCC_I420,
    .byte_order = VA_LSB_FIRST,
    .bits_per_pixel = 12,
};

}  // namespace

class VaapiUtilsTest : public testing::Test {
 protected:
  VaapiUtilsTest() = default;

  void SetUp() override {
    // Create a VaapiWrapper for testing.
    vaapi_wrapper_ = VaapiWrapper::Create(
        VaapiWrapper::kDecode, VAProfileJPEGBaseline,
        base::BindRepeating([]() { LOG(FATAL) << "Oh noes! Decoder failed"; }));
    ASSERT_TRUE(vaapi_wrapper_);
  }

 protected:
  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(VaapiUtilsTest);
};

// This test exercises the usual ScopedVAImage lifetime.
TEST_F(VaapiUtilsTest, ScopedVAImage) {
  std::vector<VASurfaceID> va_surfaces;
  const gfx::Size coded_size(64, 64);
  ASSERT_TRUE(vaapi_wrapper_->CreateContextAndSurfaces(
      VA_RT_FORMAT_YUV420, coded_size, 1, &va_surfaces));
  ASSERT_EQ(va_surfaces.size(), 1u);

  std::unique_ptr<ScopedVAImage> scoped_image;
  {
    // On Stoney-Ridge devices the output image format is dependent on the
    // surface format. However when context has not been executed the output
    // image format seems to default to I420. https://crbug.com/828119
    VAImageFormat va_image_format = kImageFormatI420;
    base::AutoLock auto_lock(*vaapi_wrapper_->va_lock_);
    scoped_image = std::make_unique<ScopedVAImage>(
        vaapi_wrapper_->va_lock_, vaapi_wrapper_->va_display_, va_surfaces[0],
        &va_image_format, coded_size);

    EXPECT_TRUE(scoped_image->image());
    ASSERT_TRUE(scoped_image->IsValid());
    EXPECT_TRUE(scoped_image->va_buffer()->IsValid());
    EXPECT_TRUE(scoped_image->va_buffer()->data());
  }
}

// This test exercises creation of a ScopedVAImage with a bad VASurfaceID.
TEST_F(VaapiUtilsTest, BadScopedVAImage) {
#if DCHECK_IS_ON()
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
#endif

  const std::vector<VASurfaceID> va_surfaces = {VA_INVALID_ID};
  const gfx::Size coded_size(64, 64);

  std::unique_ptr<ScopedVAImage> scoped_image;
  {
    VAImageFormat va_image_format = kImageFormatI420;
    base::AutoLock auto_lock(*vaapi_wrapper_->va_lock_);
    scoped_image = std::make_unique<ScopedVAImage>(
        vaapi_wrapper_->va_lock_, vaapi_wrapper_->va_display_, va_surfaces[0],
        &va_image_format, coded_size);

    EXPECT_TRUE(scoped_image->image());
    EXPECT_FALSE(scoped_image->IsValid());
#if DCHECK_IS_ON()
    EXPECT_DCHECK_DEATH(scoped_image->va_buffer());
#else
    EXPECT_FALSE(scoped_image->va_buffer());
#endif
  }
}

// This test exercises creation of a ScopedVABufferMapping with bad VABufferIDs.
TEST_F(VaapiUtilsTest, BadScopedVABufferMapping) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  base::AutoLock auto_lock(*vaapi_wrapper_->va_lock_);

  // A ScopedVABufferMapping with a VA_INVALID_ID VABufferID is DCHECK()ed.
  EXPECT_DCHECK_DEATH(std::make_unique<ScopedVABufferMapping>(
      vaapi_wrapper_->va_lock_, vaapi_wrapper_->va_display_, VA_INVALID_ID));

  // This should not hit any DCHECK() but will create an invalid
  // ScopedVABufferMapping.
  auto scoped_buffer = std::make_unique<ScopedVABufferMapping>(
      vaapi_wrapper_->va_lock_, vaapi_wrapper_->va_display_, VA_INVALID_ID - 1);
  EXPECT_FALSE(scoped_buffer->IsValid());
}

}  // namespace media
