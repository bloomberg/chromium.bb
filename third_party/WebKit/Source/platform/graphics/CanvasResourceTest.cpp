// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasResource.h"

#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "platform/wtf/Functional.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"

using ::testing::_;
using ::testing::Pointee;
using ::testing::SetArrayArgument;
using ::testing::Test;

namespace blink {

class MockGLES2InterfaceWithMailboxSupport : public FakeGLES2Interface {
 public:
  MOCK_METHOD2(ProduceTextureDirectCHROMIUM, void(GLuint, const GLbyte*));
  MOCK_METHOD1(GenMailboxCHROMIUM, void(GLbyte*));
};

class CanvasResourceTest : public Test {
 public:
  void SetUp() override {
    // Install our mock GL context so that it gets served by SharedGpuContext.
    auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
  }

  void TearDown() override { SharedGpuContext::ResetForTesting(); }

  GrContext* GetGrContext() {
    return context_provider_wrapper_->ContextProvider()->GetGrContext();
  }

 protected:
  MockGLES2InterfaceWithMailboxSupport gl_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
};

TEST_F(CanvasResourceTest, SkiaResourceNoMailboxLeak) {
  SkImageInfo image_info =
      SkImageInfo::MakeN32(10, 10, kPremul_SkAlphaType, nullptr);
  sk_sp<SkSurface> surface =
      SkSurface::MakeRenderTarget(GetGrContext(), SkBudgeted::kYes, image_info);

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_TRUE(!!context_provider_wrapper_);
  scoped_refptr<CanvasResource> resource = CanvasResource_Bitmap::Create(
      StaticBitmapImage::Create(surface->makeImageSnapshot(),
                                context_provider_wrapper_),
      nullptr, kLow_SkFilterQuality);

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  gpu::Mailbox test_mailbox;
  test_mailbox.name[0] = 1;
  EXPECT_CALL(gl_, GenMailboxCHROMIUM(_))
      .WillOnce(SetArrayArgument<0>(
          test_mailbox.name, test_mailbox.name + GL_MAILBOX_SIZE_CHROMIUM));
  resource->GetOrCreateGpuMailbox();

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  // No expected call to DeleteTextures becaus skia recycles
  EXPECT_CALL(gl_,
              ProduceTextureDirectCHROMIUM(0, Pointee(test_mailbox.name[0])))
      .Times(1);
  resource = nullptr;

  ::testing::Mock::VerifyAndClearExpectations(&gl_);
}

}  // namespace blink
