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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/Canvas2DLayerBridge.h"

#include "SkSurface.h"
#include "base/memory/scoped_ptr.h"
#include "platform/WaitableEvent.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/test/MockWebGraphicsContext3D.h"
#include "public/platform/Platform.h"
#include "public/platform/WebExternalBitmap.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/SkNullGLContext.h"
#include "wtf/RefPtr.h"

using testing::AnyNumber;
using testing::AtLeast;
using testing::InSequence;
using testing::Return;
using testing::Test;
using testing::_;

namespace blink {

namespace {

class MockCanvasContext : public MockWebGraphicsContext3D {
public:
    MOCK_METHOD0(flush, void(void));
    MOCK_METHOD0(createTexture, unsigned(void));
    MOCK_METHOD1(deleteTexture, void(unsigned));
};

class MockWebGraphicsContext3DProvider : public WebGraphicsContext3DProvider {
public:
    MockWebGraphicsContext3DProvider(WebGraphicsContext3D* context3d)
        : m_context3d(context3d)
    {
        scoped_ptr<SkGLContext> glContext(SkNullGLContext::Create());
        glContext->makeCurrent();
        m_grContext = adoptRef(GrContext::Create(kOpenGL_GrBackend, reinterpret_cast<GrBackendContext>(glContext->gl())));
    }

    WebGraphicsContext3D* context3d() override
    {
        return m_context3d;
    }

    GrContext* grContext() override
    {
        return m_grContext.get();
    }

private:
    WebGraphicsContext3D* m_context3d;
    RefPtr<GrContext> m_grContext;
};

class Canvas2DLayerBridgePtr {
public:
    Canvas2DLayerBridgePtr() { }
    Canvas2DLayerBridgePtr(PassRefPtr<Canvas2DLayerBridge> layerBridge)
        : m_layerBridge(layerBridge) { }

    ~Canvas2DLayerBridgePtr()
    {
        clear();
    }

    void clear()
    {
        if (m_layerBridge) {
            m_layerBridge->beginDestruction();
            m_layerBridge.clear();
        }
    }

    void operator=(PassRefPtr<Canvas2DLayerBridge> layerBridge)
    {
        ASSERT(!m_layerBridge);
        m_layerBridge = layerBridge;
    }

    Canvas2DLayerBridge* operator->() { return m_layerBridge.get(); }
    Canvas2DLayerBridge* get() { return m_layerBridge.get(); }

private:
    RefPtr<Canvas2DLayerBridge> m_layerBridge;
};

class NullWebExternalBitmap : public WebExternalBitmap {
public:
    WebSize size() override
    {
        return WebSize();
    }

    void setSize(WebSize) override
    {
    }

    uint8_t* pixels() override
    {
        return nullptr;
    }
};

} // anonymous namespace

class Canvas2DLayerBridgeTest : public Test {
public:
    PassRefPtr<Canvas2DLayerBridge> makeBridge(PassOwnPtr<MockWebGraphicsContext3DProvider> provider, const IntSize& size, Canvas2DLayerBridge::AccelerationMode accelerationMode)
    {
        return adoptRef(new Canvas2DLayerBridge(provider, size, 0, NonOpaque, accelerationMode));
    }

protected:
    void fullLifecycleTest()
    {
        MockCanvasContext mainMock;
        OwnPtr<MockWebGraphicsContext3DProvider> mainMockProvider = adoptPtr(new MockWebGraphicsContext3DProvider(&mainMock));

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);

        {
            Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(mainMockProvider.release(), IntSize(300, 150), 0, NonOpaque, Canvas2DLayerBridge::DisableAcceleration)));

            ::testing::Mock::VerifyAndClearExpectations(&mainMock);

            unsigned textureId = bridge->newImageSnapshot(PreferAcceleration, SnapshotReasonUnknown)->getTextureHandle(true);
            EXPECT_EQ(textureId, 0u);

            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
        } // bridge goes out of scope here

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);
    }

    void fallbackToSoftwareIfContextLost()
    {
        MockCanvasContext mainMock;
        OwnPtr<MockWebGraphicsContext3DProvider> mainMockProvider = adoptPtr(new MockWebGraphicsContext3DProvider(&mainMock));

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);

        {
            mainMock.fakeContextLost();
            Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(mainMockProvider.release(), IntSize(300, 150), 0, NonOpaque, Canvas2DLayerBridge::EnableAcceleration)));
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
            EXPECT_TRUE(bridge->checkSurfaceValid());
            EXPECT_FALSE(bridge->isAccelerated());
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
        }

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);
    }

    void fallbackToSoftwareOnFailedTextureAlloc()
    {
        MockCanvasContext mainMock;

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);

        {
            // No fallback case
            OwnPtr<MockWebGraphicsContext3DProvider> mainMockProvider = adoptPtr(new MockWebGraphicsContext3DProvider(&mainMock));
            Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(mainMockProvider.release(), IntSize(300, 150), 0, NonOpaque, Canvas2DLayerBridge::EnableAcceleration)));
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
            EXPECT_TRUE(bridge->checkSurfaceValid());
            EXPECT_TRUE(bridge->isAccelerated());
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
            RefPtr<SkImage> snapshot = bridge->newImageSnapshot(PreferAcceleration, SnapshotReasonUnknown);
            EXPECT_TRUE(bridge->isAccelerated());
            EXPECT_TRUE(snapshot->isTextureBacked());
        }

        {
            // Fallback case
            OwnPtr<MockWebGraphicsContext3DProvider> mainMockProvider = adoptPtr(new MockWebGraphicsContext3DProvider(&mainMock));
            GrContext* gr = mainMockProvider->grContext();
            Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(mainMockProvider.release(), IntSize(300, 150), 0, NonOpaque, Canvas2DLayerBridge::EnableAcceleration)));
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
            EXPECT_TRUE(bridge->checkSurfaceValid());
            EXPECT_TRUE(bridge->isAccelerated()); // We don't yet know that allocation will fail
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
            gr->abandonContext(); // This will cause SkSurface_Gpu creation to fail without Canvas2DLayerBridge otherwise detecting that anything was disabled.
            RefPtr<SkImage> snapshot = bridge->newImageSnapshot(PreferAcceleration, SnapshotReasonUnknown);
            EXPECT_FALSE(bridge->isAccelerated());
            EXPECT_FALSE(snapshot->isTextureBacked());
        }

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);
    }

    void noDrawOnContextLostTest()
    {
        MockCanvasContext mainMock;
        OwnPtr<MockWebGraphicsContext3DProvider> mainMockProvider = adoptPtr(new MockWebGraphicsContext3DProvider(&mainMock));

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);

        {
            Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(mainMockProvider.release(), IntSize(300, 150), 0, NonOpaque, Canvas2DLayerBridge::ForceAccelerationForTesting)));
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
            EXPECT_TRUE(bridge->checkSurfaceValid());
            SkPaint paint;
            uint32_t genID = bridge->getOrCreateSurface()->generationID();
            bridge->canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
            EXPECT_EQ(genID, bridge->getOrCreateSurface()->generationID());
            mainMock.fakeContextLost();
            EXPECT_EQ(genID, bridge->getOrCreateSurface()->generationID());
            bridge->canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
            EXPECT_EQ(genID, bridge->getOrCreateSurface()->generationID());
            EXPECT_FALSE(bridge->checkSurfaceValid()); // This results in the internal surface being torn down in response to the context loss
            EXPECT_EQ(nullptr, bridge->getOrCreateSurface());
            // The following passes by not crashing
            bridge->canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
            bridge->flush();
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
        }

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);
    }

    void prepareMailboxWithBitmapTest()
    {
        MockCanvasContext mainMock;
        OwnPtr<MockWebGraphicsContext3DProvider> mainMockProvider = adoptPtr(new MockWebGraphicsContext3DProvider(&mainMock));
        Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(mainMockProvider.release(), IntSize(300, 150), 0, NonOpaque, Canvas2DLayerBridge::ForceAccelerationForTesting)));
        bridge->m_lastImageId = 1;

        NullWebExternalBitmap bitmap;
        bridge->prepareMailbox(0, &bitmap);
        EXPECT_EQ(0u, bridge->m_lastImageId);
    }

    void prepareMailboxAndLoseResourceTest()
    {
        MockCanvasContext mainMock;
        bool lostResource = true;

        // Prepare a mailbox, then report the resource as lost.
        // This test passes by not crashing and not triggering assertions.
        {
            WebExternalTextureMailbox mailbox;
            OwnPtr<MockWebGraphicsContext3DProvider> mainMockProvider = adoptPtr(new MockWebGraphicsContext3DProvider(&mainMock));
            Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(mainMockProvider.release(), IntSize(300, 150), 0, NonOpaque, Canvas2DLayerBridge::ForceAccelerationForTesting)));
            bridge->prepareMailbox(&mailbox, 0);
            bridge->mailboxReleased(mailbox, lostResource);
        }

        // Retry with mailbox released while bridge destruction is in progress
        {
            OwnPtr<MockWebGraphicsContext3DProvider> mainMockProvider = adoptPtr(new MockWebGraphicsContext3DProvider(&mainMock));
            WebExternalTextureMailbox mailbox;
            Canvas2DLayerBridge* rawBridge;
            {
                Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(mainMockProvider.release(), IntSize(300, 150), 0, NonOpaque, Canvas2DLayerBridge::ForceAccelerationForTesting)));
                bridge->prepareMailbox(&mailbox, 0);
                rawBridge = bridge.get();
            } // bridge goes out of scope, but object is kept alive by self references
            // before fixing crbug.com/411864, the following line you cause a memory use after free
            // that sometimes causes a crash in normal builds and crashes consistently with ASAN.
            rawBridge->mailboxReleased(mailbox, lostResource); // This should self-destruct the bridge.
        }
    }

    void accelerationHintTest()
    {
        MockCanvasContext mainMock;
        {

            OwnPtr<MockWebGraphicsContext3DProvider> mainMockProvider = adoptPtr(new MockWebGraphicsContext3DProvider(&mainMock));
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
            Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(mainMockProvider.release(), IntSize(300, 300), 0, NonOpaque, Canvas2DLayerBridge::EnableAcceleration)));
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
            SkPaint paint;
            bridge->canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
            RefPtr<SkImage> image = bridge->newImageSnapshot(PreferAcceleration, SnapshotReasonUnknown);
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
            EXPECT_TRUE(bridge->checkSurfaceValid());
            EXPECT_TRUE(bridge->isAccelerated());
        }
        ::testing::Mock::VerifyAndClearExpectations(&mainMock);

        {
            OwnPtr<MockWebGraphicsContext3DProvider> mainMockProvider = adoptPtr(new MockWebGraphicsContext3DProvider(&mainMock));
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
            Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(mainMockProvider.release(), IntSize(300, 300), 0, NonOpaque, Canvas2DLayerBridge::EnableAcceleration)));
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
            SkPaint paint;
            bridge->canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
            RefPtr<SkImage> image = bridge->newImageSnapshot(PreferNoAcceleration, SnapshotReasonUnknown);
            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
            EXPECT_TRUE(bridge->checkSurfaceValid());
            EXPECT_FALSE(bridge->isAccelerated());
        }
        ::testing::Mock::VerifyAndClearExpectations(&mainMock);
    }
};

TEST_F(Canvas2DLayerBridgeTest, FullLifecycleSingleThreaded)
{
    fullLifecycleTest();
}

TEST_F(Canvas2DLayerBridgeTest, NoDrawOnContextLost)
{
    noDrawOnContextLostTest();
}

TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWithBitmap)
{
    prepareMailboxWithBitmapTest();
}

TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxAndLoseResource)
{
    prepareMailboxAndLoseResourceTest();
}

TEST_F(Canvas2DLayerBridgeTest, AccelerationHint)
{
    accelerationHintTest();
}

TEST_F(Canvas2DLayerBridgeTest, FallbackToSoftwareIfContextLost)
{
    fallbackToSoftwareIfContextLost();
}

TEST_F(Canvas2DLayerBridgeTest, FallbackToSoftwareOnFailedTextureAlloc)
{
    fallbackToSoftwareOnFailedTextureAlloc();
}

class MockLogger : public Canvas2DLayerBridge::Logger {
public:
    MOCK_METHOD1(reportHibernationEvent, void(Canvas2DLayerBridge::HibernationEvent));
    MOCK_METHOD0(didStartHibernating, void());
    virtual ~MockLogger() { }
};


class CreateBridgeTask : public WebTaskRunner::Task {
public:
    CreateBridgeTask(Canvas2DLayerBridgePtr* bridgePtr, MockCanvasContext* mockCanvasContext, Canvas2DLayerBridgeTest* testHost, WaitableEvent* doneEvent)
        : m_bridgePtr(bridgePtr)
        , m_mockCanvasContext(mockCanvasContext)
        , m_testHost(testHost)
        , m_doneEvent(doneEvent)
    { }

    virtual ~CreateBridgeTask() { }

    void run() override
    {
        OwnPtr<MockWebGraphicsContext3DProvider> mainMockProvider = adoptPtr(new MockWebGraphicsContext3DProvider(m_mockCanvasContext));
        *m_bridgePtr = m_testHost->makeBridge(mainMockProvider.release(), IntSize(300, 300), Canvas2DLayerBridge::EnableAcceleration);
        // draw+flush to trigger the creation of a GPU surface
        (*m_bridgePtr)->didDraw(FloatRect(0, 0, 1, 1));
        (*m_bridgePtr)->finalizeFrame(FloatRect(0, 0, 1, 1));
        (*m_bridgePtr)->flush();
        m_doneEvent->signal();
    }

private:
    Canvas2DLayerBridgePtr* m_bridgePtr;
    MockCanvasContext* m_mockCanvasContext;
    Canvas2DLayerBridgeTest* m_testHost;
    WaitableEvent* m_doneEvent;
};

class DestroyBridgeTask : public WebTaskRunner::Task {
public:
    DestroyBridgeTask(Canvas2DLayerBridgePtr* bridgePtr, WaitableEvent* doneEvent = nullptr)
        : m_bridgePtr(bridgePtr)
        , m_doneEvent(doneEvent)
    { }

    virtual ~DestroyBridgeTask() { }

    void run() override
    {
        m_bridgePtr->clear();
        if (m_doneEvent)
            m_doneEvent->signal();
    }

private:
    Canvas2DLayerBridgePtr* m_bridgePtr;
    WaitableEvent* m_doneEvent;
};

class SetIsHiddenTask : public WebTaskRunner::Task {
public:
    SetIsHiddenTask(Canvas2DLayerBridge* bridge, bool value, WaitableEvent* doneEvent = nullptr)
        : m_bridge(bridge)
        , m_value(value)
        , m_doneEvent(doneEvent)
    { }

    virtual ~SetIsHiddenTask() { }

    void run() override
    {
        m_bridge->setIsHidden(m_value);
        if (m_doneEvent)
            m_doneEvent->signal();
    }

private:
    Canvas2DLayerBridge* m_bridge;
    bool m_value;
    WaitableEvent* m_doneEvent;
};

class MockImageBuffer : public ImageBuffer {
public:
    MockImageBuffer()
        : ImageBuffer(adoptPtr(new UnacceleratedImageBufferSurface(IntSize(1, 1)))) { }

    MOCK_CONST_METHOD1(resetCanvas, void(SkCanvas*));

    virtual ~MockImageBuffer() { }
};

TEST_F(Canvas2DLayerBridgeTest, HibernationLifeCycle)
{
    MockCanvasContext mainMock;
    OwnPtr<WebThread> testThread = adoptPtr(Platform::current()->createThread("TestThread"));

    // The Canvas2DLayerBridge has to be created on the thread that will use it
    // to avoid WeakPtr thread check issues.
    Canvas2DLayerBridgePtr bridge;
    OwnPtr<WaitableEvent> bridgeCreatedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new CreateBridgeTask(&bridge, &mainMock, this, bridgeCreatedEvent.get()));
    bridgeCreatedEvent->wait();

    // Register an alternate Logger for tracking hibernation events
    OwnPtr<MockLogger> mockLogger = adoptPtr(new MockLogger);
    MockLogger* mockLoggerPtr = mockLogger.get();
    bridge->setLoggerForTesting(mockLogger.release());

    // Test entering hibernation
    OwnPtr<WaitableEvent> hibernationStartedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationScheduled));
    EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
        .WillOnce(testing::Invoke(hibernationStartedEvent.get(), &WaitableEvent::signal));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), true));
    hibernationStartedEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    EXPECT_FALSE(bridge->isAccelerated());
    EXPECT_TRUE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Test exiting hibernation
    OwnPtr<WaitableEvent> hibernationEndedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationEndedNormally));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), false, hibernationEndedEvent.get()));
    hibernationEndedEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    EXPECT_TRUE(bridge->isAccelerated());
    EXPECT_FALSE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Tear down the bridge on the thread so that 'bridge' can go out of scope
    // without crashing due to thread checks
    OwnPtr<WaitableEvent> bridgeDestroyedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new DestroyBridgeTask(&bridge, bridgeDestroyedEvent.get()));
    bridgeDestroyedEvent->wait();

    ::testing::Mock::VerifyAndClearExpectations(&mainMock);
}

TEST_F(Canvas2DLayerBridgeTest, HibernationLifeCycleWithDeferredRenderingDisabled)
{
    MockCanvasContext mainMock;
    OwnPtr<WebThread> testThread = adoptPtr(Platform::current()->createThread("TestThread"));

    // The Canvas2DLayerBridge has to be created on the thread that will use it
    // to avoid WeakPtr thread check issues.
    Canvas2DLayerBridgePtr bridge;
    OwnPtr<WaitableEvent> bridgeCreatedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new CreateBridgeTask(&bridge, &mainMock, this, bridgeCreatedEvent.get()));
    bridgeCreatedEvent->wait();
    bridge->disableDeferral(DisableDeferralReasonUnknown);
    MockImageBuffer mockImageBuffer;
    EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AnyNumber());
    bridge->setImageBuffer(&mockImageBuffer);

    // Register an alternate Logger for tracking hibernation events
    OwnPtr<MockLogger> mockLogger = adoptPtr(new MockLogger);
    MockLogger* mockLoggerPtr = mockLogger.get();
    bridge->setLoggerForTesting(mockLogger.release());

    // Test entering hibernation
    OwnPtr<WaitableEvent> hibernationStartedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationScheduled));
    EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
        .WillOnce(testing::Invoke(hibernationStartedEvent.get(), &WaitableEvent::signal));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), true));
    hibernationStartedEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
    EXPECT_FALSE(bridge->isAccelerated());
    EXPECT_TRUE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Test exiting hibernation
    OwnPtr<WaitableEvent> hibernationEndedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationEndedNormally));
    EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AtLeast(1)); // Because deferred rendering is disabled
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), false, hibernationEndedEvent.get()));
    hibernationEndedEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
    EXPECT_TRUE(bridge->isAccelerated());
    EXPECT_FALSE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Tear down the bridge on the thread so that 'bridge' can go out of scope
    // without crashing due to thread checks
    OwnPtr<WaitableEvent> bridgeDestroyedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new DestroyBridgeTask(&bridge, bridgeDestroyedEvent.get()));
    bridgeDestroyedEvent->wait();

    ::testing::Mock::VerifyAndClearExpectations(&mainMock);
}

class RenderingTask : public WebTaskRunner::Task {
public:
    RenderingTask(Canvas2DLayerBridge* bridge, WaitableEvent* doneEvent)
        : m_bridge(bridge)
        , m_doneEvent(doneEvent)
    { }

    virtual ~RenderingTask() { }

    void run() override
    {
        m_bridge->didDraw(FloatRect(0, 0, 1, 1));
        m_bridge->finalizeFrame(FloatRect(0, 0, 1, 1));
        m_bridge->flush();
        m_doneEvent->signal();
    }

private:
    Canvas2DLayerBridge* m_bridge;
    WaitableEvent* m_doneEvent;
};

TEST_F(Canvas2DLayerBridgeTest, BackgroundRenderingWhileHibernating)
{
    MockCanvasContext mainMock;
    OwnPtr<WebThread> testThread = adoptPtr(Platform::current()->createThread("TestThread"));

    // The Canvas2DLayerBridge has to be created on the thread that will use it
    // to avoid WeakPtr thread check issues.
    Canvas2DLayerBridgePtr bridge;
    OwnPtr<WaitableEvent> bridgeCreatedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new CreateBridgeTask(&bridge, &mainMock, this, bridgeCreatedEvent.get()));
    bridgeCreatedEvent->wait();

    // Register an alternate Logger for tracking hibernation events
    OwnPtr<MockLogger> mockLogger = adoptPtr(new MockLogger);
    MockLogger* mockLoggerPtr = mockLogger.get();
    bridge->setLoggerForTesting(mockLogger.release());

    // Test entering hibernation
    OwnPtr<WaitableEvent> hibernationStartedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationScheduled));
    EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
        .WillOnce(testing::Invoke(hibernationStartedEvent.get(), &WaitableEvent::signal));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), true));
    hibernationStartedEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    EXPECT_FALSE(bridge->isAccelerated());
    EXPECT_TRUE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Rendering in the background -> temp switch to SW
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationEndedWithSwitchToBackgroundRendering));
    OwnPtr<WaitableEvent> switchEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new RenderingTask(bridge.get(), switchEvent.get()));
    switchEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    EXPECT_FALSE(bridge->isAccelerated());
    EXPECT_FALSE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Unhide
    OwnPtr<WaitableEvent> unhideEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), false, unhideEvent.get()));
    unhideEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    EXPECT_TRUE(bridge->isAccelerated()); // Becoming visible causes switch back to GPU
    EXPECT_FALSE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Tear down the bridge on the thread so that 'bridge' can go out of scope
    // without crashing due to thread checks
    OwnPtr<WaitableEvent> bridgeDestroyedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new DestroyBridgeTask(&bridge, bridgeDestroyedEvent.get()));
    bridgeDestroyedEvent->wait();

    ::testing::Mock::VerifyAndClearExpectations(&mainMock);
}

TEST_F(Canvas2DLayerBridgeTest, BackgroundRenderingWhileHibernatingWithDeferredRenderingDisabled)
{
    MockCanvasContext mainMock;
    OwnPtr<WebThread> testThread = adoptPtr(Platform::current()->createThread("TestThread"));

    // The Canvas2DLayerBridge has to be created on the thread that will use it
    // to avoid WeakPtr thread check issues.
    Canvas2DLayerBridgePtr bridge;
    OwnPtr<WaitableEvent> bridgeCreatedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new CreateBridgeTask(&bridge, &mainMock, this, bridgeCreatedEvent.get()));
    bridgeCreatedEvent->wait();
    MockImageBuffer mockImageBuffer;
    EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AnyNumber());
    bridge->setImageBuffer(&mockImageBuffer);
    bridge->disableDeferral(DisableDeferralReasonUnknown);

    // Register an alternate Logger for tracking hibernation events
    OwnPtr<MockLogger> mockLogger = adoptPtr(new MockLogger);
    MockLogger* mockLoggerPtr = mockLogger.get();
    bridge->setLoggerForTesting(mockLogger.release());

    // Test entering hibernation
    OwnPtr<WaitableEvent> hibernationStartedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationScheduled));
    EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
        .WillOnce(testing::Invoke(hibernationStartedEvent.get(), &WaitableEvent::signal));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), true));
    hibernationStartedEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
    EXPECT_FALSE(bridge->isAccelerated());
    EXPECT_TRUE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Rendering in the background -> temp switch to SW
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationEndedWithSwitchToBackgroundRendering));
    EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AtLeast(1));
    OwnPtr<WaitableEvent> switchEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new RenderingTask(bridge.get(), switchEvent.get()));
    switchEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
    EXPECT_FALSE(bridge->isAccelerated());
    EXPECT_FALSE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Unhide
    EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AtLeast(1));
    OwnPtr<WaitableEvent> unhideEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), false, unhideEvent.get()));
    unhideEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
    EXPECT_TRUE(bridge->isAccelerated()); // Becoming visible causes switch back to GPU
    EXPECT_FALSE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Tear down the bridge on the thread so that 'bridge' can go out of scope
    // without crashing due to thread checks
    EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AnyNumber());
    OwnPtr<WaitableEvent> bridgeDestroyedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new DestroyBridgeTask(&bridge, bridgeDestroyedEvent.get()));
    bridgeDestroyedEvent->wait();
}

TEST_F(Canvas2DLayerBridgeTest, DisableDeferredRenderingWhileHibernating)
{
    MockCanvasContext mainMock;
    OwnPtr<WebThread> testThread = adoptPtr(Platform::current()->createThread("TestThread"));

    // The Canvas2DLayerBridge has to be created on the thread that will use it
    // to avoid WeakPtr thread check issues.
    Canvas2DLayerBridgePtr bridge;
    OwnPtr<WaitableEvent> bridgeCreatedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new CreateBridgeTask(&bridge, &mainMock, this, bridgeCreatedEvent.get()));
    bridgeCreatedEvent->wait();
    MockImageBuffer mockImageBuffer;
    EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AnyNumber());
    bridge->setImageBuffer(&mockImageBuffer);

    // Register an alternate Logger for tracking hibernation events
    OwnPtr<MockLogger> mockLogger = adoptPtr(new MockLogger);
    MockLogger* mockLoggerPtr = mockLogger.get();
    bridge->setLoggerForTesting(mockLogger.release());

    // Test entering hibernation
    OwnPtr<WaitableEvent> hibernationStartedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationScheduled));
    EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
        .WillOnce(testing::Invoke(hibernationStartedEvent.get(), &WaitableEvent::signal));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), true));
    hibernationStartedEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
    EXPECT_FALSE(bridge->isAccelerated());
    EXPECT_TRUE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Disable deferral while background rendering
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationEndedWithSwitchToBackgroundRendering));
    EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AtLeast(1));
    bridge->disableDeferral(DisableDeferralReasonUnknown);
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
    EXPECT_FALSE(bridge->isAccelerated());
    EXPECT_FALSE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Unhide
    EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AtLeast(1));
    OwnPtr<WaitableEvent> unhideEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), false, unhideEvent.get()));
    unhideEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
    EXPECT_TRUE(bridge->isAccelerated()); // Becoming visible causes switch back to GPU
    EXPECT_FALSE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Tear down the bridge on the thread so that 'bridge' can go out of scope
    // without crashing due to thread checks
    EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AnyNumber());
    OwnPtr<WaitableEvent> bridgeDestroyedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new DestroyBridgeTask(&bridge, bridgeDestroyedEvent.get()));
    bridgeDestroyedEvent->wait();
}

TEST_F(Canvas2DLayerBridgeTest, TeardownWhileHibernating)
{
    MockCanvasContext mainMock;
    OwnPtr<WebThread> testThread = adoptPtr(Platform::current()->createThread("TestThread"));

    // The Canvas2DLayerBridge has to be created on the thread that will use it
    // to avoid WeakPtr thread check issues.
    Canvas2DLayerBridgePtr bridge;
    OwnPtr<WaitableEvent> bridgeCreatedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new CreateBridgeTask(&bridge, &mainMock, this, bridgeCreatedEvent.get()));
    bridgeCreatedEvent->wait();

    // Register an alternate Logger for tracking hibernation events
    OwnPtr<MockLogger> mockLogger = adoptPtr(new MockLogger);
    MockLogger* mockLoggerPtr = mockLogger.get();
    bridge->setLoggerForTesting(mockLogger.release());

    // Test entering hibernation
    OwnPtr<WaitableEvent> hibernationStartedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationScheduled));
    EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
        .WillOnce(testing::Invoke(hibernationStartedEvent.get(), &WaitableEvent::signal));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), true));
    hibernationStartedEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    EXPECT_FALSE(bridge->isAccelerated());
    EXPECT_TRUE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Tear down the bridge while hibernating
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationEndedWithTeardown));
    OwnPtr<WaitableEvent> bridgeDestroyedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new DestroyBridgeTask(&bridge, bridgeDestroyedEvent.get()));
    bridgeDestroyedEvent->wait();

    ::testing::Mock::VerifyAndClearExpectations(&mainMock);
}

class IdleFenceTask : public WebThread::IdleTask {
public:
    IdleFenceTask(WaitableEvent* doneEvent)
        : m_doneEvent(doneEvent)
    { }

    virtual ~IdleFenceTask() { }

    void run(double /*deadline*/) override
    {
        m_doneEvent->signal();
    }

private:
    WaitableEvent* m_doneEvent;
};

TEST_F(Canvas2DLayerBridgeTest, TeardownWhileHibernationIsPending)
{
    MockCanvasContext mainMock;
    OwnPtr<WebThread> testThread = adoptPtr(Platform::current()->createThread("TestThread"));

    // The Canvas2DLayerBridge has to be created on the thread that will use it
    // to avoid WeakPtr thread check issues.
    Canvas2DLayerBridgePtr bridge;
    OwnPtr<WaitableEvent> bridgeCreatedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new CreateBridgeTask(&bridge, &mainMock, this, bridgeCreatedEvent.get()));
    bridgeCreatedEvent->wait();

    // Register an alternate Logger for tracking hibernation events
    OwnPtr<MockLogger> mockLogger = adoptPtr(new MockLogger);
    MockLogger* mockLoggerPtr = mockLogger.get();
    bridge->setLoggerForTesting(mockLogger.release());

    // Test entering hibernation
    OwnPtr<WaitableEvent> hibernationScheduledEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationScheduled));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), true, hibernationScheduledEvent.get()));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new DestroyBridgeTask(&bridge));
    // In production, we would expect a
    // HibernationAbortedDueToDestructionWhileHibernatePending event to be
    // fired, but that signal is lost in the unit test due to no longer having
    // a bridge to hold the mockLogger.
    hibernationScheduledEvent->wait();
    // Once we know the hibernation task is scheduled, we can schedule a fence.
    // Assuming Idle tasks are guaranteed to run in the order they were
    // submitted, this fence will guarantee the attempt to hibernate runs to
    // completion before the thread is destroyed.
    // This test passes by not crashing, which proves that the WeakPtr logic
    // is sound.
    OwnPtr<WaitableEvent> fenceEvent = adoptPtr(new WaitableEvent());
    testThread->scheduler()->postIdleTask(BLINK_FROM_HERE, new IdleFenceTask(fenceEvent.get()));
    fenceEvent->wait();

    ::testing::Mock::VerifyAndClearExpectations(&mainMock);
}

class BeginDestroyBridgeTask : public WebTaskRunner::Task {
public:
    BeginDestroyBridgeTask(Canvas2DLayerBridge* bridge)
        : m_bridge(bridge)
    { }

    virtual ~BeginDestroyBridgeTask() { }

    void run() override
    {
        m_bridge->beginDestruction();
    }

private:
    Canvas2DLayerBridge* m_bridge;
};

TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToPendingTeardown)
{
    MockCanvasContext mainMock;
    OwnPtr<WebThread> testThread = adoptPtr(Platform::current()->createThread("TestThread"));

    // The Canvas2DLayerBridge has to be created on the thread that will use it
    // to avoid WeakPtr thread check issues.
    Canvas2DLayerBridgePtr bridge;
    OwnPtr<WaitableEvent> bridgeCreatedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new CreateBridgeTask(&bridge, &mainMock, this, bridgeCreatedEvent.get()));
    bridgeCreatedEvent->wait();

    // Register an alternate Logger for tracking hibernation events
    OwnPtr<MockLogger> mockLogger = adoptPtr(new MockLogger);
    MockLogger* mockLoggerPtr = mockLogger.get();
    bridge->setLoggerForTesting(mockLogger.release());

    // Test entering hibernation
    OwnPtr<WaitableEvent> hibernationAbortedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationScheduled));
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationAbortedDueToPendingDestruction))
        .WillOnce(testing::InvokeWithoutArgs(hibernationAbortedEvent.get(), &WaitableEvent::signal));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), true));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new BeginDestroyBridgeTask(bridge.get()));
    hibernationAbortedEvent->wait();

    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);

    // Tear down bridge on thread
    OwnPtr<WaitableEvent> bridgeDestroyedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new DestroyBridgeTask(&bridge, bridgeDestroyedEvent.get()));
    bridgeDestroyedEvent->wait();

    ::testing::Mock::VerifyAndClearExpectations(&mainMock);
}

TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToVisibilityChange)
{
    MockCanvasContext mainMock;
    OwnPtr<WebThread> testThread = adoptPtr(Platform::current()->createThread("TestThread"));

    // The Canvas2DLayerBridge has to be created on the thread that will use it
    // to avoid WeakPtr thread check issues.
    Canvas2DLayerBridgePtr bridge;
    OwnPtr<WaitableEvent> bridgeCreatedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new CreateBridgeTask(&bridge, &mainMock, this, bridgeCreatedEvent.get()));
    bridgeCreatedEvent->wait();

    // Register an alternate Logger for tracking hibernation events
    OwnPtr<MockLogger> mockLogger = adoptPtr(new MockLogger);
    MockLogger* mockLoggerPtr = mockLogger.get();
    bridge->setLoggerForTesting(mockLogger.release());

    // Test entering hibernation
    OwnPtr<WaitableEvent> hibernationAbortedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationScheduled));
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationAbortedDueToVisibilityChange))
        .WillOnce(testing::InvokeWithoutArgs(hibernationAbortedEvent.get(), &WaitableEvent::signal));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), true));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), false));
    hibernationAbortedEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    EXPECT_TRUE(bridge->isAccelerated());
    EXPECT_FALSE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Tear down the bridge on the thread so that 'bridge' can go out of scope
    // without crashing due to thread checks
    OwnPtr<WaitableEvent> bridgeDestroyedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new DestroyBridgeTask(&bridge, bridgeDestroyedEvent.get()));
    bridgeDestroyedEvent->wait();

    ::testing::Mock::VerifyAndClearExpectations(&mainMock);
}

TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToLostContext)
{
    MockCanvasContext mainMock;
    OwnPtr<WebThread> testThread = adoptPtr(Platform::current()->createThread("TestThread"));

    // The Canvas2DLayerBridge has to be created on the thread that will use it
    // to avoid WeakPtr thread check issues.
    Canvas2DLayerBridgePtr bridge;
    OwnPtr<WaitableEvent> bridgeCreatedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new CreateBridgeTask(&bridge, &mainMock, this, bridgeCreatedEvent.get()));
    bridgeCreatedEvent->wait();

    // Register an alternate Logger for tracking hibernation events
    OwnPtr<MockLogger> mockLogger = adoptPtr(new MockLogger);
    MockLogger* mockLoggerPtr = mockLogger.get();
    bridge->setLoggerForTesting(mockLogger.release());

    mainMock.fakeContextLost();
    // Test entering hibernation
    OwnPtr<WaitableEvent> hibernationAbortedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationScheduled));
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationAbortedDueGpuContextLoss))
        .WillOnce(testing::InvokeWithoutArgs(hibernationAbortedEvent.get(), &WaitableEvent::signal));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), true));
    hibernationAbortedEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    EXPECT_FALSE(bridge->isHibernating());

    // Tear down the bridge on the thread so that 'bridge' can go out of scope
    // without crashing due to thread checks
    OwnPtr<WaitableEvent> bridgeDestroyedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new DestroyBridgeTask(&bridge, bridgeDestroyedEvent.get()));
    bridgeDestroyedEvent->wait();

    ::testing::Mock::VerifyAndClearExpectations(&mainMock);
}

TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhileHibernating)
{
    MockCanvasContext mainMock;
    OwnPtr<WebThread> testThread = adoptPtr(Platform::current()->createThread("TestThread"));

    // The Canvas2DLayerBridge has to be created on the thread that will use it
    // to avoid WeakPtr thread check issues.
    Canvas2DLayerBridgePtr bridge;
    OwnPtr<WaitableEvent> bridgeCreatedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new CreateBridgeTask(&bridge, &mainMock, this, bridgeCreatedEvent.get()));
    bridgeCreatedEvent->wait();

    // Register an alternate Logger for tracking hibernation events
    OwnPtr<MockLogger> mockLogger = adoptPtr(new MockLogger);
    MockLogger* mockLoggerPtr = mockLogger.get();
    bridge->setLoggerForTesting(mockLogger.release());

    // Test entering hibernation
    OwnPtr<WaitableEvent> hibernationStartedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationScheduled));
    EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
        .WillOnce(testing::Invoke(hibernationStartedEvent.get(), &WaitableEvent::signal));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), true));
    hibernationStartedEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);

    // Test prepareMailbox while hibernating
    WebExternalTextureMailbox mailbox;
    EXPECT_FALSE(bridge->prepareMailbox(&mailbox, 0));
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Tear down the bridge on the thread so that 'bridge' can go out of scope
    // without crashing due to thread checks
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationEndedWithTeardown));
    OwnPtr<WaitableEvent> bridgeDestroyedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new DestroyBridgeTask(&bridge, bridgeDestroyedEvent.get()));
    bridgeDestroyedEvent->wait();
}

TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhileBackgroundRendering)
{
    MockCanvasContext mainMock;
    OwnPtr<WebThread> testThread = adoptPtr(Platform::current()->createThread("TestThread"));

    // The Canvas2DLayerBridge has to be created on the thread that will use it
    // to avoid WeakPtr thread check issues.
    Canvas2DLayerBridgePtr bridge;
    OwnPtr<WaitableEvent> bridgeCreatedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new CreateBridgeTask(&bridge, &mainMock, this, bridgeCreatedEvent.get()));
    bridgeCreatedEvent->wait();

    // Register an alternate Logger for tracking hibernation events
    OwnPtr<MockLogger> mockLogger = adoptPtr(new MockLogger);
    MockLogger* mockLoggerPtr = mockLogger.get();
    bridge->setLoggerForTesting(mockLogger.release());

    // Test entering hibernation
    OwnPtr<WaitableEvent> hibernationStartedEvent = adoptPtr(new WaitableEvent());
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationScheduled));
    EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
        .WillOnce(testing::Invoke(hibernationStartedEvent.get(), &WaitableEvent::signal));
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new SetIsHiddenTask(bridge.get(), true));
    hibernationStartedEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);

    // Rendering in the background -> temp switch to SW
    EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(Canvas2DLayerBridge::HibernationEndedWithSwitchToBackgroundRendering));
    OwnPtr<WaitableEvent> switchEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new RenderingTask(bridge.get(), switchEvent.get()));
    switchEvent->wait();
    ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
    EXPECT_FALSE(bridge->isAccelerated());
    EXPECT_FALSE(bridge->isHibernating());
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Test prepareMailbox while background rendering
    WebExternalTextureMailbox mailbox;
    EXPECT_FALSE(bridge->prepareMailbox(&mailbox, 0));
    EXPECT_TRUE(bridge->checkSurfaceValid());

    // Tear down the bridge on the thread so that 'bridge' can go out of scope
    // without crashing due to thread checks
    OwnPtr<WaitableEvent> bridgeDestroyedEvent = adoptPtr(new WaitableEvent());
    testThread->taskRunner()->postTask(BLINK_FROM_HERE, new DestroyBridgeTask(&bridge, bridgeDestroyedEvent.get()));
    bridgeDestroyedEvent->wait();
}

} // namespace blink
