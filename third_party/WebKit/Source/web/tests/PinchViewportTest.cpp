// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/frame/PinchViewport.h"

#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "public/platform/Platform.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "web/tests/URLTestHelpers.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define EXPECT_FLOAT_POINT_EQ(expected, actual) \
    do { \
        EXPECT_FLOAT_EQ((expected).x(), (actual).x()); \
        EXPECT_FLOAT_EQ((expected).y(), (actual).y()); \
    } while (false)

#define EXPECT_SIZE_EQ(expected, actual) \
    do { \
        EXPECT_EQ((expected).width(), (actual).width()); \
        EXPECT_EQ((expected).height(), (actual).height()); \
    } while (false)

#define EXPECT_FLOAT_RECT_EQ(expected, actual) \
    do { \
        EXPECT_FLOAT_EQ((expected).x(), (actual).x()); \
        EXPECT_FLOAT_EQ((expected).y(), (actual).y()); \
        EXPECT_FLOAT_EQ((expected).width(), (actual).width()); \
        EXPECT_FLOAT_EQ((expected).height(), (actual).height()); \
    } while (false)


using namespace WebCore;
using namespace blink;

namespace {

class PinchViewportTest : public testing::Test {
public:
    PinchViewportTest()
        : m_baseURL("http://www.test.com/")
    {
        m_helper.initialize(true, 0, &m_mockWebViewClient, &configureSettings);
    }

    virtual ~PinchViewportTest()
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    void navigateTo(const std::string& url)
    {
        FrameTestHelpers::loadFrame(webViewImpl()->mainFrame(), url);
        Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    }

    void forceFullCompositingUpdate()
    {
        webViewImpl()->layout();
    }

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(fileName.c_str()));
    }

    WebLayer* getRootScrollLayer()
    {
        RenderLayerCompositor* compositor = frame()->contentRenderer()->compositor();
        ASSERT(compositor);
        ASSERT(compositor->scrollLayer());

        WebLayer* webScrollLayer = compositor->scrollLayer()->platformLayer();
        return webScrollLayer;
    }

    WebViewImpl* webViewImpl() const { return m_helper.webViewImpl(); }
    LocalFrame* frame() const { return m_helper.webViewImpl()->mainFrameImpl()->frame(); }

protected:
    std::string m_baseURL;
    FrameTestHelpers::TestWebViewClient m_mockWebViewClient;

private:
    static void configureSettings(WebSettings* settings)
    {
        settings->setJavaScriptEnabled(true);
        settings->setForceCompositingMode(true);
        settings->setAcceleratedCompositingEnabled(true);
        settings->setAcceleratedCompositingForFixedPositionEnabled(true);
        settings->setAcceleratedCompositingForOverflowScrollEnabled(true);
        settings->setCompositedScrollingForFramesEnabled(true);
        settings->setPinchVirtualViewportEnabled(true);
    }

    FrameTestHelpers::WebViewHelper m_helper;
};

// Test that resizing the PinchViewport works as expected. Note that resizes occur on the
// unscaled viewport, that is, they affect the size of the viewport at minimum scale. The
// pinch viewport must always be wholly contained by the main frame so setting a larger
// size should be clamped.
TEST_F(PinchViewportTest, TestResize)
{
    webViewImpl()->resize(IntSize(320, 240));
    navigateTo("about:blank");
    forceFullCompositingUpdate();

    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();

    IntSize initialSize = webViewImpl()->size();
    IntSize webViewSize = webViewImpl()->size();

    // Make sure the pinch viewport was initialized.
    EXPECT_SIZE_EQ(webViewSize, pinchViewport.size());

    // Resizing the frame to be bigger shouldn't change the pinch viewport's size (it's
    // changed on the first resize if it doesn't yet have a size).
    webViewSize = IntSize(640, 480);
    webViewImpl()->resize(webViewSize);
    EXPECT_SIZE_EQ(webViewSize, IntSize(webViewImpl()->size()));
    EXPECT_SIZE_EQ(initialSize, pinchViewport.size());

    IntSize newViewportSize = IntSize(320, 200);
    pinchViewport.setSize(newViewportSize);
    EXPECT_SIZE_EQ(webViewSize, IntSize(webViewImpl()->size()));
    EXPECT_SIZE_EQ(newViewportSize, pinchViewport.size());

    webViewSize = IntSize(400, 240);
    webViewImpl()->resize(webViewSize);
    EXPECT_SIZE_EQ(webViewSize, IntSize(webViewImpl()->size()));
    EXPECT_SIZE_EQ(newViewportSize, pinchViewport.size());

    // If the frame is resized to smaller than the viewport, the viewport must be resized
    // to stay within the frame's bounds.
    webViewSize = IntSize(160, 80);
    webViewImpl()->resize(webViewSize);
    EXPECT_SIZE_EQ(webViewSize, IntSize(webViewImpl()->size()));
    EXPECT_SIZE_EQ(webViewSize, pinchViewport.size());

    // Resize the pinch viewport to larger than the main frame, should be clamped to main
    // frame.
    newViewportSize = IntSize(400, 300);
    pinchViewport.setSize(newViewportSize);
    EXPECT_SIZE_EQ(webViewSize, pinchViewport.size());

    // Try again but only with one dimension clamped.
    webViewSize = IntSize(320, 240);
    newViewportSize = IntSize(400, 200);
    webViewImpl()->resize(webViewSize);
    pinchViewport.setSize(newViewportSize);
    EXPECT_SIZE_EQ(IntSize(webViewSize.width(), newViewportSize.height()),
        pinchViewport.size());
}

// Make sure that the visibleRect method acurately reflects the scale and scroll location
// of the viewport.
TEST_F(PinchViewportTest, TestVisibleRect)
{
    webViewImpl()->resize(IntSize(320, 240));
    navigateTo("about:blank");
    forceFullCompositingUpdate();

    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();

    // Initial visible rect should be the whole frame.
    EXPECT_SIZE_EQ(IntSize(webViewImpl()->size()), pinchViewport.size());

    // Viewport is whole frame.
    IntSize size = IntSize(400, 200);
    webViewImpl()->resize(size);
    pinchViewport.setSize(size);

    // Scale the viewport to 2X; size should not change.
    FloatRect expectedRect(FloatPoint(0, 0), size);
    expectedRect.scale(0.5);
    pinchViewport.setScale(2);
    EXPECT_EQ(2, pinchViewport.scale());
    EXPECT_SIZE_EQ(size, pinchViewport.size());
    EXPECT_FLOAT_RECT_EQ(expectedRect, pinchViewport.visibleRect());

    // Move the viewport.
    expectedRect.setLocation(FloatPoint(5, 7));
    pinchViewport.setLocation(expectedRect.location());
    EXPECT_FLOAT_RECT_EQ(expectedRect, pinchViewport.visibleRect());

    expectedRect.setLocation(FloatPoint(200, 100));
    pinchViewport.setLocation(expectedRect.location());
    EXPECT_FLOAT_RECT_EQ(expectedRect, pinchViewport.visibleRect());

    // Scale the viewport to 3X to introduce some non-int values.
    FloatPoint oldLocation = expectedRect.location();
    expectedRect = FloatRect(FloatPoint(), size);
    expectedRect.scale(1 / 3.0f);
    expectedRect.setLocation(oldLocation);
    pinchViewport.setScale(3);
    EXPECT_FLOAT_RECT_EQ(expectedRect, pinchViewport.visibleRect());

    expectedRect.setLocation(FloatPoint(0.25f, 0.333f));
    pinchViewport.setLocation(expectedRect.location());
    EXPECT_FLOAT_RECT_EQ(expectedRect, pinchViewport.visibleRect());
}

// Test that the viewport's scroll offset is always appropriately bounded such that the
// pinch viewport always stays within the bounds of the main frame.
TEST_F(PinchViewportTest, TestOffsetClamping)
{
    webViewImpl()->resize(IntSize(320, 240));
    navigateTo("about:blank");
    forceFullCompositingUpdate();

    // Pinch viewport should be initialized to same size as frame so no scrolling possible.
    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());

    pinchViewport.setLocation(FloatPoint(-1, -2));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());

    pinchViewport.setLocation(FloatPoint(100, 200));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());

    pinchViewport.setLocation(FloatPoint(-5, 10));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());

    // Scale by 2x. The viewport's visible rect should now have a size of 160x120.
    pinchViewport.setScale(2);
    FloatPoint location(10, 50);
    pinchViewport.setLocation(location);
    EXPECT_FLOAT_POINT_EQ(location, pinchViewport.visibleRect().location());

    pinchViewport.setLocation(FloatPoint(1000, 2000));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(160, 120), pinchViewport.visibleRect().location());

    pinchViewport.setLocation(FloatPoint(-1000, -2000));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());

    // Make sure offset gets clamped on scale out. Scale to 1.25 so the viewport is 256x192.
    pinchViewport.setLocation(FloatPoint(160, 120));
    pinchViewport.setScale(1.25);
    EXPECT_FLOAT_POINT_EQ(FloatPoint(64, 48), pinchViewport.visibleRect().location());

    // Scale out smaller than 1.
    pinchViewport.setScale(0.25);
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());
}

// Test that the viewport can be scrolled around only within the main frame in the presence
// of viewport resizes, as would be the case if the on screen keyboard came up.
TEST_F(PinchViewportTest, TestOffsetClampingWithResize)
{
    webViewImpl()->resize(IntSize(320, 240));
    navigateTo("about:blank");
    forceFullCompositingUpdate();

    // Pinch viewport should be initialized to same size as frame so no scrolling possible.
    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());

    // Shrink the viewport vertically. The resize shouldn't affect the location, but it
    // should allow vertical scrolling.
    pinchViewport.setSize(IntSize(320, 200));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(10, 20));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 20), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(0, 100));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 40), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(0, 10));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 10), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(0, -100));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());

    // Repeat the above but for horizontal dimension.
    pinchViewport.setSize(IntSize(280, 240));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(10, 20));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 0), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(100, 0));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(40, 0), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(10, 0));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 0), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(-100, 0));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());

    // Now with both dimensions.
    pinchViewport.setSize(IntSize(280, 200));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(10, 20));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 20), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(100, 100));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(40, 40), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(10, 3));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 3), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(-10, -4));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());
}

// Test that the viewport is scrollable but bounded appropriately within the main frame
// when we apply both scaling and resizes.
TEST_F(PinchViewportTest, TestOffsetClampingWithResizeAndScale)
{
    webViewImpl()->resize(IntSize(320, 240));
    navigateTo("about:blank");
    forceFullCompositingUpdate();

    // Pinch viewport should be initialized to same size as frame so no scrolling possible.
    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), pinchViewport.visibleRect().location());

    // Zoom in to 2X so we can scroll the viewport to 160x120.
    pinchViewport.setScale(2);
    pinchViewport.setLocation(FloatPoint(200, 200));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(160, 120), pinchViewport.visibleRect().location());

    // Now resize the viewport to make it 10px smaller. Since we're zoomed in by 2X it should
    // allow us to scroll by 5px more.
    pinchViewport.setSize(IntSize(310, 230));
    pinchViewport.setLocation(FloatPoint(200, 200));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(165, 125), pinchViewport.visibleRect().location());

    // Even though we're zoomed in, we shouldn't be able to resize the viewport larger than
    // the main frame (currently 320, 240).
    pinchViewport.setSize(IntSize(330, 250));
    EXPECT_SIZE_EQ(IntSize(320, 240), pinchViewport.size());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(160, 120), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(200, 200));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(160, 120), pinchViewport.visibleRect().location());

    // Resize the viewport to make it 10px larger. The max scrollable position should be
    // reduced by 5px. The current offset should also be changed to keep the viewport
    // bounded by the main frame.
    webViewImpl()->resize(IntSize(640, 480));
    pinchViewport.setLocation(FloatPoint(1000, 1000));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(480, 360), pinchViewport.visibleRect().location());

    pinchViewport.setSize(IntSize(330, 250));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(475, 355), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(500, 400));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(475, 355), pinchViewport.visibleRect().location());

    // Make sure resizing the frame doesn't move the viewport if the resize doesn't make
    // the viewport go out of bounds.
    pinchViewport.setLocation(FloatPoint(200, 200));
    webViewImpl()->resize(IntSize(600, 400));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(200, 200), pinchViewport.visibleRect().location());

    // Resizing the main frame such that the viewport is out of bounds should move the
    // viewport.
    pinchViewport.setSize(IntSize(320, 250));
    webViewImpl()->resize(IntSize(340, 260));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(180, 135), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(1000, 1000));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(180, 135), pinchViewport.visibleRect().location());
}

// Test that the pinch viewport still gets sized in AutoSize/AutoResize mode.
TEST_F(PinchViewportTest, TestPinchViewportGetsSizeInAutoSizeMode)
{
    EXPECT_SIZE_EQ(IntSize(0, 0), IntSize(webViewImpl()->size()));
    EXPECT_SIZE_EQ(IntSize(0, 0), frame()->page()->frameHost().pinchViewport().size());

    webViewImpl()->enableAutoResizeMode(WebSize(10, 10), WebSize(1000, 1000));

    registerMockedHttpURLLoad("200-by-300.html");
    navigateTo(m_baseURL + "200-by-300.html");

    EXPECT_SIZE_EQ(IntSize(200, 300), frame()->page()->frameHost().pinchViewport().size());
}
} // namespace
