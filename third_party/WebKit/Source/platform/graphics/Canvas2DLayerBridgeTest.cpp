/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "platform/graphics/Canvas2DLayerBridge.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/viz/common/quads/texture_mailbox.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "skia/ext/texture_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

#include <memory>

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::Test;
using ::testing::_;

namespace blink {

namespace {

class Canvas2DLayerBridgePtr {
 public:
  Canvas2DLayerBridgePtr() {}
  Canvas2DLayerBridgePtr(std::unique_ptr<Canvas2DLayerBridge> layer_bridge)
      : layer_bridge_(std::move(layer_bridge)) {}

  ~Canvas2DLayerBridgePtr() { Clear(); }

  void Clear() {
    if (layer_bridge_) {
      layer_bridge_->BeginDestruction();
      layer_bridge_.reset();
    }
  }

  void operator=(std::unique_ptr<Canvas2DLayerBridge> layer_bridge) {
    DCHECK(!layer_bridge_);
    layer_bridge_ = std::move(layer_bridge);
  }

  Canvas2DLayerBridge* operator->() { return layer_bridge_.get(); }
  Canvas2DLayerBridge* Get() { return layer_bridge_.get(); }

 private:
  std::unique_ptr<Canvas2DLayerBridge> layer_bridge_;
};

class FakeGLES2InterfaceWithImageSupport : public FakeGLES2Interface {
 public:
  MOCK_METHOD0(Flush, void());

  GLuint CreateImageCHROMIUM(ClientBuffer, GLsizei, GLsizei, GLenum) override {
    return ++create_image_count_;
  }
  void DestroyImageCHROMIUM(GLuint) override { ++destroy_image_count_; }

  GLuint CreateImageCount() { return create_image_count_; }
  GLuint DestroyImageCount() { return destroy_image_count_; }

 private:
  GLuint create_image_count_ = 0;
  GLuint destroy_image_count_ = 0;
};

class FakePlatformSupport : public TestingPlatformSupport {
 public:
  void RunUntilStop() const { base::RunLoop().Run(); }

  void StopRunning() const { base::RunLoop().Quit(); }

 private:
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override {
    return &test_gpu_memory_buffer_manager_;
  }

  viz::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager_;
};

}  // anonymous namespace

class Canvas2DLayerBridgeTest : public Test {
 public:
  std::unique_ptr<Canvas2DLayerBridge> MakeBridge(
      const IntSize& size,
      Canvas2DLayerBridge::AccelerationMode acceleration_mode) {
    std::unique_ptr<Canvas2DLayerBridge> bridge =
        WTF::WrapUnique(new Canvas2DLayerBridge(
            size, 0, acceleration_mode, CanvasColorParams(), IsUnitTest()));
    bridge->DontUseIdleSchedulingForTesting();
    return bridge;
  }
  void SetUp() override {
    SharedGpuContext::SetContextProviderFactoryForTesting([this] {
      return std::unique_ptr<WebGraphicsContext3DProvider>(
          new FakeWebGraphicsContext3DProvider(&gl_));
    });
  }
  void TearDown() override {
    SharedGpuContext::SetContextProviderFactoryForTesting(nullptr);
  }
  bool IsUnitTest() { return true; }

 protected:
  FakeGLES2InterfaceWithImageSupport gl_;

  void FullLifecycleTest() {
    Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
        IntSize(300, 150), 0, Canvas2DLayerBridge::kDisableAcceleration,
        CanvasColorParams(), IsUnitTest())));

    const GrGLTextureInfo* texture_info =
        skia::GrBackendObjectToGrGLTextureInfo(
            bridge
                ->NewImageSnapshot(kPreferAcceleration,
                                   kSnapshotReasonUnitTests)
                ->PaintImageForCurrentFrame()
                .GetSkImage()
                ->getTextureHandle(true));
    EXPECT_EQ(texture_info, nullptr);
    bridge.Clear();
  }

  void FallbackToSoftwareIfContextLost() {
    gl_.SetIsContextLost(true);
    Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
        IntSize(300, 150), 0, Canvas2DLayerBridge::kEnableAcceleration,
        CanvasColorParams(), IsUnitTest())));
    EXPECT_TRUE(bridge->IsValid());
    EXPECT_FALSE(bridge->IsAccelerated());
  }

  void FallbackToSoftwareOnFailedTextureAlloc() {
    {
      // No fallback case.
      Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
          IntSize(300, 150), 0, Canvas2DLayerBridge::kEnableAcceleration,
          CanvasColorParams(), IsUnitTest())));
      EXPECT_TRUE(bridge->IsValid());
      EXPECT_TRUE(bridge->IsAccelerated());
      RefPtr<StaticBitmapImage> snapshot = bridge->NewImageSnapshot(
          kPreferAcceleration, kSnapshotReasonUnitTests);
      EXPECT_TRUE(bridge->IsAccelerated());
      EXPECT_TRUE(snapshot->IsTextureBacked());
    }

    {
      // Fallback case.
      GrContext* gr = SharedGpuContext::ContextProviderWrapper()
                          ->ContextProvider()
                          ->GetGrContext();
      Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
          IntSize(300, 150), 0, Canvas2DLayerBridge::kEnableAcceleration,
          CanvasColorParams(), IsUnitTest())));
      EXPECT_TRUE(bridge->IsValid());
      EXPECT_TRUE(bridge->IsAccelerated());  // We don't yet know that
                                             // allocation will fail.
      // This will cause SkSurface_Gpu creation to fail without
      // Canvas2DLayerBridge otherwise detecting that anything was disabled.
      gr->abandonContext();
      RefPtr<StaticBitmapImage> snapshot = bridge->NewImageSnapshot(
          kPreferAcceleration, kSnapshotReasonUnitTests);
      EXPECT_FALSE(bridge->IsAccelerated());
      EXPECT_FALSE(snapshot->IsTextureBacked());
    }
  }

  void NoDrawOnContextLostTest() {
    Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
        IntSize(300, 150), 0, Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams(), IsUnitTest())));
    EXPECT_TRUE(bridge->IsValid());
    PaintFlags flags;
    uint32_t gen_id = bridge->GetOrCreateSurface()->generationID();
    bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
    EXPECT_EQ(gen_id, bridge->GetOrCreateSurface()->generationID());
    gl_.SetIsContextLost(true);
    EXPECT_EQ(gen_id, bridge->GetOrCreateSurface()->generationID());
    bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
    EXPECT_EQ(gen_id, bridge->GetOrCreateSurface()->generationID());
    // This results in the internal surface being torn down in response to the
    // context loss.
    EXPECT_FALSE(bridge->IsValid());
    EXPECT_EQ(nullptr, bridge->GetOrCreateSurface());
    // The following passes by not crashing
    bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
    bridge->Flush(kFlushReasonUnknown);
  }

  void PrepareMailboxWhenContextIsLost() {
    Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
        IntSize(300, 150), 0, Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams(), IsUnitTest())));

    EXPECT_TRUE(bridge->IsAccelerated());

    // When the context is lost we are not sure if we should still be producing
    // GL frames for the compositor or not, so fail to generate frames.
    gl_.SetIsContextLost(true);

    viz::TextureMailbox texture_mailbox;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;
    EXPECT_FALSE(
        bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback));
  }

  void PrepareMailboxWhenContextIsLostWithFailedRestore() {
    Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
        IntSize(300, 150), 0, Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams(), IsUnitTest())));

    bridge->GetOrCreateSurface();
    EXPECT_TRUE(bridge->IsValid());
    // When the context is lost we are not sure if we should still be producing
    // GL frames for the compositor or not, so fail to generate frames.
    gl_.SetIsContextLost(true);
    EXPECT_FALSE(bridge->IsValid());

    // Restoration will fail because
    // Platform::createSharedOffscreenGraphicsContext3DProvider() is stubbed
    // in unit tests.  This simulates what would happen when attempting to
    // restore while the GPU process is down.
    bridge->Restore();

    viz::TextureMailbox texture_mailbox;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;
    EXPECT_FALSE(
        bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback));
  }

  void ReleaseCallbackWithNullContextProviderWrapperTest() {
    viz::TextureMailbox texture_mailbox;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;

    {
      Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
          IntSize(300, 150), 0,
          Canvas2DLayerBridge::kForceAccelerationForTesting,
          CanvasColorParams())));
      EXPECT_TRUE(
          bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback));
    }

    bool lost_resource = true;
    gl_.SetIsContextLost(true);
    // Get a new context provider so that the WeakPtr to the old one is null.
    // This is the test to make sure that ReleaseMailboxImageResource() handles
    // null context_provider_wrapper properly.
    SharedGpuContext::ContextProviderWrapper();
    release_callback->Run(gpu::SyncToken(), lost_resource);
  }

  void PrepareMailboxAndLoseResourceTest() {
    // Prepare a mailbox, then report the resource as lost.
    // This test passes by not crashing and not triggering assertions.
    {
      Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
          IntSize(300, 150), 0,
          Canvas2DLayerBridge::kForceAccelerationForTesting,
          CanvasColorParams(), IsUnitTest())));

      viz::TextureMailbox texture_mailbox;
      std::unique_ptr<viz::SingleReleaseCallback> release_callback;
      EXPECT_TRUE(
          bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback));

      bool lost_resource = true;
      release_callback->Run(gpu::SyncToken(), lost_resource);
    }

    // Retry with mailbox released while bridge destruction is in progress.
    {
      viz::TextureMailbox texture_mailbox;
      std::unique_ptr<viz::SingleReleaseCallback> release_callback;

      {
        Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
            IntSize(300, 150), 0,
            Canvas2DLayerBridge::kForceAccelerationForTesting,
            CanvasColorParams(), IsUnitTest())));
        bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback);
        // |bridge| goes out of scope and would normally be destroyed, but
        // object is kept alive by self references.
      }

      // This should cause the bridge to be destroyed.
      bool lost_resource = true;
      // Before fixing crbug.com/411864, the following line would cause a memory
      // use after free that sometimes caused a crash in normal builds and
      // crashed consistently with ASAN.
      release_callback->Run(gpu::SyncToken(), lost_resource);
    }
  }

  void AccelerationHintTest() {
    {
      Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
          IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
          CanvasColorParams(), IsUnitTest())));
      PaintFlags flags;
      bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
      RefPtr<StaticBitmapImage> image = bridge->NewImageSnapshot(
          kPreferAcceleration, kSnapshotReasonUnitTests);
      EXPECT_TRUE(bridge->IsValid());
      EXPECT_TRUE(bridge->IsAccelerated());
    }

    {
      Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
          IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
          CanvasColorParams(), IsUnitTest())));
      PaintFlags flags;
      bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
      RefPtr<StaticBitmapImage> image = bridge->NewImageSnapshot(
          kPreferNoAcceleration, kSnapshotReasonUnitTests);
      EXPECT_TRUE(bridge->IsValid());
      EXPECT_FALSE(bridge->IsAccelerated());
    }
  }
};

TEST_F(Canvas2DLayerBridgeTest, FullLifecycleSingleThreaded) {
  FullLifecycleTest();
}

TEST_F(Canvas2DLayerBridgeTest, NoDrawOnContextLost) {
  NoDrawOnContextLostTest();
}

TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhenContextIsLost) {
  PrepareMailboxWhenContextIsLost();
}

TEST_F(Canvas2DLayerBridgeTest,
       PrepareMailboxWhenContextIsLostWithFailedRestore) {
  PrepareMailboxWhenContextIsLostWithFailedRestore();
}

TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxAndLoseResource) {
  PrepareMailboxAndLoseResourceTest();
}

TEST_F(Canvas2DLayerBridgeTest, ReleaseCallbackWithNullContextProviderWrapper) {
  ReleaseCallbackWithNullContextProviderWrapperTest();
}

TEST_F(Canvas2DLayerBridgeTest, AccelerationHint) {
  AccelerationHintTest();
}

TEST_F(Canvas2DLayerBridgeTest, FallbackToSoftwareIfContextLost) {
  FallbackToSoftwareIfContextLost();
}

TEST_F(Canvas2DLayerBridgeTest, FallbackToSoftwareOnFailedTextureAlloc) {
  FallbackToSoftwareOnFailedTextureAlloc();
}

class MockLogger : public Canvas2DLayerBridge::Logger {
 public:
  MOCK_METHOD1(ReportHibernationEvent,
               void(Canvas2DLayerBridge::HibernationEvent));
  MOCK_METHOD0(DidStartHibernating, void());
  virtual ~MockLogger() {}
};

class MockImageBuffer : public ImageBuffer {
 public:
  MockImageBuffer()
      : ImageBuffer(WTF::WrapUnique(
            new UnacceleratedImageBufferSurface(IntSize(1, 1)))) {}

  MOCK_CONST_METHOD1(ResetCanvas, void(PaintCanvas*));

  virtual ~MockImageBuffer() {}
};

void DrawSomething(Canvas2DLayerBridgePtr& bridge) {
  bridge->DidDraw(FloatRect(0, 0, 1, 1));
  bridge->FinalizeFrame();
  bridge->Flush(kFlushReasonUnknown);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationLifeCycle)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationLifeCycle)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);

  bridge->SetIsHidden(true);
  platform->RunUntilIdle();

  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally));

  bridge->SetIsHidden(false);

  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationReEntry)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationReEntry)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  // Toggle visibility before the task that enters hibernation gets a
  // chance to run.
  bridge->SetIsHidden(false);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();

  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally));

  bridge->SetIsHidden(false);

  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest,
       HibernationLifeCycleWithDeferredRenderingDisabled)
#else
TEST_F(Canvas2DLayerBridgeTest,
       DISABLED_HibernationLifeCycleWithDeferredRenderingDisabled)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);
  bridge->DisableDeferral(kDisableDeferralReasonUnknown);
  MockImageBuffer mock_image_buffer;
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AnyNumber());
  bridge->SetImageBuffer(&mock_image_buffer);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally));
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_))
      .Times(AtLeast(1));  // Because deferred rendering is disabled
  bridge->SetIsHidden(false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest, BackgroundRenderingWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_BackgroundRenderingWhileHibernating)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  DrawSomething(bridge);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Unhide
  bridge->SetIsHidden(false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_TRUE(
      bridge->IsAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest,
       BackgroundRenderingWhileHibernatingWithDeferredRenderingDisabled)
#else
TEST_F(
    Canvas2DLayerBridgeTest,
    DISABLED_BackgroundRenderingWhileHibernatingWithDeferredRenderingDisabled)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);
  MockImageBuffer mock_image_buffer;
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AnyNumber());
  bridge->SetImageBuffer(&mock_image_buffer);
  bridge->DisableDeferral(kDisableDeferralReasonUnknown);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AtLeast(1));
  DrawSomething(bridge);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Unhide
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AtLeast(1));
  bridge->SetIsHidden(false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_TRUE(
      bridge->IsAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest, DisableDeferredRenderingWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest,
       DISABLED_DisableDeferredRenderingWhileHibernating)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);

  MockImageBuffer mock_image_buffer;
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AnyNumber());
  bridge->SetImageBuffer(&mock_image_buffer);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Disable deferral while background rendering
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AtLeast(1));
  bridge->DisableDeferral(kDisableDeferralReasonUnknown);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Unhide
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AtLeast(1));
  bridge->SetIsHidden(false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_TRUE(
      bridge->IsAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AnyNumber());
  bridge.Clear();
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, TeardownWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_TeardownWhileHibernating)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Tear down the bridge while hibernating
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::kHibernationEndedWithTeardown));
  bridge.Clear();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, SnapshotWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_SnapshotWhileHibernating)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Take a snapshot and verify that it is not accelerated due to hibernation
  RefPtr<StaticBitmapImage> image =
      bridge->NewImageSnapshot(kPreferAcceleration, kSnapshotReasonUnitTests);
  EXPECT_FALSE(image->IsTextureBacked());
  image = nullptr;

  // Verify that taking a snapshot did not affect the state of bridge
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // End hibernation normally
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally))
      .Times(1);
  bridge->SetIsHidden(false);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, TeardownWhileHibernationIsPending)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_TeardownWhileHibernationIsPending)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  bridge->SetIsHidden(true);
  bridge.Clear();
  // In production, we would expect a
  // HibernationAbortedDueToDestructionWhileHibernatePending event to be
  // fired, but that signal is lost in the unit test due to no longer having
  // a bridge to hold the mockLogger.
  platform->RunUntilIdle();
  // This test passes by not crashing, which proves that the WeakPtr logic
  // is sound.
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToPendingTeardown)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationAbortedDueToPendingTeardown)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(
          Canvas2DLayerBridge::kHibernationAbortedDueToPendingDestruction))
      .Times(1);
  bridge->SetIsHidden(true);
  bridge->BeginDestruction();
  platform->RunUntilIdle();

  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToVisibilityChange)
#else
TEST_F(Canvas2DLayerBridgeTest,
       DISABLED_HibernationAbortedDueToVisibilityChange)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(
          Canvas2DLayerBridge::kHibernationAbortedDueToVisibilityChange))
      .Times(1);
  bridge->SetIsHidden(true);
  bridge->SetIsHidden(false);
  platform->RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToLostContext)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationAbortedDueToLostContext)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  gl_.SetIsContextLost(true);

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::kHibernationAbortedDueGpuContextLoss))
      .Times(1);

  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsHibernating());
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_PrepareMailboxWhileHibernating)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);

  // Test prepareMailbox while hibernating
  viz::TextureMailbox texture_mailbox;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  EXPECT_FALSE(
      bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback));
  EXPECT_TRUE(bridge->IsValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::kHibernationEndedWithTeardown));
  bridge.Clear();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhileBackgroundRendering)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_PrepareMailboxWhileBackgroundRendering)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams(), IsUnitTest())));
  DrawSomething(bridge);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_started_event =
      std::make_unique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  DrawSomething(bridge);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Test prepareMailbox while background rendering
  viz::TextureMailbox texture_mailbox;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  EXPECT_FALSE(
      bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback));
  EXPECT_TRUE(bridge->IsValid());
}

TEST_F(Canvas2DLayerBridgeTest, DeleteGpuMemoryBufferAfterTeardown) {
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
  RuntimeEnabledFeatures::SetCanvas2dImageChromiumEnabled(true);
#endif
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;

  viz::TextureMailbox texture_mailbox;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;

  {
    Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
        IntSize(300, 150), 0, Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams(), IsUnitTest())));
    bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback);
  }

  bool lost_resource = false;
  release_callback->Run(gpu::SyncToken(), lost_resource);
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
  EXPECT_EQ(1u, gl_.CreateImageCount());
  EXPECT_EQ(1u, gl_.DestroyImageCount());
#else
  EXPECT_EQ(0u, gl_.CreateImageCount());
  EXPECT_EQ(0u, gl_.DestroyImageCount());
#endif
}

TEST_F(Canvas2DLayerBridgeTest, NoUnnecessaryFlushes) {
  EXPECT_CALL(gl_, Flush()).Times(0);
  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 150), 0, Canvas2DLayerBridge::kForceAccelerationForTesting,
      CanvasColorParams(), IsUnitTest())));
  EXPECT_FALSE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, Flush()).Times(0);
  bridge->DidDraw(FloatRect(0, 0, 1, 1));
  EXPECT_TRUE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, Flush()).Times(1);
  bridge->FlushGpu(kFlushReasonUnknown);
  EXPECT_FALSE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, Flush()).Times(0);
  bridge->DidDraw(FloatRect(0, 0, 1, 1));
  EXPECT_TRUE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, Flush()).Times(1);
  bridge->FlushGpu(kFlushReasonUnknown);
  EXPECT_FALSE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  // No flush because already flushed since last draw
  EXPECT_CALL(gl_, Flush()).Times(0);
  bridge->FlushGpu(kFlushReasonUnknown);
  EXPECT_FALSE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, Flush()).Times(0);
  bridge->DidDraw(FloatRect(0, 0, 1, 1));
  EXPECT_TRUE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  // Flushes recording, but not the gpu
  EXPECT_CALL(gl_, Flush()).Times(0);
  bridge->Flush(kFlushReasonUnknown);
  EXPECT_FALSE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, Flush()).Times(1);
  bridge->FlushGpu(kFlushReasonUnknown);
  EXPECT_FALSE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl_);
}

}  // namespace blink
