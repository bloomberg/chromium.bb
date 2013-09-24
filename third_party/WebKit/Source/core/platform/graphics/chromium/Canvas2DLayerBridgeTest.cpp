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

#include "core/platform/graphics/chromium/Canvas2DLayerBridge.h"

#include "SkDeferredCanvas.h"
#include "SkSurface.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/tests/FakeWebGraphicsContext3D.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "wtf/RefPtr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKit;
using testing::InSequence;
using testing::Return;
using testing::Test;

namespace {

class MockCanvasContext : public FakeWebGraphicsContext3D {
public:
    MOCK_METHOD0(flush, void(void));
    MOCK_METHOD0(createTexture, unsigned(void));
    MOCK_METHOD1(deleteTexture, void(unsigned));

    virtual GrGLInterface* onCreateGrGLInterface() OVERRIDE { return 0; }
};

class FakeCanvas2DLayerBridge : public Canvas2DLayerBridge {
public:
    static PassRefPtr<Canvas2DLayerBridge> create(PassRefPtr<GraphicsContext3D> context, SkDeferredCanvas* canvas, OpacityMode opacityMode)
    {
        return adoptRef(static_cast<Canvas2DLayerBridge*>(new FakeCanvas2DLayerBridge(context, canvas, opacityMode)));
    }
protected:
    FakeCanvas2DLayerBridge(PassRefPtr<GraphicsContext3D> context, SkDeferredCanvas* canvas, OpacityMode opacityMode) :
        Canvas2DLayerBridge(context, canvas, opacityMode)
    { }
};

} // namespace

class Canvas2DLayerBridgeTest : public Test {
protected:
    void fullLifecycleTest()
    {
        RefPtr<GraphicsContext3D> mainContext = GraphicsContext3D::createGraphicsContextFromWebContext(adoptPtr(new MockCanvasContext));

        MockCanvasContext& mainMock = *static_cast<MockCanvasContext*>(mainContext->webContext());

        SkAutoTUnref<SkDeferredCanvas> canvas(SkDeferredCanvas::Create(SkSurface::NewRasterPMColor(300, 150)));

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);

        Canvas2DLayerBridgePtr bridge = FakeCanvas2DLayerBridge::create(mainContext.release(), canvas.get(), Canvas2DLayerBridge::NonOpaque);

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);

        EXPECT_CALL(mainMock, flush());
        unsigned textureId = bridge->backBufferTexture();
        EXPECT_EQ(textureId, 0u);

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);

        bridge.clear();

        ::testing::Mock::VerifyAndClearExpectations(&mainMock);
    }
};

namespace {

TEST_F(Canvas2DLayerBridgeTest, testFullLifecycleSingleThreaded)
{
    fullLifecycleTest();
}

} // namespace
