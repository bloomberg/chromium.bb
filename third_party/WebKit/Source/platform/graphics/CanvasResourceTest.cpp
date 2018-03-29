// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasResource.h"

#include "base/run_loop.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/Functional.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"

using testing::_;
using testing::Pointee;
using testing::Return;
using testing::SetArrayArgument;
using testing::Test;

namespace blink {

class MockGLES2InterfaceWithMailboxSupport : public FakeGLES2Interface {
 public:
  MOCK_METHOD2(ProduceTextureDirectCHROMIUM, void(GLuint, const GLbyte*));
  MOCK_METHOD1(GenMailboxCHROMIUM, void(GLbyte*));
  MOCK_METHOD1(GenUnverifiedSyncTokenCHROMIUM, void(GLbyte*));
  MOCK_METHOD4(CreateImageCHROMIUM,
               GLuint(ClientBuffer, GLsizei, GLsizei, GLenum));
};

class FakeCanvasResourcePlatformSupport : public TestingPlatformSupport {
 public:
  void RunUntilStop() const { base::RunLoop().Run(); }

  void StopRunning() const { base::RunLoop().Quit(); }

 private:
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override {
    return &test_gpu_memory_buffer_manager_;
  }

  viz::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager_;
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

gpu::SyncToken GenTestSyncToken(int id) {
  gpu::SyncToken token;
  token.Set(gpu::CommandBufferNamespace::GPU_IO,
            gpu::CommandBufferId::FromUnsafeValue(id), 1);
  return token;
}

TEST_F(CanvasResourceTest, SkiaResourceNoMailboxLeak) {
  SkImageInfo image_info =
      SkImageInfo::MakeN32(10, 10, kPremul_SkAlphaType, nullptr);
  sk_sp<SkSurface> surface =
      SkSurface::MakeRenderTarget(GetGrContext(), SkBudgeted::kYes, image_info);

  testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_TRUE(!!context_provider_wrapper_);
  scoped_refptr<CanvasResource> resource = CanvasResource_Bitmap::Create(
      StaticBitmapImage::Create(surface->makeImageSnapshot(),
                                context_provider_wrapper_),
      nullptr, kLow_SkFilterQuality);

  testing::Mock::VerifyAndClearExpectations(&gl_);

  gpu::Mailbox test_mailbox;
  test_mailbox.name[0] = 1;
  EXPECT_CALL(gl_, GenMailboxCHROMIUM(_))
      .WillOnce(SetArrayArgument<0>(
          test_mailbox.name, test_mailbox.name + GL_MAILBOX_SIZE_CHROMIUM));
  resource->GetOrCreateGpuMailbox();

  testing::Mock::VerifyAndClearExpectations(&gl_);

  // No expected call to DeleteTextures becaus skia recycles
  // No expected call to ProduceTextureDirectCHROMIUM(0, *) because
  // mailbox is cached by GraphicsContext3DUtils and therefore does not need to
  // be orphaned.
  EXPECT_CALL(gl_, ProduceTextureDirectCHROMIUM(0, _)).Times(0);
  resource = nullptr;

  testing::Mock::VerifyAndClearExpectations(&gl_);

  // Purging skia's resource cache will finally delete the GrTexture, resulting
  // in the mailbox being orphaned via ProduceTextureDirectCHROMIUM.
  EXPECT_CALL(gl_,
              ProduceTextureDirectCHROMIUM(0, Pointee(test_mailbox.name[0])))
      .Times(0);
  GetGrContext()->performDeferredCleanup(std::chrono::milliseconds(0));

  testing::Mock::VerifyAndClearExpectations(&gl_);
}

TEST_F(CanvasResourceTest, GpuMemoryBufferSyncTokenRefresh) {
  ScopedTestingPlatformSupport<FakeCanvasResourcePlatformSupport> platform;

  constexpr GLuint image_id = 1;
  EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _)).WillOnce(Return(image_id));
  scoped_refptr<CanvasResource> resource =
      CanvasResource_GpuMemoryBuffer::Create(
          IntSize(10, 10), CanvasColorParams(),
          SharedGpuContext::ContextProviderWrapper(),
          nullptr,  // Resource provider
          kLow_SkFilterQuality);

  EXPECT_TRUE(bool(resource));

  testing::Mock::VerifyAndClearExpectations(&gl_);

  gpu::Mailbox test_mailbox;
  test_mailbox.name[0] = 1;
  EXPECT_CALL(gl_, GenMailboxCHROMIUM(_))
      .WillOnce(SetArrayArgument<0>(
          test_mailbox.name, test_mailbox.name + GL_MAILBOX_SIZE_CHROMIUM));
  resource->GetOrCreateGpuMailbox();

  testing::Mock::VerifyAndClearExpectations(&gl_);

  const gpu::SyncToken reference_token1 = GenTestSyncToken(1);
  EXPECT_CALL(gl_, GenUnverifiedSyncTokenCHROMIUM(_))
      .WillOnce(SetArrayArgument<0>(
          reinterpret_cast<const GLbyte*>(&reference_token1),
          reinterpret_cast<const GLbyte*>(&reference_token1 + 1)));
  gpu::SyncToken actual_token = resource->GetSyncToken();
  EXPECT_EQ(actual_token, reference_token1);

  testing::Mock::VerifyAndClearExpectations(&gl_);

  // Grabbing the mailbox again without making any changes must not result in
  // a sync token refresh
  EXPECT_CALL(gl_, GenMailboxCHROMIUM(_)).Times(0);
  EXPECT_CALL(gl_, GenUnverifiedSyncTokenCHROMIUM(_)).Times(0);
  resource->GetOrCreateGpuMailbox();
  actual_token = resource->GetSyncToken();
  EXPECT_EQ(actual_token, reference_token1);

  testing::Mock::VerifyAndClearExpectations(&gl_);

  // Grabbing the mailbox again after a content change must result in
  // a sync token refresh, but the mailbox gets re-used.
  EXPECT_CALL(gl_, GenMailboxCHROMIUM(_)).Times(0);
  const gpu::SyncToken reference_token2 = GenTestSyncToken(2);
  EXPECT_CALL(gl_, GenUnverifiedSyncTokenCHROMIUM(_))
      .WillOnce(SetArrayArgument<0>(
          reinterpret_cast<const GLbyte*>(&reference_token2),
          reinterpret_cast<const GLbyte*>(&reference_token2 + 1)));
  resource->CopyFromTexture(1,  // source texture id
                            GL_RGBA, GL_UNSIGNED_BYTE);
  resource->GetOrCreateGpuMailbox();
  actual_token = resource->GetSyncToken();
  EXPECT_EQ(actual_token, reference_token2);

  testing::Mock::VerifyAndClearExpectations(&gl_);
}

}  // namespace blink
