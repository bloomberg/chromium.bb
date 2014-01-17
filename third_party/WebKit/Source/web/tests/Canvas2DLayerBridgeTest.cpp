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

#include "SkDeferredCanvas.h"
#include "SkSurface.h"
#include "platform/graphics/ImageBuffer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "web/tests/MockWebGraphicsContext3D.h"
#include "wtf/RefPtr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using namespace blink;
using testing::InSequence;
using testing::Return;
using testing::Test;

namespace {

class MockCanvasContext : public MockWebGraphicsContext3D {
public:
    MOCK_METHOD0(flush, void(void));
    MOCK_METHOD0(createTexture, unsigned(void));
    MOCK_METHOD1(deleteTexture, void(unsigned));
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

} // namespace

class Canvas2DLayerBridgeTest : public Test {
protected:
    void fullLifecycleTest()
    {
        MockCanvasContext mainMock;
        RefPtr<GraphicsContext3D> mainContext = GraphicsContext3D::createContextSupport(&mainMock);

        OwnPtr<SkDeferredCanvas> canvas = adoptPtr(SkDeferredCanvas::Create(SkSurface::NewRasterPMColor(300, 150)));

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);

        {
            Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(mainContext.release(), canvas.release(), 0, NonOpaque)));

            ::testing::Mock::VerifyAndClearExpectations(&mainMock);

            EXPECT_CALL(mainMock, flush());
            unsigned textureId = bridge->getBackingTexture();
            EXPECT_EQ(textureId, 0u);

            ::testing::Mock::VerifyAndClearExpectations(&mainMock);
        } // bridge goes out of scope here

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);
    }
};

namespace {

TEST_F(Canvas2DLayerBridgeTest, testFullLifecycleSingleThreaded)
{
    fullLifecycleTest();
}

} // namespace
