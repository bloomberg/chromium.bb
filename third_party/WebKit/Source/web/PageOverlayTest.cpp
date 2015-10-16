// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/PageOverlay.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutView.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCanvas.h"
#include "public/platform/WebThread.h"
#include "public/web/WebSettings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "web/WebGraphicsContextImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::_;
using testing::AtLeast;
using testing::Property;

namespace blink {
namespace {

static const int viewportWidth = 800;
static const int viewportHeight = 600;

// These unit tests cover both PageOverlay and PageOverlayList.

void enableAcceleratedCompositing(WebSettings* settings)
{
    settings->setAcceleratedCompositingEnabled(true);
}

void disableAcceleratedCompositing(WebSettings* settings)
{
    settings->setAcceleratedCompositingEnabled(false);
}

class PageOverlayTest : public ::testing::Test {
protected:
    enum CompositingMode { AcceleratedCompositing, UnacceleratedCompositing };

    void initialize(CompositingMode compositingMode)
    {
        m_helper.initialize(
            false /* enableJavascript */, nullptr /* webFrameClient */, nullptr /* webViewClient */,
            compositingMode == AcceleratedCompositing ? enableAcceleratedCompositing : disableAcceleratedCompositing);
        webViewImpl()->resize(WebSize(viewportWidth, viewportHeight));
        webViewImpl()->layout();
        ASSERT_EQ(compositingMode == AcceleratedCompositing, webViewImpl()->isAcceleratedCompositingActive());
    }

    WebViewImpl* webViewImpl() const { return m_helper.webViewImpl(); }

    template <typename OverlayType>
    void runPageOverlayTestWithAcceleratedCompositing();

private:
    FrameTestHelpers::WebViewHelper m_helper;
};

// PageOverlay that uses a WebCanvas to draw a solid color.
class SimpleCanvasOverlay : public PageOverlay::Delegate {
public:
    SimpleCanvasOverlay(SkColor color) : m_color(color) { }

    void paintPageOverlay(WebGraphicsContext* context, const WebSize& size) const override
    {
        WebFloatRect rect(0, 0, size.width, size.height);
        WebCanvas* canvas = context->beginDrawing(rect);
        SkPaint paint;
        paint.setColor(m_color);
        paint.setStyle(SkPaint::kFill_Style);
        canvas->drawRectCoords(0, 0, size.width, size.height, paint);
        context->endDrawing();
    }

private:
    SkColor m_color;
};

// PageOverlay that uses the underlying blink::GraphicsContext to paint a
// solid color.
class PrivateGraphicsContextOverlay : public PageOverlay::Delegate {
public:
    PrivateGraphicsContextOverlay(Color color) : m_color(color) { }

    void paintPageOverlay(WebGraphicsContext* context, const WebSize& size) const override
    {
        GraphicsContext& graphicsContext = toWebGraphicsContextImpl(context)->graphicsContext();
        if (DrawingRecorder::useCachedDrawingIfPossible(graphicsContext, *this, DisplayItem::PageOverlay))
            return;
        FloatRect rect(0, 0, size.width, size.height);
        DrawingRecorder drawingRecorder(graphicsContext, *this, DisplayItem::PageOverlay, rect);
        graphicsContext.fillRect(rect, m_color);
    }

    DisplayItemClient displayItemClient() const { return toDisplayItemClient(this); }
    String debugName() const { return "PrivateGraphicsContextOverlay"; }

private:
    Color m_color;
};

template <bool(*getter)(), void(*setter)(bool)>
class RuntimeFeatureChange {
public:
    RuntimeFeatureChange(bool newValue) : m_oldValue(getter()) { setter(newValue); }
    ~RuntimeFeatureChange() { setter(m_oldValue); }
private:
    bool m_oldValue;
};

class MockCanvas : public SkCanvas {
public:
    MockCanvas(int width, int height) : SkCanvas(width, height) { }
    MOCK_METHOD2(onDrawRect, void(const SkRect&, const SkPaint&));
};

template <typename OverlayType>
void PageOverlayTest::runPageOverlayTestWithAcceleratedCompositing()
{
    initialize(AcceleratedCompositing);
    webViewImpl()->layerTreeView()->setViewportSize(WebSize(viewportWidth, viewportHeight));

    OwnPtr<PageOverlay> pageOverlay = PageOverlay::create(webViewImpl(), new OverlayType(SK_ColorYELLOW));
    pageOverlay->update();
    webViewImpl()->layout();

    // Ideally, we would get results from the compositor that showed that this
    // page overlay actually winds up getting drawn on top of the rest.
    // For now, we just check that the GraphicsLayer will draw the right thing.

    MockCanvas canvas(viewportWidth, viewportHeight);
    EXPECT_CALL(canvas, onDrawRect(_, _)).Times(AtLeast(0));
    EXPECT_CALL(canvas, onDrawRect(SkRect::MakeWH(viewportWidth, viewportHeight), Property(&SkPaint::getColor, SK_ColorYELLOW)));

    GraphicsLayer* graphicsLayer = pageOverlay->graphicsLayer();
    WebRect rect(0, 0, viewportWidth, viewportHeight);

    // Paint the layer with a null canvas to get a display list, and then
    // replay that onto the mock canvas for examination.
    PaintController* paintController = graphicsLayer->paintController();
    ASSERT(paintController);
    GraphicsContext graphicsContext(*paintController);
    graphicsLayer->paint(graphicsContext, rect);

    graphicsContext.beginRecording(IntRect(rect));
    paintController->commitNewDisplayItems();
    paintController->paintArtifact().replay(graphicsContext);
    graphicsContext.endRecording()->playback(&canvas);
}

TEST_F(PageOverlayTest, SimpleCanvasOverlay_AcceleratedCompositing)
{
    runPageOverlayTestWithAcceleratedCompositing<SimpleCanvasOverlay>();
}

TEST_F(PageOverlayTest, PrivateGraphicsContextOverlay_AcceleratedCompositing)
{
    runPageOverlayTestWithAcceleratedCompositing<PrivateGraphicsContextOverlay>();
}

} // namespace
} // namespace blink
