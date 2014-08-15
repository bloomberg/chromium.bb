// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/frame/PinchViewport.h"

#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "public/platform/Platform.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebContextMenuData.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "web/tests/URLTestHelpers.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define EXPECT_POINT_EQ(expected, actual) \
    do { \
        EXPECT_EQ((expected).x(), (actual).x()); \
        EXPECT_EQ((expected).y(), (actual).y()); \
    } while (false)

#define EXPECT_FLOAT_POINT_EQ(expected, actual) \
    do { \
        EXPECT_FLOAT_EQ((expected).x(), (actual).x()); \
        EXPECT_FLOAT_EQ((expected).y(), (actual).y()); \
    } while (false)

#define EXPECT_POINT_EQ(expected, actual) \
    do { \
        EXPECT_EQ((expected).x(), (actual).x()); \
        EXPECT_EQ((expected).y(), (actual).y()); \
    } while (false)

#define EXPECT_SIZE_EQ(expected, actual) \
    do { \
        EXPECT_EQ((expected).width(), (actual).width()); \
        EXPECT_EQ((expected).height(), (actual).height()); \
    } while (false)

#define EXPECT_FLOAT_SIZE_EQ(expected, actual) \
    do { \
        EXPECT_FLOAT_EQ((expected).width(), (actual).width()); \
        EXPECT_FLOAT_EQ((expected).height(), (actual).height()); \
    } while (false)

#define EXPECT_FLOAT_RECT_EQ(expected, actual) \
    do { \
        EXPECT_FLOAT_EQ((expected).x(), (actual).x()); \
        EXPECT_FLOAT_EQ((expected).y(), (actual).y()); \
        EXPECT_FLOAT_EQ((expected).width(), (actual).width()); \
        EXPECT_FLOAT_EQ((expected).height(), (actual).height()); \
    } while (false)


using namespace blink;
using namespace blink;

using ::testing::_;
using ::testing::PrintToString;
using ::testing::Mock;

namespace blink {
::std::ostream& operator<<(::std::ostream& os, const WebContextMenuData& data)
{
    return os << "Context menu location: ["
        << data.mousePosition.x << ", " << data.mousePosition.y << "]";
}
}


namespace {

class PinchViewportTest : public testing::Test {
public:
    PinchViewportTest()
        : m_baseURL("http://www.test.com/")
    {
    }

    void initializeWithDesktopSettings(void (*overrideSettingsFunc)(WebSettings*) = 0)
    {
        if (!overrideSettingsFunc)
            overrideSettingsFunc = &configureSettings;
        m_helper.initialize(true, 0, &m_mockWebViewClient, overrideSettingsFunc);
        webViewImpl()->setPageScaleFactorLimits(1, 4);
    }

    void initializeWithAndroidSettings(void (*overrideSettingsFunc)(WebSettings*) = 0)
    {
        if (!overrideSettingsFunc)
            overrideSettingsFunc = &configureAndroidSettings;
        m_helper.initialize(true, 0, &m_mockWebViewClient, overrideSettingsFunc);
    }

    virtual ~PinchViewportTest()
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    void navigateTo(const std::string& url)
    {
        FrameTestHelpers::loadFrame(webViewImpl()->mainFrame(), url);
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

    static void configureSettings(WebSettings* settings)
    {
        settings->setJavaScriptEnabled(true);
        settings->setAcceleratedCompositingEnabled(true);
        settings->setPreferCompositingToLCDTextEnabled(true);
        settings->setAcceleratedCompositingForOverflowScrollEnabled(true);
        settings->setCompositedScrollingForFramesEnabled(true);
        settings->setPinchVirtualViewportEnabled(true);
    }

    static void configureAndroidSettings(WebSettings* settings)
    {
        configureSettings(settings);
        settings->setViewportEnabled(true);
        settings->setViewportMetaEnabled(true);
        settings->setShrinksViewportContentToFit(true);
    }

protected:
    std::string m_baseURL;
    FrameTestHelpers::TestWebViewClient m_mockWebViewClient;

private:
    FrameTestHelpers::WebViewHelper m_helper;
};

// Test that resizing the PinchViewport works as expected and that resizing the
// WebView resizes the PinchViewport.
TEST_F(PinchViewportTest, TestResize)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(320, 240));

    navigateTo("about:blank");
    forceFullCompositingUpdate();

    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();

    IntSize webViewSize = webViewImpl()->size();

    // Make sure the pinch viewport was initialized.
    EXPECT_SIZE_EQ(webViewSize, pinchViewport.size());

    // Resizing the WebView should change the PinchViewport.
    webViewSize = IntSize(640, 480);
    webViewImpl()->resize(webViewSize);
    EXPECT_SIZE_EQ(webViewSize, IntSize(webViewImpl()->size()));
    EXPECT_SIZE_EQ(webViewSize, pinchViewport.size());

    // Resizing the pinch viewport shouldn't affect the WebView.
    IntSize newViewportSize = IntSize(320, 200);
    pinchViewport.setSize(newViewportSize);
    EXPECT_SIZE_EQ(webViewSize, IntSize(webViewImpl()->size()));
    EXPECT_SIZE_EQ(newViewportSize, pinchViewport.size());
}

static void disableAcceleratedCompositing(WebSettings* settings)
{
    PinchViewportTest::configureSettings(settings);
    // FIXME: This setting is being removed, so this test needs to be rewritten to
    // do something else. crbug.com/173949
    settings->setAcceleratedCompositingEnabled(false);
}

// Test that the container layer gets sized properly if the WebView is resized
// prior to the PinchViewport being attached to the layer tree.
TEST_F(PinchViewportTest, TestWebViewResizedBeforeAttachment)
{
    initializeWithDesktopSettings(disableAcceleratedCompositing);
    webViewImpl()->resize(IntSize(320, 240));

    navigateTo("about:blank");
    forceFullCompositingUpdate();
    webViewImpl()->settings()->setAcceleratedCompositingEnabled(true);
    webViewImpl()->layout();

    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();
    EXPECT_FLOAT_SIZE_EQ(FloatSize(320, 240), pinchViewport.containerLayer()->size());
}
// Make sure that the visibleRect method acurately reflects the scale and scroll location
// of the viewport.
TEST_F(PinchViewportTest, TestVisibleRect)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(320, 240));

    navigateTo("about:blank");
    forceFullCompositingUpdate();

    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();

    // Initial visible rect should be the whole frame.
    EXPECT_SIZE_EQ(IntSize(webViewImpl()->size()), pinchViewport.size());

    // Viewport is whole frame.
    IntSize size = IntSize(400, 200);
    webViewImpl()->resize(size);
    webViewImpl()->layout();
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
    initializeWithDesktopSettings();
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
    initializeWithDesktopSettings();
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
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(320, 240));

    navigateTo("about:blank");
    forceFullCompositingUpdate();

    // Pinch viewport should be initialized to same size as WebView so no scrolling possible.
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

    // The viewport can be larger than the main frame (currently 320, 240) though typically
    // the scale will be clamped to prevent it from actually being larger. Make sure size
    // changes clamp the offset so the inner remains within the outer.
    pinchViewport.setSize(IntSize(330, 250));
    EXPECT_SIZE_EQ(IntSize(330, 250), pinchViewport.size());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(155, 115), pinchViewport.visibleRect().location());
    pinchViewport.setLocation(FloatPoint(200, 200));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(155, 115), pinchViewport.visibleRect().location());

    // Resize both the viewport and the frame to be larger.
    webViewImpl()->resize(IntSize(640, 480));
    webViewImpl()->layout();
    EXPECT_SIZE_EQ(IntSize(webViewImpl()->size()), pinchViewport.size());
    EXPECT_SIZE_EQ(IntSize(webViewImpl()->size()), frame()->view()->frameRect().size());
    pinchViewport.setLocation(FloatPoint(1000, 1000));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(320, 240), pinchViewport.visibleRect().location());

    // Make sure resizing the viewport doesn't change its offset if the resize doesn't make
    // the viewport go out of bounds.
    pinchViewport.setLocation(FloatPoint(200, 200));
    pinchViewport.setSize(IntSize(880, 560));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(200, 200), pinchViewport.visibleRect().location());

    // Resizing the viewport such that the viewport is out of bounds should move the
    // viewport.
    pinchViewport.setSize(IntSize(920, 640));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(180, 160), pinchViewport.visibleRect().location());
}

// The main FrameView's size should be set such that its the size of the pinch viewport
// at minimum scale. If there's no explicit minimum scale set, the FrameView should be
// set to the content width and height derived by the aspect ratio.
TEST_F(PinchViewportTest, TestFrameViewSizedToContent)
{
    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(320, 240));

    registerMockedHttpURLLoad("200-by-300-viewport.html");
    navigateTo(m_baseURL + "200-by-300-viewport.html");

    webViewImpl()->resize(IntSize(600, 800));
    webViewImpl()->layout();

    EXPECT_SIZE_EQ(IntSize(200, 266),
        webViewImpl()->mainFrameImpl()->frameView()->frameRect().size());
}

// The main FrameView's size should be set such that its the size of the pinch viewport
// at minimum scale. On Desktop, the minimum scale is set at 1 so make sure the FrameView
// is sized to the viewport.
TEST_F(PinchViewportTest, TestFrameViewSizedToMinimumScale)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(320, 240));

    registerMockedHttpURLLoad("200-by-300.html");
    navigateTo(m_baseURL + "200-by-300.html");

    webViewImpl()->resize(IntSize(100, 160));
    webViewImpl()->layout();

    EXPECT_SIZE_EQ(IntSize(100, 160),
        webViewImpl()->mainFrameImpl()->frameView()->frameRect().size());
}

// The main FrameView's size should be set such that its the size of the pinch viewport
// at minimum scale. Test that the FrameView is appropriately sized in the presence
// of a viewport <meta> tag.
TEST_F(PinchViewportTest, TestFrameViewSizedToViewportMetaMinimumScale)
{
    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(320, 240));

    registerMockedHttpURLLoad("200-by-300-min-scale-2.html");
    navigateTo(m_baseURL + "200-by-300-min-scale-2.html");

    webViewImpl()->resize(IntSize(100, 160));
    webViewImpl()->layout();

    EXPECT_SIZE_EQ(IntSize(50, 80),
        webViewImpl()->mainFrameImpl()->frameView()->frameRect().size());
}

// Test that the pinch viewport still gets sized in AutoSize/AutoResize mode.
TEST_F(PinchViewportTest, TestPinchViewportGetsSizeInAutoSizeMode)
{
    initializeWithDesktopSettings();

    EXPECT_SIZE_EQ(IntSize(0, 0), IntSize(webViewImpl()->size()));
    EXPECT_SIZE_EQ(IntSize(0, 0), frame()->page()->frameHost().pinchViewport().size());

    webViewImpl()->enableAutoResizeMode(WebSize(10, 10), WebSize(1000, 1000));

    registerMockedHttpURLLoad("200-by-300.html");
    navigateTo(m_baseURL + "200-by-300.html");

    EXPECT_SIZE_EQ(IntSize(200, 300), frame()->page()->frameHost().pinchViewport().size());
}

// Test that the text selection handle's position accounts for the pinch viewport.
TEST_F(PinchViewportTest, TestTextSelectionHandles)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(500, 800));

    registerMockedHttpURLLoad("pinch-viewport-input-field.html");
    navigateTo(m_baseURL + "pinch-viewport-input-field.html");

    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();
    webViewImpl()->setInitialFocus(false);

    WebRect originalAnchor;
    WebRect originalFocus;
    webViewImpl()->selectionBounds(originalAnchor, originalFocus);

    webViewImpl()->setPageScaleFactor(2);
    pinchViewport.setLocation(FloatPoint(100, 400));

    WebRect anchor;
    WebRect focus;
    webViewImpl()->selectionBounds(anchor, focus);

    IntPoint expected(IntRect(originalAnchor).location());
    expected.moveBy(-flooredIntPoint(pinchViewport.visibleRect().location()));
    expected.scale(pinchViewport.scale(), pinchViewport.scale());

    EXPECT_POINT_EQ(expected, IntRect(anchor).location());
    EXPECT_POINT_EQ(expected, IntRect(focus).location());

    // FIXME(bokan) - http://crbug.com/364154 - Figure out how to test text selection
    // as well rather than just carret.
}

// Test that the HistoryItem for the page stores the pinch viewport's offset and scale.
TEST_F(PinchViewportTest, TestSavedToHistoryItem)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(200, 300));
    webViewImpl()->layout();

    registerMockedHttpURLLoad("200-by-300.html");
    navigateTo(m_baseURL + "200-by-300.html");

    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
        toLocalFrame(webViewImpl()->page()->mainFrame())->loader().currentItem()->pinchViewportScrollPoint());

    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();
    pinchViewport.setScale(2);

    EXPECT_EQ(2, toLocalFrame(webViewImpl()->page()->mainFrame())->loader().currentItem()->pageScaleFactor());

    pinchViewport.setLocation(FloatPoint(10, 20));

    EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 20),
        toLocalFrame(webViewImpl()->page()->mainFrame())->loader().currentItem()->pinchViewportScrollPoint());
}

// Test restoring a HistoryItem properly restores the pinch viewport's state.
TEST_F(PinchViewportTest, TestRestoredFromHistoryItem)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(200, 300));

    registerMockedHttpURLLoad("200-by-300.html");

    WebHistoryItem item;
    item.initialize();
    WebURL destinationURL(blink::URLTestHelpers::toKURL(m_baseURL + "200-by-300.html"));
    item.setURLString(destinationURL.string());
    item.setPinchViewportScrollOffset(WebFloatPoint(100, 120));
    item.setPageScaleFactor(2);

    FrameTestHelpers::loadHistoryItem(webViewImpl()->mainFrame(), item, WebHistoryDifferentDocumentLoad, WebURLRequest::UseProtocolCachePolicy);

    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();
    EXPECT_EQ(2, pinchViewport.scale());

    EXPECT_FLOAT_POINT_EQ(FloatPoint(100, 120), pinchViewport.visibleRect().location());
}

// Test restoring a HistoryItem without the pinch viewport offset falls back to distributing
// the scroll offset between the main frame and the pinch viewport.
TEST_F(PinchViewportTest, TestRestoredFromLegacyHistoryItem)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(100, 150));

    registerMockedHttpURLLoad("200-by-300-viewport.html");

    WebHistoryItem item;
    item.initialize();
    WebURL destinationURL(blink::URLTestHelpers::toKURL(m_baseURL + "200-by-300-viewport.html"));
    item.setURLString(destinationURL.string());
    // (-1, -1) will be used if the HistoryItem is an older version prior to having
    // pinch viewport scroll offset.
    item.setPinchViewportScrollOffset(WebFloatPoint(-1, -1));
    item.setScrollOffset(WebPoint(120, 180));
    item.setPageScaleFactor(2);

    FrameTestHelpers::loadHistoryItem(webViewImpl()->mainFrame(), item, WebHistoryDifferentDocumentLoad, WebURLRequest::UseProtocolCachePolicy);

    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();
    EXPECT_EQ(2, pinchViewport.scale());
    EXPECT_POINT_EQ(IntPoint(100, 150), frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(20, 30), pinchViewport.visibleRect().location());
}

// Test that the coordinates sent into moveRangeSelection are offset by the
// pinch viewport's location.
TEST_F(PinchViewportTest, TestWebFrameRangeAccountsForPinchViewportScroll)
{
    initializeWithDesktopSettings();
    webViewImpl()->settings()->setDefaultFontSize(12);
    webViewImpl()->resize(WebSize(640, 480));
    registerMockedHttpURLLoad("move_range.html");
    navigateTo(m_baseURL + "move_range.html");

    WebRect baseRect;
    WebRect extentRect;

    webViewImpl()->setPageScaleFactor(2);
    WebFrame* mainFrame = webViewImpl()->mainFrame();

    // Select some text and get the base and extent rects (that's the start of
    // the range and its end). Do a sanity check that the expected text is
    // selected
    mainFrame->executeScript(WebScriptSource("selectRange();"));
    EXPECT_EQ("ir", mainFrame->selectionAsText().utf8());

    webViewImpl()->selectionBounds(baseRect, extentRect);
    WebPoint initialPoint(baseRect.x, baseRect.y);
    WebPoint endPoint(extentRect.x, extentRect.y);

    // Move the pinch viewport over and make the selection in the same
    // screen-space location. The selection should change to two characters to
    // the right and down one line.
    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();
    pinchViewport.move(FloatPoint(60, 25));
    mainFrame->moveRangeSelection(initialPoint, endPoint);
    EXPECT_EQ("t ", mainFrame->selectionAsText().utf8());
}

// Test that the scrollFocusedNodeIntoRect method works with the pinch viewport.
TEST_F(PinchViewportTest, DISABLED_TestScrollFocusedNodeIntoRect)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(500, 300));

    registerMockedHttpURLLoad("pinch-viewport-input-field.html");
    navigateTo(m_baseURL + "pinch-viewport-input-field.html");

    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();
    webViewImpl()->resizePinchViewport(IntSize(200, 100));
    webViewImpl()->setInitialFocus(false);
    pinchViewport.setLocation(FloatPoint());
    webViewImpl()->scrollFocusedNodeIntoRect(IntRect(0, 0, 500, 200));

    EXPECT_POINT_EQ(IntPoint(0, frame()->view()->maximumScrollPosition().y()),
        frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(150, 200), pinchViewport.visibleRect().location());

    // Try it again but with the page zoomed in
    frame()->view()->notifyScrollPositionChanged(IntPoint(0, 0));
    webViewImpl()->resizePinchViewport(IntSize(500, 300));
    pinchViewport.setLocation(FloatPoint(0, 0));

    webViewImpl()->setPageScaleFactor(2);
    webViewImpl()->scrollFocusedNodeIntoRect(IntRect(0, 0, 500, 200));
    EXPECT_POINT_EQ(IntPoint(0, frame()->view()->maximumScrollPosition().y()),
        frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(125, 150), pinchViewport.visibleRect().location());

    // Once more but make sure that we don't move the pinch viewport unless necessary.
    registerMockedHttpURLLoad("pinch-viewport-input-field-long-and-wide.html");
    navigateTo(m_baseURL + "pinch-viewport-input-field-long-and-wide.html");
    webViewImpl()->setInitialFocus(false);
    pinchViewport.setLocation(FloatPoint());
    frame()->view()->notifyScrollPositionChanged(IntPoint(0, 0));
    webViewImpl()->resizePinchViewport(IntSize(500, 300));
    pinchViewport.setLocation(FloatPoint(30, 50));

    webViewImpl()->setPageScaleFactor(2);
    webViewImpl()->scrollFocusedNodeIntoRect(IntRect(0, 0, 500, 200));
    EXPECT_POINT_EQ(IntPoint(200-30-75, 600-50-65), frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(30, 50), pinchViewport.visibleRect().location());
}

// Test that resizing the WebView causes ViewportConstrained objects to relayout.
TEST_F(PinchViewportTest, TestWebViewResizeCausesViewportConstrainedLayout)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(500, 300));

    registerMockedHttpURLLoad("pinch-viewport-fixed-pos.html");
    navigateTo(m_baseURL + "pinch-viewport-fixed-pos.html");

    RenderObject* navbar = frame()->document()->getElementById("navbar")->renderer();

    EXPECT_FALSE(navbar->needsLayout());

    frame()->view()->resize(IntSize(500, 200));

    EXPECT_TRUE(navbar->needsLayout());
}

class MockWebFrameClient : public blink::WebFrameClient {
public:
    MOCK_METHOD1(showContextMenu, void(const WebContextMenuData&));
};

MATCHER_P2(ContextMenuAtLocation, x, y,
    std::string(negation ? "is" : "isn't")
    + " at expected location ["
    + PrintToString(x) + ", " + PrintToString(y) + "]")
{
    return arg.mousePosition.x == x && arg.mousePosition.y == y;
}

// Test that the context menu's location is correct in the presence of pinch
// viewport offset.
TEST_F(PinchViewportTest, TestContextMenuShownInCorrectLocation)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(200, 300));

    registerMockedHttpURLLoad("200-by-300.html");
    navigateTo(m_baseURL + "200-by-300.html");

    WebMouseEvent mouseDownEvent;
    mouseDownEvent.type = WebInputEvent::MouseDown;
    mouseDownEvent.x = 10;
    mouseDownEvent.y = 10;
    mouseDownEvent.windowX = 10;
    mouseDownEvent.windowY = 10;
    mouseDownEvent.globalX = 110;
    mouseDownEvent.globalY = 210;
    mouseDownEvent.clickCount = 1;
    mouseDownEvent.button = WebMouseEvent::ButtonRight;

    // Corresponding release event (Windows shows context menu on release).
    WebMouseEvent mouseUpEvent(mouseDownEvent);
    mouseUpEvent.type = WebInputEvent::MouseUp;

    WebFrameClient* oldClient = webViewImpl()->mainFrameImpl()->client();
    MockWebFrameClient mockWebFrameClient;
    EXPECT_CALL(mockWebFrameClient, showContextMenu(ContextMenuAtLocation(mouseDownEvent.x, mouseDownEvent.y)));

    // Do a sanity check with no scale applied.
    webViewImpl()->mainFrameImpl()->setClient(&mockWebFrameClient);
    webViewImpl()->handleInputEvent(mouseDownEvent);
    webViewImpl()->handleInputEvent(mouseUpEvent);

    Mock::VerifyAndClearExpectations(&mockWebFrameClient);
    mouseDownEvent.button = WebMouseEvent::ButtonLeft;
    webViewImpl()->handleInputEvent(mouseDownEvent);

    // Now pinch zoom into the page and move the pinch viewport. The context
    // menu should still appear at the location of the event, relative to the
    // WebView.
    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();
    webViewImpl()->setPageScaleFactor(2);
    pinchViewport.setLocation(FloatPoint(60, 80));
    EXPECT_CALL(mockWebFrameClient, showContextMenu(ContextMenuAtLocation(mouseDownEvent.x, mouseDownEvent.y)));

    mouseDownEvent.button = WebMouseEvent::ButtonRight;
    webViewImpl()->handleInputEvent(mouseDownEvent);
    webViewImpl()->handleInputEvent(mouseUpEvent);

    // Reset the old client so destruction can occur naturally.
    webViewImpl()->mainFrameImpl()->setClient(oldClient);
}

// Test that the scrollIntoView correctly scrolls the main frame
// and pinch viewports such that the given rect is centered in the viewport.
TEST_F(PinchViewportTest, DISABLED_TestScrollingDocumentRegionIntoView)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(100, 150));

    registerMockedHttpURLLoad("200-by-300-viewport.html");
    navigateTo(m_baseURL + "200-by-300-viewport.html");

    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();

    // Test that the pinch viewport is scrolled if the viewport has been
    // resized (as is the case when the ChromeOS keyboard comes up) but not
    // scaled.
    webViewImpl()->resizePinchViewport(WebSize(100, 100));
    pinchViewport.scrollIntoView(FloatRect(100, 250, 50, 50));
    EXPECT_POINT_EQ(IntPoint(75, 150), frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 50), pinchViewport.visibleRect().location());

    pinchViewport.scrollIntoView(FloatRect(25, 75, 50, 50));
    EXPECT_POINT_EQ(IntPoint(0, 0), frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 50), pinchViewport.visibleRect().location());

    // Reset the pinch viewport's size, scale the page and repeat the test
    webViewImpl()->resizePinchViewport(IntSize(100, 150));
    webViewImpl()->setPageScaleFactor(2);
    pinchViewport.setLocation(FloatPoint());

    pinchViewport.scrollIntoView(FloatRect(50, 75, 50, 75));
    EXPECT_POINT_EQ(IntPoint(50, 75), frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(), pinchViewport.visibleRect().location());

    pinchViewport.scrollIntoView(FloatRect(190, 290, 10, 10));
    EXPECT_POINT_EQ(IntPoint(100, 150), frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(50, 75), pinchViewport.visibleRect().location());
}

} // namespace
