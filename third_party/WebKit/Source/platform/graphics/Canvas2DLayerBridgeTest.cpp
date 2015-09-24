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

#include "config.h"
#include "platform/graphics/Canvas2DLayerBridge.h"

#include "SkSurface.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/test/MockWebGraphicsContext3D.h"
#include "public/platform/Platform.h"
#include "public/platform/WebExternalBitmap.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/WebThread.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/SkNullGLContext.h"
#include "wtf/RefPtr.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::InSequence;
using testing::Return;
using testing::Test;

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
        RefPtr<SkGLContext> glContext = adoptRef(SkNullGLContext::Create(kNone_GrGLStandard));
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
    Canvas2DLayerBridgePtr(PassRefPtr<Canvas2DLayerBridge> layerBridge)
        : m_layerBridge(layerBridge) { }

    ~Canvas2DLayerBridgePtr()
    {
        m_layerBridge->beginDestruction();
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

    uint8* pixels() override
    {
        return nullptr;
    }
};

} // anonymous namespace

class Canvas2DLayerBridgeTest : public Test {
protected:
    void fullLifecycleTest()
    {
        MockCanvasContext mainMock;
        OwnPtr<MockWebGraphicsContext3DProvider> mainMockProvider = adoptPtr(new MockWebGraphicsContext3DProvider(&mainMock));

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);

        {
            Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(mainMockProvider.release(), IntSize(300, 150), 0, NonOpaque, Canvas2DLayerBridge::DisableAcceleration)));

            ::testing::Mock::VerifyAndClearExpectations(&mainMock);

            unsigned textureId = bridge->newImageSnapshot(PreferAcceleration)->getTextureHandle(true);
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
            RefPtr<SkImage> snapshot = bridge->newImageSnapshot(PreferAcceleration);
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
            RefPtr<SkImage> snapshot = bridge->newImageSnapshot(PreferAcceleration);
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
            RefPtr<SkImage> image = bridge->newImageSnapshot(PreferAcceleration);
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
            RefPtr<SkImage> image = bridge->newImageSnapshot(PreferNoAcceleration);
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

} // namespace blink
