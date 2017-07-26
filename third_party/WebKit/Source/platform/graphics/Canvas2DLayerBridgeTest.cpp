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

#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "components/viz/common/quads/single_release_callback.h"
#include "components/viz/common/quads/texture_mailbox.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/PtrUtil.h"
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
  Canvas2DLayerBridgePtr(PassRefPtr<Canvas2DLayerBridge> layer_bridge)
      : layer_bridge_(std::move(layer_bridge)) {}

  ~Canvas2DLayerBridgePtr() { Clear(); }

  void Clear() {
    if (layer_bridge_) {
      layer_bridge_->BeginDestruction();
      layer_bridge_.Clear();
    }
  }

  void operator=(PassRefPtr<Canvas2DLayerBridge> layer_bridge) {
    DCHECK(!layer_bridge_);
    layer_bridge_ = std::move(layer_bridge);
  }

  Canvas2DLayerBridge* operator->() { return layer_bridge_.Get(); }
  Canvas2DLayerBridge* Get() { return layer_bridge_.Get(); }

 private:
  RefPtr<Canvas2DLayerBridge> layer_bridge_;
};

class FakeGLES2InterfaceWithImageSupport : public FakeGLES2Interface {
 public:
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
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override {
    return &test_gpu_memory_buffer_manager_;
  }

 private:
  cc::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager_;
};

}  // anonymous namespace

class Canvas2DLayerBridgeTest : public Test {
 public:
  PassRefPtr<Canvas2DLayerBridge> MakeBridge(
      std::unique_ptr<FakeWebGraphicsContext3DProvider> provider,
      const IntSize& size,
      Canvas2DLayerBridge::AccelerationMode acceleration_mode) {
    RefPtr<Canvas2DLayerBridge> bridge = AdoptRef(
        new Canvas2DLayerBridge(std::move(provider), size, 0, kNonOpaque,
                                acceleration_mode, CanvasColorParams()));
    bridge->DontUseIdleSchedulingForTesting();
    return bridge;
  }

 protected:
  void FullLifecycleTest() {
    FakeGLES2Interface gl;
    std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
        WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));

    Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
        std::move(context_provider), IntSize(300, 150), 0, kNonOpaque,
        Canvas2DLayerBridge::kDisableAcceleration, CanvasColorParams())));

    const GrGLTextureInfo* texture_info =
        skia::GrBackendObjectToGrGLTextureInfo(
            bridge
                ->NewImageSnapshot(kPreferAcceleration,
                                   kSnapshotReasonUnitTests)
                ->getTextureHandle(true));
    EXPECT_EQ(texture_info, nullptr);
    bridge.Clear();
  }

  void FallbackToSoftwareIfContextLost() {
    FakeGLES2Interface gl;
    std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
        WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));

    gl.SetIsContextLost(true);
    Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
        std::move(context_provider), IntSize(300, 150), 0, kNonOpaque,
        Canvas2DLayerBridge::kEnableAcceleration, CanvasColorParams())));
    EXPECT_TRUE(bridge->CheckSurfaceValid());
    EXPECT_FALSE(bridge->IsAccelerated());
  }

  void FallbackToSoftwareOnFailedTextureAlloc() {
    {
      // No fallback case.
      FakeGLES2Interface gl;
      std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
          WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
      Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
          std::move(context_provider), IntSize(300, 150), 0, kNonOpaque,
          Canvas2DLayerBridge::kEnableAcceleration, CanvasColorParams())));
      EXPECT_TRUE(bridge->CheckSurfaceValid());
      EXPECT_TRUE(bridge->IsAccelerated());
      sk_sp<SkImage> snapshot = bridge->NewImageSnapshot(
          kPreferAcceleration, kSnapshotReasonUnitTests);
      EXPECT_TRUE(bridge->IsAccelerated());
      EXPECT_TRUE(snapshot->isTextureBacked());
    }

    {
      // Fallback case.
      FakeGLES2Interface gl;
      std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
          WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
      GrContext* gr = context_provider->GetGrContext();
      Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
          std::move(context_provider), IntSize(300, 150), 0, kNonOpaque,
          Canvas2DLayerBridge::kEnableAcceleration, CanvasColorParams())));
      EXPECT_TRUE(bridge->CheckSurfaceValid());
      EXPECT_TRUE(bridge->IsAccelerated());  // We don't yet know that
                                             // allocation will fail.
      // This will cause SkSurface_Gpu creation to fail without
      // Canvas2DLayerBridge otherwise detecting that anything was disabled.
      gr->abandonContext();
      sk_sp<SkImage> snapshot = bridge->NewImageSnapshot(
          kPreferAcceleration, kSnapshotReasonUnitTests);
      EXPECT_FALSE(bridge->IsAccelerated());
      EXPECT_FALSE(snapshot->isTextureBacked());
    }
  }

  void NoDrawOnContextLostTest() {
    FakeGLES2Interface gl;
    std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
        WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));

    Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
        std::move(context_provider), IntSize(300, 150), 0, kNonOpaque,
        Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams())));
    EXPECT_TRUE(bridge->CheckSurfaceValid());
    PaintFlags flags;
    uint32_t gen_id = bridge->GetOrCreateSurface()->generationID();
    bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
    EXPECT_EQ(gen_id, bridge->GetOrCreateSurface()->generationID());
    gl.SetIsContextLost(true);
    EXPECT_EQ(gen_id, bridge->GetOrCreateSurface()->generationID());
    bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
    EXPECT_EQ(gen_id, bridge->GetOrCreateSurface()->generationID());
    // This results in the internal surface being torn down in response to the
    // context loss.
    EXPECT_FALSE(bridge->CheckSurfaceValid());
    EXPECT_EQ(nullptr, bridge->GetOrCreateSurface());
    // The following passes by not crashing
    bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
    bridge->Flush();
  }

  void PrepareMailboxWhenContextIsLost() {
    FakeGLES2Interface gl;
    std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
        WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
    Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
        std::move(context_provider), IntSize(300, 150), 0, kNonOpaque,
        Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams())));

    EXPECT_TRUE(bridge->IsAccelerated());

    // When the context is lost we are not sure if we should still be producing
    // GL frames for the compositor or not, so fail to generate frames.
    gl.SetIsContextLost(true);

    viz::TextureMailbox texture_mailbox;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;
    EXPECT_FALSE(
        bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback));
  }

  void PrepareMailboxWhenContextIsLostWithFailedRestore() {
    FakeGLES2Interface gl;
    std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
        WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
    Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
        std::move(context_provider), IntSize(300, 150), 0, kNonOpaque,
        Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams())));

    bridge->GetOrCreateSurface();
    EXPECT_TRUE(bridge->CheckSurfaceValid());
    // When the context is lost we are not sure if we should still be producing
    // GL frames for the compositor or not, so fail to generate frames.
    gl.SetIsContextLost(true);
    EXPECT_FALSE(bridge->CheckSurfaceValid());

    // Restoration will fail because
    // Platform::createSharedOffscreenGraphicsContext3DProvider() is stubbed
    // in unit tests.  This simulates what would happen when attempting to
    // restore while the GPU process is down.
    bridge->RestoreSurface();

    viz::TextureMailbox texture_mailbox;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;
    EXPECT_FALSE(
        bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback));
  }

  void PrepareMailboxAndLoseResourceTest() {
    // Prepare a mailbox, then report the resource as lost.
    // This test passes by not crashing and not triggering assertions.
    {
      FakeGLES2Interface gl;
      std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
          WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
      Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
          std::move(context_provider), IntSize(300, 150), 0, kNonOpaque,
          Canvas2DLayerBridge::kForceAccelerationForTesting,
          CanvasColorParams())));

      viz::TextureMailbox texture_mailbox;
      std::unique_ptr<viz::SingleReleaseCallback> release_callback;
      EXPECT_TRUE(
          bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback));

      bool lost_resource = true;
      release_callback->Run(gpu::SyncToken(), lost_resource);
    }

    // Retry with mailbox released while bridge destruction is in progress.
    {
      FakeGLES2Interface gl;
      std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
          WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));

      viz::TextureMailbox texture_mailbox;
      std::unique_ptr<viz::SingleReleaseCallback> release_callback;

      {
        Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
            std::move(context_provider), IntSize(300, 150), 0, kNonOpaque,
            Canvas2DLayerBridge::kForceAccelerationForTesting,
            CanvasColorParams())));
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
      FakeGLES2Interface gl;
      std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
          WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
      Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
          std::move(context_provider), IntSize(300, 300), 0, kNonOpaque,
          Canvas2DLayerBridge::kEnableAcceleration, CanvasColorParams())));
      PaintFlags flags;
      bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
      sk_sp<SkImage> image = bridge->NewImageSnapshot(kPreferAcceleration,
                                                      kSnapshotReasonUnitTests);
      EXPECT_TRUE(bridge->CheckSurfaceValid());
      EXPECT_TRUE(bridge->IsAccelerated());
    }

    {
      FakeGLES2Interface gl;
      std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
          WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
      Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
          std::move(context_provider), IntSize(300, 300), 0, kNonOpaque,
          Canvas2DLayerBridge::kEnableAcceleration, CanvasColorParams())));
      PaintFlags flags;
      bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
      sk_sp<SkImage> image = bridge->NewImageSnapshot(kPreferNoAcceleration,
                                                      kSnapshotReasonUnitTests);
      EXPECT_TRUE(bridge->CheckSurfaceValid());
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

void RunCreateBridgeTask(Canvas2DLayerBridgePtr* bridge_ptr,
                         gpu::gles2::GLES2Interface* gl,
                         Canvas2DLayerBridgeTest* test_host,
                         WaitableEvent* done_event) {
  std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
      WTF::MakeUnique<FakeWebGraphicsContext3DProvider>(gl);
  *bridge_ptr =
      test_host->MakeBridge(std::move(context_provider), IntSize(300, 300),
                            Canvas2DLayerBridge::kEnableAcceleration);
  // draw+flush to trigger the creation of a GPU surface
  (*bridge_ptr)->DidDraw(FloatRect(0, 0, 1, 1));
  (*bridge_ptr)->FinalizeFrame();
  (*bridge_ptr)->Flush();
  done_event->Signal();
}

void PostAndWaitCreateBridgeTask(const WebTraceLocation& location,
                                 WebThread* test_thread,
                                 Canvas2DLayerBridgePtr* bridge_ptr,
                                 gpu::gles2::GLES2Interface* gl,
                                 Canvas2DLayerBridgeTest* test_host) {
  std::unique_ptr<WaitableEvent> bridge_created_event =
      WTF::MakeUnique<WaitableEvent>();
  test_thread->GetWebTaskRunner()->PostTask(
      location, CrossThreadBind(
                    &RunCreateBridgeTask, CrossThreadUnretained(bridge_ptr),
                    CrossThreadUnretained(gl), CrossThreadUnretained(test_host),
                    CrossThreadUnretained(bridge_created_event.get())));
  bridge_created_event->Wait();
}

void RunDestroyBridgeTask(Canvas2DLayerBridgePtr* bridge_ptr,
                          WaitableEvent* done_event) {
  bridge_ptr->Clear();
  if (done_event)
    done_event->Signal();
}

void PostDestroyBridgeTask(const WebTraceLocation& location,
                           WebThread* test_thread,
                           Canvas2DLayerBridgePtr* bridge_ptr) {
  test_thread->GetWebTaskRunner()->PostTask(
      location, CrossThreadBind(&RunDestroyBridgeTask,
                                CrossThreadUnretained(bridge_ptr), nullptr));
}

void PostAndWaitDestroyBridgeTask(const WebTraceLocation& location,
                                  WebThread* test_thread,
                                  Canvas2DLayerBridgePtr* bridge_ptr) {
  std::unique_ptr<WaitableEvent> bridge_destroyed_event =
      WTF::MakeUnique<WaitableEvent>();
  test_thread->GetWebTaskRunner()->PostTask(
      location,
      CrossThreadBind(&RunDestroyBridgeTask, CrossThreadUnretained(bridge_ptr),
                      CrossThreadUnretained(bridge_destroyed_event.get())));
  bridge_destroyed_event->Wait();
}

void RunSetIsHiddenTask(Canvas2DLayerBridge* bridge,
                        bool value,
                        WaitableEvent* done_event) {
  bridge->SetIsHidden(value);
  if (done_event)
    done_event->Signal();
}

void PostSetIsHiddenTask(const WebTraceLocation& location,
                         WebThread* test_thread,
                         Canvas2DLayerBridge* bridge,
                         bool value,
                         WaitableEvent* done_event = nullptr) {
  test_thread->GetWebTaskRunner()->PostTask(
      location,
      CrossThreadBind(&RunSetIsHiddenTask, CrossThreadUnretained(bridge), value,
                      CrossThreadUnretained(done_event)));
}

void PostAndWaitSetIsHiddenTask(const WebTraceLocation& location,
                                WebThread* test_thread,
                                Canvas2DLayerBridge* bridge,
                                bool value) {
  std::unique_ptr<WaitableEvent> done_event = WTF::MakeUnique<WaitableEvent>();
  PostSetIsHiddenTask(location, test_thread, bridge, value, done_event.get());
  done_event->Wait();
}

class MockImageBuffer : public ImageBuffer {
 public:
  MockImageBuffer()
      : ImageBuffer(WTF::WrapUnique(
            new UnacceleratedImageBufferSurface(IntSize(1, 1)))) {}

  MOCK_CONST_METHOD1(ResetCanvas, void(PaintCanvas*));

  virtual ~MockImageBuffer() {}
};

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationLifeCycle)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationLifeCycle)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_started_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating())
      .WillOnce(testing::Invoke(hibernation_started_event.get(),
                                &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  hibernation_started_event->Wait();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally));
  PostAndWaitSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(),
                             false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationReEntry)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationReEntry)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_started_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating())
      .WillOnce(testing::Invoke(hibernation_started_event.get(),
                                &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  // Toggle visibility before the task that enters hibernation gets a
  // chance to run.
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), false);
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);

  hibernation_started_event->Wait();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally));
  PostAndWaitSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(),
                             false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest,
       HibernationLifeCycleWithDeferredRenderingDisabled)
#else
TEST_F(Canvas2DLayerBridgeTest,
       DISABLED_HibernationLifeCycleWithDeferredRenderingDisabled)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);
  bridge->DisableDeferral(kDisableDeferralReasonUnknown);
  MockImageBuffer mock_image_buffer;
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AnyNumber());
  bridge->SetImageBuffer(&mock_image_buffer);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_started_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating())
      .WillOnce(testing::Invoke(hibernation_started_event.get(),
                                &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  hibernation_started_event->Wait();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally));
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_))
      .Times(AtLeast(1));  // Because deferred rendering is disabled
  PostAndWaitSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(),
                             false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
}

void RunRenderingTask(Canvas2DLayerBridge* bridge, WaitableEvent* done_event) {
  bridge->DidDraw(FloatRect(0, 0, 1, 1));
  bridge->FinalizeFrame();
  bridge->Flush();
  done_event->Signal();
}

void PostAndWaitRenderingTask(const WebTraceLocation& location,
                              WebThread* test_thread,
                              Canvas2DLayerBridge* bridge) {
  std::unique_ptr<WaitableEvent> done_event = WTF::MakeUnique<WaitableEvent>();
  test_thread->GetWebTaskRunner()->PostTask(
      location,
      CrossThreadBind(&RunRenderingTask, CrossThreadUnretained(bridge),
                      CrossThreadUnretained(done_event.get())));
  done_event->Wait();
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest, BackgroundRenderingWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_BackgroundRenderingWhileHibernating)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_started_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating())
      .WillOnce(testing::Invoke(hibernation_started_event.get(),
                                &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  hibernation_started_event->Wait();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  PostAndWaitRenderingTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get());
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Unhide
  PostAndWaitSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(),
                             false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_TRUE(
      bridge->IsAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
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
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);
  MockImageBuffer mock_image_buffer;
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AnyNumber());
  bridge->SetImageBuffer(&mock_image_buffer);
  bridge->DisableDeferral(kDisableDeferralReasonUnknown);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_started_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating())
      .WillOnce(testing::Invoke(hibernation_started_event.get(),
                                &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  hibernation_started_event->Wait();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AtLeast(1));
  PostAndWaitRenderingTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get());
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Unhide
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AtLeast(1));
  PostAndWaitSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(),
                             false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_TRUE(
      bridge->IsAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AnyNumber());
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest, DisableDeferredRenderingWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest,
       DISABLED_DisableDeferredRenderingWhileHibernating)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);
  MockImageBuffer mock_image_buffer;
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AnyNumber());
  bridge->SetImageBuffer(&mock_image_buffer);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_started_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating())
      .WillOnce(testing::Invoke(hibernation_started_event.get(),
                                &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  hibernation_started_event->Wait();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

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
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Unhide
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AtLeast(1));
  PostAndWaitSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(),
                             false);
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  ::testing::Mock::VerifyAndClearExpectations(&mock_image_buffer);
  EXPECT_TRUE(
      bridge->IsAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  EXPECT_CALL(mock_image_buffer, ResetCanvas(_)).Times(AnyNumber());
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, TeardownWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_TeardownWhileHibernating)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_started_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating())
      .WillOnce(testing::Invoke(hibernation_started_event.get(),
                                &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  hibernation_started_event->Wait();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Tear down the bridge while hibernating
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::kHibernationEndedWithTeardown));
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, SnapshotWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_SnapshotWhileHibernating)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_started_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating())
      .WillOnce(testing::Invoke(hibernation_started_event.get(),
                                &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  hibernation_started_event->Wait();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Take a snapshot and verify that it is not accelerated due to hibernation
  sk_sp<SkImage> image =
      bridge->NewImageSnapshot(kPreferAcceleration, kSnapshotReasonUnitTests);
  EXPECT_FALSE(image->isTextureBacked());
  image.reset();

  // Verify that taking a snapshot did not affect the state of bridge
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // End hibernation normally
  std::unique_ptr<WaitableEvent> hibernation_ended_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally))
      .WillOnce(testing::InvokeWithoutArgs(hibernation_ended_event.get(),
                                           &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), false);
  hibernation_ended_event->Wait();

  // Tear down the bridge while hibernating
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, TeardownWhileHibernationIsPending)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_TeardownWhileHibernationIsPending)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_scheduled_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true,
                      hibernation_scheduled_event.get());
  PostDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
  // In production, we would expect a
  // HibernationAbortedDueToDestructionWhileHibernatePending event to be
  // fired, but that signal is lost in the unit test due to no longer having
  // a bridge to hold the mockLogger.
  hibernation_scheduled_event->Wait();
  // Once we know the hibernation task is scheduled, we can schedule a fence.
  // Assuming tasks are guaranteed to run in the order they were
  // submitted, this fence will guarantee the attempt to hibernate runs to
  // completion before the thread is destroyed.
  // This test passes by not crashing, which proves that the WeakPtr logic
  // is sound.
  std::unique_ptr<WaitableEvent> fence_event = WTF::MakeUnique<WaitableEvent>();
  test_thread->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&WaitableEvent::Signal, WTF::Unretained(fence_event.get())));
  fence_event->Wait();
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToPendingTeardown)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationAbortedDueToPendingTeardown)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_aborted_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(
          Canvas2DLayerBridge::kHibernationAbortedDueToPendingDestruction))
      .WillOnce(testing::InvokeWithoutArgs(hibernation_aborted_event.get(),
                                           &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  test_thread->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, CrossThreadBind(&Canvas2DLayerBridge::BeginDestruction,
                                       CrossThreadUnretained(bridge.Get())));
  hibernation_aborted_event->Wait();

  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);

  // Tear down bridge on thread
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToVisibilityChange)
#else
TEST_F(Canvas2DLayerBridgeTest,
       DISABLED_HibernationAbortedDueToVisibilityChange)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_aborted_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(
          Canvas2DLayerBridge::kHibernationAbortedDueToVisibilityChange))
      .WillOnce(testing::InvokeWithoutArgs(hibernation_aborted_event.get(),
                                           &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), false);
  hibernation_aborted_event->Wait();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToLostContext)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationAbortedDueToLostContext)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  gl.SetIsContextLost(true);
  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_aborted_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::kHibernationAbortedDueGpuContextLoss))
      .WillOnce(testing::InvokeWithoutArgs(hibernation_aborted_event.get(),
                                           &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  hibernation_aborted_event->Wait();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsHibernating());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_PrepareMailboxWhileHibernating)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_started_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating())
      .WillOnce(testing::Invoke(hibernation_started_event.get(),
                                &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  hibernation_started_event->Wait();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);

  // Test prepareMailbox while hibernating
  viz::TextureMailbox texture_mailbox;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  EXPECT_FALSE(
      bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback));
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::kHibernationEndedWithTeardown));
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhileBackgroundRendering)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_PrepareMailboxWhileBackgroundRendering)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> test_thread =
      Platform::Current()->CreateThread("TestThread");

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  PostAndWaitCreateBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = WTF::WrapUnique(new MockLogger);
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_started_event =
      WTF::MakeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating())
      .WillOnce(testing::Invoke(hibernation_started_event.get(),
                                &WaitableEvent::Signal));
  PostSetIsHiddenTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get(), true);
  hibernation_started_event->Wait();
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  PostAndWaitRenderingTask(BLINK_FROM_HERE, test_thread.get(), bridge.Get());
  ::testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Test prepareMailbox while background rendering
  viz::TextureMailbox texture_mailbox;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  EXPECT_FALSE(
      bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback));
  EXPECT_TRUE(bridge->CheckSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  PostAndWaitDestroyBridgeTask(BLINK_FROM_HERE, test_thread.get(), &bridge);
}

#if USE_IOSURFACE_FOR_2D_CANVAS
TEST_F(Canvas2DLayerBridgeTest, DeleteIOSurfaceAfterTeardown)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_DeleteIOSurfaceAfterTeardown)
#endif
{
  FakeGLES2InterfaceWithImageSupport gl;
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
      WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));

  viz::TextureMailbox texture_mailbox;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;

  {
    Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
        std::move(context_provider), IntSize(300, 150), 0, kNonOpaque,
        Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams())));
    bridge->PrepareTextureMailbox(&texture_mailbox, &release_callback);
  }

  bool lost_resource = false;
  release_callback->Run(gpu::SyncToken(), lost_resource);

  EXPECT_EQ(1u, gl.CreateImageCount());
  EXPECT_EQ(1u, gl.DestroyImageCount());
}

class FlushMockGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
 public:
  MOCK_METHOD0(Flush, void());
};

TEST_F(Canvas2DLayerBridgeTest, NoUnnecessaryFlushes) {
  FlushMockGLES2Interface gl;
  std::unique_ptr<FakeWebGraphicsContext3DProvider> context_provider =
      WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl));

  EXPECT_CALL(gl, Flush()).Times(0);
  Canvas2DLayerBridgePtr bridge(AdoptRef(new Canvas2DLayerBridge(
      std::move(context_provider), IntSize(300, 150), 0, kNonOpaque,
      Canvas2DLayerBridge::kForceAccelerationForTesting, CanvasColorParams())));
  EXPECT_FALSE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl);

  EXPECT_CALL(gl, Flush()).Times(0);
  bridge->DidDraw(FloatRect(0, 0, 1, 1));
  EXPECT_TRUE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl);

  EXPECT_CALL(gl, Flush()).Times(1);
  bridge->FlushGpu();
  EXPECT_FALSE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl);

  EXPECT_CALL(gl, Flush()).Times(0);
  bridge->DidDraw(FloatRect(0, 0, 1, 1));
  EXPECT_TRUE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl);

  EXPECT_CALL(gl, Flush()).Times(1);
  bridge->FlushGpu();
  EXPECT_FALSE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl);

  // No flush because already flushed since last draw
  EXPECT_CALL(gl, Flush()).Times(0);
  bridge->FlushGpu();
  EXPECT_FALSE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl);

  EXPECT_CALL(gl, Flush()).Times(0);
  bridge->DidDraw(FloatRect(0, 0, 1, 1));
  EXPECT_TRUE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl);

  // Flushes recording, but not the gpu
  EXPECT_CALL(gl, Flush()).Times(0);
  bridge->Flush();
  EXPECT_FALSE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl);

  EXPECT_CALL(gl, Flush()).Times(1);
  bridge->FlushGpu();
  EXPECT_FALSE(bridge->HasRecordedDrawCommands());
  ::testing::Mock::VerifyAndClearExpectations(&gl);
}

}  // namespace blink
