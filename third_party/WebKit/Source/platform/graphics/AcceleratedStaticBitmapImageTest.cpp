// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "skia/ext/texture_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"

using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace blink {

class AcceleratedStaticBitmapImageTest : public Test {
 public:
  void SetUp() override {
    auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl, nullptr);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
  }
  void TearDown() override { SharedGpuContext::ResetForTesting(); }

 protected:
  FakeGLES2Interface gl_;
};

TEST_F(AcceleratedStaticBitmapImageTest, NoTextureHolderThrashing) {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  GrContext* gr = context_provider_wrapper->ContextProvider()->GetGrContext();
  SkImageInfo imageInfo = SkImageInfo::MakeN32Premul(100, 100);

  sk_sp<SkSurface> surface =
      SkSurface::MakeRenderTarget(gr, SkBudgeted::kNo, imageInfo);

  sk_sp<SkImage> image = surface->makeImageSnapshot();
  scoped_refptr<AcceleratedStaticBitmapImage> bitmap =
      AcceleratedStaticBitmapImage::CreateFromSkImage(image,
                                                      context_provider_wrapper);
  EXPECT_TRUE(bitmap->TextureHolderForTesting()->IsSkiaTextureHolder());

  sk_sp<SkImage> stored_image =
      bitmap->PaintImageForCurrentFrame().GetSkImage();
  EXPECT_EQ(stored_image.get(), image.get());

  bitmap->EnsureMailbox(kUnverifiedSyncToken, GL_LINEAR);
  EXPECT_TRUE(bitmap->TextureHolderForTesting()->IsMailboxTextureHolder());

  // Verify that calling PaintImageForCurrentFrame does not swap out of mailbox
  // mode. It should use the cached original image instead.
  stored_image = bitmap->PaintImageForCurrentFrame().GetSkImage();

  EXPECT_EQ(stored_image.get(), image.get());
  EXPECT_TRUE(bitmap->TextureHolderForTesting()->IsMailboxTextureHolder());
}

}  // namespace blink
