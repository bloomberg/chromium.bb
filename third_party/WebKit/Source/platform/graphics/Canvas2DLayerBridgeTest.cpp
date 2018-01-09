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

#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/CanvasResourceHost.h"
#include "platform/graphics/CanvasResourceProvider.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "skia/ext/texture_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

#include <memory>

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Test;
using ::testing::_;

namespace blink {

namespace {

class Canvas2DLayerBridgePtr {
 public:
  Canvas2DLayerBridgePtr() = default;
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
    DCHECK(!layer_bridge_);  // Existing ref must be removed with Clear()
    layer_bridge_ = std::move(layer_bridge);
  }

  Canvas2DLayerBridge* operator->() { return layer_bridge_.get(); }
  Canvas2DLayerBridge* Get() { return layer_bridge_.get(); }

 private:
  std::unique_ptr<Canvas2DLayerBridge> layer_bridge_;
};

class MockGLES2InterfaceWithImageSupport : public FakeGLES2Interface {
 public:
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD4(CreateImageCHROMIUM,
               GLuint(ClientBuffer, GLsizei, GLsizei, GLenum));
  MOCK_METHOD1(DestroyImageCHROMIUM, void(GLuint));
  MOCK_METHOD2(GenTextures, void(GLsizei, GLuint*));
  MOCK_METHOD2(DeleteTextures, void(GLsizei, const GLuint*));
  // Fake
  void GenMailboxCHROMIUM(GLbyte* name) {
    name[0] = 1;  // Make non-zero mailbox names
  }
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
        WTF::WrapUnique(new Canvas2DLayerBridge(size, 0, acceleration_mode,
                                                CanvasColorParams()));
    bridge->DontUseIdleSchedulingForTesting();
    return bridge;
  }
  void SetUp() override {
    auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
  }
  void TearDown() override { SharedGpuContext::ResetForTesting(); }

 protected:
  MockGLES2InterfaceWithImageSupport gl_;

  void FullLifecycleTest() {
    Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
        IntSize(300, 150), 0, Canvas2DLayerBridge::kDisableAcceleration,
        CanvasColorParams())));

    const GrGLTextureInfo* texture_info =
        skia::GrBackendObjectToGrGLTextureInfo(
            bridge->NewImageSnapshot(kPreferAcceleration)
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
        CanvasColorParams())));
    EXPECT_TRUE(bridge->IsValid());
    EXPECT_FALSE(bridge->IsAccelerated());
  }

  void FallbackToSoftwareOnFailedTextureAlloc() {
    {
      // No fallback case.
      Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
          IntSize(300, 150), 0, Canvas2DLayerBridge::kEnableAcceleration,
          CanvasColorParams())));
      EXPECT_TRUE(bridge->IsValid());
      EXPECT_TRUE(bridge->IsAccelerated());
      scoped_refptr<StaticBitmapImage> snapshot =
          bridge->NewImageSnapshot(kPreferAcceleration);
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
          CanvasColorParams())));
      EXPECT_TRUE(bridge->IsValid());
      EXPECT_TRUE(bridge->IsAccelerated());  // We don't yet know that
                                             // allocation will fail.
      // This will cause SkSurface_Gpu creation to fail without
      // Canvas2DLayerBridge otherwise detecting that anything was disabled.
      gr->abandonContext();
      scoped_refptr<StaticBitmapImage> snapshot =
          bridge->NewImageSnapshot(kPreferAcceleration);
      EXPECT_FALSE(bridge->IsAccelerated());
      EXPECT_FALSE(snapshot->IsTextureBacked());
    }
  }

  void NoDrawOnContextLostTest() {
    Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
        IntSize(300, 150), 0, Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams())));
    EXPECT_TRUE(bridge->IsValid());
    PaintFlags flags;
    uint32_t gen_id = bridge->GetOrCreateResourceProvider()->ContentUniqueID();
    bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
    EXPECT_EQ(gen_id, bridge->GetOrCreateResourceProvider()->ContentUniqueID());
    gl_.SetIsContextLost(true);
    EXPECT_EQ(gen_id, bridge->GetOrCreateResourceProvider()->ContentUniqueID());
    bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
    EXPECT_EQ(gen_id, bridge->GetOrCreateResourceProvider()->ContentUniqueID());
    // This results in the internal surface being torn down in response to the
    // context loss.
    EXPECT_FALSE(bridge->IsValid());
    EXPECT_EQ(nullptr, bridge->GetOrCreateResourceProvider());
    // The following passes by not crashing
    bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
    bridge->NewImageSnapshot(kPreferAcceleration);
  }

  void PrepareMailboxWhenContextIsLost() {
    Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
        IntSize(300, 150), 0, Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams())));

    EXPECT_TRUE(bridge->IsAccelerated());
    bridge->FinalizeFrame();  // Trigger the creation of a backing store
    // When the context is lost we are not sure if we should still be producing
    // GL frames for the compositor or not, so fail to generate frames.
    gl_.SetIsContextLost(true);

    viz::TransferableResource resource;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;
    EXPECT_FALSE(
        bridge->PrepareTransferableResource(&resource, &release_callback));
  }

  void PrepareMailboxWhenContextIsLostWithFailedRestore() {
    Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
        IntSize(300, 150), 0, Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams())));

    bridge->GetOrCreateResourceProvider();
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

    viz::TransferableResource resource;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;
    EXPECT_FALSE(
        bridge->PrepareTransferableResource(&resource, &release_callback));
  }

  void ReleaseCallbackWithNullContextProviderWrapperTest() {
    viz::TransferableResource resource;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;

    {
      Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
          IntSize(300, 150), 0,
          Canvas2DLayerBridge::kForceAccelerationForTesting,
          CanvasColorParams())));
      bridge->FinalizeFrame();
      EXPECT_TRUE(
          bridge->PrepareTransferableResource(&resource, &release_callback));
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
          CanvasColorParams())));
      bridge->FinalizeFrame();
      viz::TransferableResource resource;
      std::unique_ptr<viz::SingleReleaseCallback> release_callback;
      EXPECT_TRUE(
          bridge->PrepareTransferableResource(&resource, &release_callback));

      bool lost_resource = true;
      release_callback->Run(gpu::SyncToken(), lost_resource);
    }

    // Retry with mailbox released while bridge destruction is in progress.
    {
      viz::TransferableResource resource;
      std::unique_ptr<viz::SingleReleaseCallback> release_callback;

      {
        Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
            IntSize(300, 150), 0,
            Canvas2DLayerBridge::kForceAccelerationForTesting,
            CanvasColorParams())));
        bridge->FinalizeFrame();
        bridge->PrepareTransferableResource(&resource, &release_callback);
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
          CanvasColorParams())));
      PaintFlags flags;
      bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
      scoped_refptr<StaticBitmapImage> image =
          bridge->NewImageSnapshot(kPreferAcceleration);
      EXPECT_TRUE(bridge->IsValid());
      EXPECT_TRUE(bridge->IsAccelerated());
    }

    {
      Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
          IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
          CanvasColorParams())));
      PaintFlags flags;
      bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
      scoped_refptr<StaticBitmapImage> image =
          bridge->NewImageSnapshot(kPreferNoAcceleration);
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
  virtual ~MockLogger() = default;
};

class MockCanvasResourceHost : public CanvasResourceHost {
 public:
  void NotifySurfaceInvalid() {}
  void SetNeedsCompositingUpdate() {}
  void UpdateMemoryUsage() {}
  MOCK_CONST_METHOD1(RestoreCanvasMatrixClipStack, void(PaintCanvas*));
};

void DrawSomething(Canvas2DLayerBridgePtr& bridge) {
  bridge->DidDraw(FloatRect(0, 0, 1, 1));
  bridge->FinalizeFrame();
  // Grabbing an image forces a flush
  bridge->NewImageSnapshot(kPreferAcceleration);
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
      CanvasColorParams())));
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
      CanvasColorParams())));
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
  MockCanvasResourceHost mock_host;
  EXPECT_CALL(mock_host, RestoreCanvasMatrixClipStack(_)).Times(AnyNumber());

  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 300), 0, Canvas2DLayerBridge::kEnableAcceleration,
      CanvasColorParams())));

  bridge->SetCanvasResourceHost(&mock_host);
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);
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
  ::testing::Mock::VerifyAndClearExpectations(&mock_host);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally));
  EXPECT_CALL(mock_host, RestoreCanvasMatrixClipStack(_))
      .Times(AtLeast(1));  // Because deferred rendering is disabled
  bridge->SetIsHidden(false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_host);
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
      CanvasColorParams())));
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
      CanvasColorParams())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);
  MockCanvasResourceHost mock_canvas_resource_host;
  EXPECT_CALL(mock_canvas_resource_host, RestoreCanvasMatrixClipStack(_))
      .Times(AnyNumber());
  bridge->SetCanvasResourceHost(&mock_canvas_resource_host);
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
  ::testing::Mock::VerifyAndClearExpectations(&mock_canvas_resource_host);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  EXPECT_CALL(mock_canvas_resource_host, RestoreCanvasMatrixClipStack(_))
      .Times(AtLeast(1));
  DrawSomething(bridge);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_canvas_resource_host);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Unhide
  EXPECT_CALL(mock_canvas_resource_host, RestoreCanvasMatrixClipStack(_))
      .Times(AtLeast(1));
  bridge->SetIsHidden(false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_canvas_resource_host);
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
      CanvasColorParams())));
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge);

  MockCanvasResourceHost mock_canvas_resource_host;
  EXPECT_CALL(mock_canvas_resource_host, RestoreCanvasMatrixClipStack(_))
      .Times(AnyNumber());
  bridge->SetCanvasResourceHost(&mock_canvas_resource_host);

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
  ::testing::Mock::VerifyAndClearExpectations(&mock_canvas_resource_host);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Disable deferral while background rendering
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  EXPECT_CALL(mock_canvas_resource_host, RestoreCanvasMatrixClipStack(_))
      .Times(AtLeast(1));
  bridge->DisableDeferral(kDisableDeferralReasonUnknown);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_canvas_resource_host);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Unhide
  EXPECT_CALL(mock_canvas_resource_host, RestoreCanvasMatrixClipStack(_))
      .Times(AtLeast(1));
  bridge->SetIsHidden(false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_canvas_resource_host);
  EXPECT_TRUE(
      bridge->IsAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  EXPECT_CALL(mock_canvas_resource_host, RestoreCanvasMatrixClipStack(_))
      .Times(AnyNumber());
  bridge.Clear();
  ::testing::Mock::VerifyAndClearExpectations(&mock_canvas_resource_host);
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
      CanvasColorParams())));
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
      CanvasColorParams())));
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
  scoped_refptr<StaticBitmapImage> image =
      bridge->NewImageSnapshot(kPreferAcceleration);
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
      CanvasColorParams())));
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
      CanvasColorParams())));
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
      CanvasColorParams())));
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
      CanvasColorParams())));
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
      CanvasColorParams())));
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

  // Test PrepareTransferableResource() while hibernating
  viz::TransferableResource resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  EXPECT_FALSE(
      bridge->PrepareTransferableResource(&resource, &release_callback));
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
      CanvasColorParams())));
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
  viz::TransferableResource resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  EXPECT_FALSE(
      bridge->PrepareTransferableResource(&resource, &release_callback));
  EXPECT_TRUE(bridge->IsValid());
}

TEST_F(Canvas2DLayerBridgeTest, GpuMemoryBufferRecycling) {
  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(true);
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  const_cast<gpu::Capabilities&>(SharedGpuContext::ContextProviderWrapper()
                                     ->ContextProvider()
                                     ->GetCapabilities())
      .texture_format_bgra8888 = true;

  viz::TransferableResource resource1;
  viz::TransferableResource resource2;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback1;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback2;
  const GLuint texture_id1 = 1;
  const GLuint texture_id2 = 2;
  const GLuint image_id1 = 3;
  const GLuint image_id2 = 4;

  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 150), 0, Canvas2DLayerBridge::kForceAccelerationForTesting,
      CanvasColorParams())));

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, GenTextures(1, _)).WillOnce(SetArgPointee<1>(texture_id1));
  EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _)).WillOnce(Return(image_id1));
  DrawSomething(bridge);
  bridge->PrepareTransferableResource(&resource1, &release_callback1);

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, GenTextures(1, _)).WillOnce(SetArgPointee<1>(texture_id2));
  EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _)).WillOnce(Return(image_id2));
  DrawSomething(bridge);
  bridge->PrepareTransferableResource(&resource2, &release_callback2);

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  // Check that release resources does not result in destruction due
  // to recycling.
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(_)).Times(0);
  EXPECT_CALL(gl_, DeleteTextures(_, _)).Times(0);
  bool lost_resource = false;
  release_callback1->Run(gpu::SyncToken(), lost_resource);
  release_callback1 = nullptr;

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, DestroyImageCHROMIUM(_)).Times(0);
  EXPECT_CALL(gl_, DeleteTextures(_, _)).Times(0);
  release_callback2->Run(gpu::SyncToken(), lost_resource);
  release_callback2 = nullptr;

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  // Destroying the bridge results in destruction of cached resources.
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_id1)).Times(1);
  EXPECT_CALL(gl_, DeleteTextures(1, Pointee(texture_id1))).Times(1);
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_id2)).Times(1);
  EXPECT_CALL(gl_, DeleteTextures(1, Pointee(texture_id2))).Times(1);
  bridge.Clear();

  ::testing::Mock::VerifyAndClearExpectations(&gl_);
}

TEST_F(Canvas2DLayerBridgeTest, NoGpuMemoryBufferRecyclingWhenPageHidden) {
  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(true);
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  const_cast<gpu::Capabilities&>(SharedGpuContext::ContextProviderWrapper()
                                     ->ContextProvider()
                                     ->GetCapabilities())
      .texture_format_bgra8888 = true;

  viz::TransferableResource resource1;
  viz::TransferableResource resource2;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback1;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback2;
  const GLuint texture_id1 = 1;
  const GLuint texture_id2 = 2;
  const GLuint image_id1 = 3;
  const GLuint image_id2 = 4;

  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 150), 0, Canvas2DLayerBridge::kForceAccelerationForTesting,
      CanvasColorParams())));

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, GenTextures(1, _)).WillOnce(SetArgPointee<1>(texture_id1));
  EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _)).WillOnce(Return(image_id1));
  DrawSomething(bridge);
  bridge->PrepareTransferableResource(&resource1, &release_callback1);

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, GenTextures(1, _)).WillOnce(SetArgPointee<1>(texture_id2));
  EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _)).WillOnce(Return(image_id2));
  DrawSomething(bridge);
  bridge->PrepareTransferableResource(&resource2, &release_callback2);

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  // Release first frame to cache
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(_)).Times(0);
  EXPECT_CALL(gl_, DeleteTextures(_, _)).Times(0);
  bool lost_resource = false;
  release_callback1->Run(gpu::SyncToken(), lost_resource);
  release_callback1 = nullptr;

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  // Switching to Hidden frees cached resources immediately
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_id1)).Times(1);
  EXPECT_CALL(gl_, DeleteTextures(1, Pointee(texture_id1))).Times(1);
  bridge->SetIsHidden(true);

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  // Release second frame and verify that its resource is destroyed immediately
  // due to the layer bridge being hidden
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_id2)).Times(1);
  EXPECT_CALL(gl_, DeleteTextures(1, Pointee(texture_id2))).Times(1);
  release_callback2->Run(gpu::SyncToken(), lost_resource);
  release_callback2 = nullptr;

  ::testing::Mock::VerifyAndClearExpectations(&gl_);
}

TEST_F(Canvas2DLayerBridgeTest, ReleaseGpuMemoryBufferAfterBridgeDestroyed) {
  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(true);
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  const_cast<gpu::Capabilities&>(SharedGpuContext::ContextProviderWrapper()
                                     ->ContextProvider()
                                     ->GetCapabilities())
      .texture_format_bgra8888 = true;

  viz::TransferableResource resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  const GLuint texture_id = 1;
  const GLuint image_id = 2;

  Canvas2DLayerBridgePtr bridge(WTF::WrapUnique(new Canvas2DLayerBridge(
      IntSize(300, 150), 0, Canvas2DLayerBridge::kForceAccelerationForTesting,
      CanvasColorParams())));

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, GenTextures(1, _)).WillOnce(SetArgPointee<1>(texture_id));
  EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _)).WillOnce(Return(image_id));
  DrawSomething(bridge);
  bridge->PrepareTransferableResource(&resource, &release_callback);

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  // Tearing down the bridge does not destroy unreleased resources.
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(_)).Times(0);
  EXPECT_CALL(gl_, DeleteTextures(_, _)).Times(0);
  bridge.Clear();

  ::testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_id)).Times(1);
  EXPECT_CALL(gl_, DeleteTextures(1, Pointee(texture_id))).Times(1);
  bool lost_resource = false;
  release_callback->Run(gpu::SyncToken(), lost_resource);
  release_callback = nullptr;

  ::testing::Mock::VerifyAndClearExpectations(&gl_);
}

}  // namespace blink
