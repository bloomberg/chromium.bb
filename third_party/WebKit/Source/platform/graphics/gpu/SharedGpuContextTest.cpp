// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/SharedGpuContext.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/Canvas2DLayerBridge.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using ::testing::Test;

namespace blink {

namespace {

template <class GLES2InterfaceType>
class SharedGpuContextTestBase : public Test {
 public:
  void SetUp() override {
    SharedGpuContext::SetContextProviderFactoryForTesting([this] {
      gl_.SetIsContextLost(false);
      return std::unique_ptr<WebGraphicsContext3DProvider>(
          new FakeWebGraphicsContext3DProvider(&gl_));
    });
  }

  void TearDown() override {
    SharedGpuContext::SetContextProviderFactoryForTesting(nullptr);
  }

  bool IsUnitTest() { return true; }

  GLES2InterfaceType gl_;
};

class SharedGpuContextTest
    : public SharedGpuContextTestBase<FakeGLES2Interface> {};

class MailboxMockGLES2Interface : public FakeGLES2Interface {
 public:
  MOCK_METHOD1(GenMailboxCHROMIUM, void(GLbyte*));
  MOCK_METHOD2(GenSyncTokenCHROMIUM, void(GLuint64, GLbyte*));
  MOCK_METHOD2(GenUnverifiedSyncTokenCHROMIUM, void(GLuint64, GLbyte*));
  MOCK_METHOD0(InsertFenceSyncCHROMIUM, GLuint64(void));
};

class MailboxSharedGpuContextTest
    : public SharedGpuContextTestBase<MailboxMockGLES2Interface> {};

// Test fixure that simulate a graphics context creation failure
class BadSharedGpuContextTest : public Test {
 public:
  void SetUp() override {
    SharedGpuContext::SetContextProviderFactoryForTesting(
        [] { return std::unique_ptr<WebGraphicsContext3DProvider>(nullptr); });
  }

  void TearDown() override {
    SharedGpuContext::SetContextProviderFactoryForTesting(nullptr);
  }
};

TEST_F(SharedGpuContextTest, contextLossAutoRecovery) {
  EXPECT_NE(SharedGpuContext::ContextProviderWrapper(), nullptr);
  WeakPtr<WebGraphicsContext3DProviderWrapper> context =
      SharedGpuContext::ContextProviderWrapper();
  gl_.SetIsContextLost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
  EXPECT_TRUE(!!context);

  // Context recreation results in old provider being discarded.
  EXPECT_TRUE(!!SharedGpuContext::ContextProviderWrapper());
  EXPECT_FALSE(!!context);
}

TEST_F(SharedGpuContextTest, AccelerateImageBufferSurfaceAutoRecovery) {
  // Verifies that after a context loss, attempting to allocate an
  // AcceleratedImageBufferSurface will restore the context and succeed
  gl_.SetIsContextLost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
  IntSize size(10, 10);
  std::unique_ptr<ImageBufferSurface> surface =
      WTF::WrapUnique(new AcceleratedImageBufferSurface(size));
  EXPECT_TRUE(surface->IsValid());
  EXPECT_TRUE(SharedGpuContext::IsValidWithoutRestoring());
}

TEST_F(SharedGpuContextTest, Canvas2DLayerBridgeAutoRecovery) {
  // Verifies that after a context loss, attempting to allocate a
  // Canvas2DLayerBridge will restore the context and succeed.
  gl_.SetIsContextLost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
  IntSize size(10, 10);
  CanvasColorParams color_params;
  std::unique_ptr<Canvas2DLayerBridge> bridge = WTF::WrapUnique(
      new Canvas2DLayerBridge(size, 0, /*msaa sample count*/
                              Canvas2DLayerBridge::kEnableAcceleration,
                              color_params, IsUnitTest()));
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_TRUE(SharedGpuContext::IsValidWithoutRestoring());
  bridge->BeginDestruction();
}

TEST_F(SharedGpuContextTest, IsValidWithoutRestoring) {
  EXPECT_NE(SharedGpuContext::ContextProviderWrapper(), nullptr);
  EXPECT_TRUE(SharedGpuContext::IsValidWithoutRestoring());
}

TEST_F(BadSharedGpuContextTest, IsValidWithoutRestoring) {
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
}

TEST_F(BadSharedGpuContextTest, AllowSoftwareToAcceleratedCanvasUpgrade) {
  EXPECT_FALSE(SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade());
}

TEST_F(BadSharedGpuContextTest, AccelerateImageBufferSurfaceCreationFails) {
  // With a bad shared context, AccelerateImageBufferSurface creation should
  // fail gracefully
  IntSize size(10, 10);
  std::unique_ptr<ImageBufferSurface> surface =
      WTF::WrapUnique(new AcceleratedImageBufferSurface(size));
  EXPECT_FALSE(surface->IsValid());
}

class FakeMailboxGenerator {
 public:
  void GenMailbox(GLbyte* name) { *name = counter_++; }

  GLbyte counter_ = 1;
};

TEST_F(MailboxSharedGpuContextTest, MailboxCaching) {
  IntSize size(10, 10);
  std::unique_ptr<ImageBufferSurface> surface =
      WTF::WrapUnique(new AcceleratedImageBufferSurface(size));
  EXPECT_TRUE(surface->IsValid());
  RefPtr<StaticBitmapImage> image =
      surface->NewImageSnapshot(kPreferAcceleration, kSnapshotReasonUnitTests);
  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  FakeMailboxGenerator mailboxGenerator;
  gpu::Mailbox mailbox;
  mailbox.name[0] = 0;

  EXPECT_CALL(gl_, GenMailboxCHROMIUM(mailbox.name))
      .Times(1)
      .WillOnce(::testing::Invoke(&mailboxGenerator,
                                  &FakeMailboxGenerator::GenMailbox));

  SharedGpuContext::ContextProviderWrapper()->Utils()->GetMailboxForSkImage(
      mailbox, image->PaintImageForCurrentFrame().GetSkImage());

  EXPECT_EQ(mailbox.name[0], 1);

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, GenMailboxCHROMIUM(mailbox.name))
      .Times(0);  // GenMailboxCHROMIUM must not be called!

  mailbox.name[0] = 0;
  SharedGpuContext::ContextProviderWrapper()->Utils()->GetMailboxForSkImage(
      mailbox, image->PaintImageForCurrentFrame().GetSkImage());
  EXPECT_EQ(mailbox.name[0], 1);

  ::testing::Mock::VerifyAndClearExpectations(&gl_);
}

TEST_F(MailboxSharedGpuContextTest, MailboxCacheSurvivesSkiaRecycling) {
  IntSize size(10, 10);
  std::unique_ptr<ImageBufferSurface> surface =
      WTF::WrapUnique(new AcceleratedImageBufferSurface(size));
  EXPECT_TRUE(surface->IsValid());
  RefPtr<StaticBitmapImage> image =
      surface->NewImageSnapshot(kPreferAcceleration, kSnapshotReasonUnitTests);
  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  FakeMailboxGenerator mailboxGenerator;
  gpu::Mailbox mailbox;
  mailbox.name[0] = 0;

  EXPECT_CALL(gl_, GenMailboxCHROMIUM(mailbox.name))
      .Times(1)
      .WillOnce(::testing::Invoke(&mailboxGenerator,
                                  &FakeMailboxGenerator::GenMailbox));

  SharedGpuContext::ContextProviderWrapper()->Utils()->GetMailboxForSkImage(
      mailbox, image->PaintImageForCurrentFrame().GetSkImage());

  EXPECT_EQ(mailbox.name[0], 1);
  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  // Destroy image and surface to return texture to recleable resource pool
  image = nullptr;
  surface = nullptr;

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  // Re-creating surface should recycle the old GrTexture inside skia
  surface = WTF::WrapUnique(new AcceleratedImageBufferSurface(size));
  EXPECT_TRUE(surface->IsValid());
  image =
      surface->NewImageSnapshot(kPreferAcceleration, kSnapshotReasonUnitTests);

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, GenMailboxCHROMIUM(mailbox.name))
      .Times(0);  // GenMailboxCHROMIUM must not be called!

  mailbox.name[0] = 0;
  SharedGpuContext::ContextProviderWrapper()->Utils()->GetMailboxForSkImage(
      mailbox, image->PaintImageForCurrentFrame().GetSkImage());
  EXPECT_EQ(mailbox.name[0], 1);

  ::testing::Mock::VerifyAndClearExpectations(&gl_);
}

}  // unnamed namespace

}  // blink
