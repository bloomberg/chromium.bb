/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "core/frame/TopControls.h"

#include "core/dom/ClientRect.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutView.h"
#include "core/page/Page.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebCache.h"
#include "public/web/WebElement.h"
#include "public/web/WebSettings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"

namespace blink {

// These tests cover top controls scrolling on main-thread.
// The animation for completing a partial show/hide is done in compositor so
// it is not covered here.
class TopControlsTest : public testing::Test {
public:
    TopControlsTest()
        : m_baseURL("http://www.test.com/")
    {
        registerMockedHttpURLLoad("large-div.html");
        registerMockedHttpURLLoad("overflow-scrolling.html");
        registerMockedHttpURLLoad("iframe-scrolling.html");
        registerMockedHttpURLLoad("iframe-scrolling-inner.html");
        registerMockedHttpURLLoad("percent-height.html");
        registerMockedHttpURLLoad("vh-height.html");
        registerMockedHttpURLLoad("vh-height-width-800.html");
        registerMockedHttpURLLoad("vh-height-width-800-extra-wide.html");
    }

    ~TopControlsTest() override
    {
        Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
        WebCache::clear();
    }

    WebViewImpl* initialize(const std::string& pageName = "large-div.html")
    {
        RuntimeEnabledFeatures::setInertTopControlsEnabled(true);

        // Load a page with large body and set viewport size to 400x400 to ensure
        // main frame is scrollable.
        m_helper.initializeAndLoad(m_baseURL + pageName, true, nullptr, nullptr, nullptr, &configureSettings);

        webViewImpl()->resize(IntSize(400, 400));
        return webViewImpl();
    }

    static void configureSettings(WebSettings* settings)
    {
        settings->setJavaScriptEnabled(true);
        settings->setAcceleratedCompositingEnabled(true);
        settings->setPreferCompositingToLCDTextEnabled(true);
        // Android settings
        settings->setViewportEnabled(true);
        settings->setViewportMetaEnabled(true);
        settings->setShrinksViewportContentToFit(true);
        settings->setMainFrameResizesAreOrientationChanges(true);
    }

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(fileName.c_str()));
    }

    WebGestureEvent generateEvent(WebInputEvent::Type type, int deltaX = 0, int deltaY = 0)
    {
        WebGestureEvent event;
        event.type = type;
        event.sourceDevice = WebGestureDeviceTouchscreen;
        event.x = 100;
        event.y = 100;
        if (type == WebInputEvent::GestureScrollUpdate) {
            event.data.scrollUpdate.deltaX = deltaX;
            event.data.scrollUpdate.deltaY = deltaY;
        }
        return event;
    }

    void verticalScroll(float deltaY)
    {
        webViewImpl()->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
        webViewImpl()->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, deltaY));
        webViewImpl()->handleInputEvent(generateEvent(WebInputEvent::GestureScrollEnd));
    }

    Element* getElementById(const WebString& id)
    {
        return static_cast<Element*>(
            webViewImpl()->mainFrame()->document().getElementById(id));
    }


    WebViewImpl* webViewImpl() const { return m_helper.webViewImpl(); }
    LocalFrame* frame() const { return m_helper.webViewImpl()->mainFrameImpl()->frame(); }
    VisualViewport& visualViewport() const { return m_helper.webViewImpl()->page()->frameHost().visualViewport(); }

private:
    std::string m_baseURL;
    FrameTestHelpers::WebViewHelper m_helper;
};

#define EXPECT_POINT_EQ(expected, actual)               \
    do {                                                \
        EXPECT_DOUBLE_EQ((expected).x(), (actual).x()); \
        EXPECT_DOUBLE_EQ((expected).y(), (actual).y()); \
    } while (false)

// Disable these tests on Mac OSX until further investigation.
// Local build on Mac is OK but the bot fails. This is not an issue as
// Top Controls are currently only used on Android.
#if OS(MACOSX)
#define MAYBE(test) DISABLED_##test
#else
#define MAYBE(test) test
#endif

// Scrolling down should hide top controls.
TEST_F(TopControlsTest, MAYBE(HideOnScrollDown))
{
    WebViewImpl* webView = initialize();
    // initialize top controls to be shown.
    webView->resizeWithTopControls(webView->size(), 50.f, true);
    webView->topControls().setShownRatio(1);

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());

    // Top controls should be scrolled partially and page should not scroll.
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, -25.f));
    EXPECT_FLOAT_EQ(25.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 0), frame()->view()->scrollPosition());

    // Top controls should consume 25px and become hidden. Excess scroll should be consumed by the page.
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, -40.f));
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 15), frame()->view()->scrollPosition());

    // Only page should consume scroll
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, -20.f));
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 35), frame()->view()->scrollPosition());
}

// Scrolling up should show top controls.
TEST_F(TopControlsTest, MAYBE(ShowOnScrollUp))
{
    WebViewImpl* webView = initialize();
    // initialize top controls to be hidden.
    webView->resizeWithTopControls(webView->size(), 50.f, false);
    webView->topControls().setShownRatio(0);

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, 10.f));
    EXPECT_FLOAT_EQ(10.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 0), frame()->view()->scrollPosition());

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, 50.f));
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 0), frame()->view()->scrollPosition());
}

// Scrolling up after previous scroll downs should cause top controls to be
// shown only after all previously scrolled down amount is compensated.
TEST_F(TopControlsTest, MAYBE(ScrollDownThenUp))
{
    WebViewImpl* webView = initialize();
    // initialize top controls to be shown and position page at 100px.
    webView->resizeWithTopControls(webView->size(), 50.f, true);
    webView->topControls().setShownRatio(1);
    frame()->view()->getScrollableArea()->setScrollPosition(IntPoint(0, 100), ProgrammaticScroll);

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());

    // Scroll down to completely hide top controls. Excess deltaY (100px) should be consumed by the page.
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, -150.f));
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 200), frame()->view()->scrollPosition());

    // Scroll up and ensure the top controls does not move until we recover 100px previously scrolled.
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, 40.f));
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 160), frame()->view()->scrollPosition());

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, 60.f));
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 100), frame()->view()->scrollPosition());

    // Now we have hit the threshold so further scroll up should be consumed by top controls.
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, 30.f));
    EXPECT_FLOAT_EQ(30.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 100), frame()->view()->scrollPosition());

    // Once top control is fully shown then page should consume any excess scroll.
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, 70.f));
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 50), frame()->view()->scrollPosition());
}

// Scrolling down should always cause visible top controls to start hiding even
// if we have been scrolling up previously.
TEST_F(TopControlsTest, MAYBE(ScrollUpThenDown))
{
    WebViewImpl* webView = initialize();
    // initialize top controls to be hidden and position page at 100px.
    webView->resizeWithTopControls(webView->size(), 50.f, false);
    webView->topControls().setShownRatio(0);
    frame()->view()->getScrollableArea()->setScrollPosition(IntPoint(0, 100), ProgrammaticScroll);

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());

    // Scroll up to completely show top controls. Excess deltaY (50px) should be consumed by the page.
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, 100.f));
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 50), frame()->view()->scrollPosition());

    // Scroll down and ensure only top controls is scrolled
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, -40.f));
    EXPECT_FLOAT_EQ(10.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 50), frame()->view()->scrollPosition());

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, -60.f));
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 100), frame()->view()->scrollPosition());
}

// Top controls should not consume horizontal scroll.
TEST_F(TopControlsTest, MAYBE(HorizontalScroll))
{
    WebViewImpl* webView = initialize();
    // initialize top controls to be shown.
    webView->resizeWithTopControls(webView->size(), 50.f, true);
    webView->topControls().setShownRatio(1);

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());

    // Top controls should not consume horizontal scroll.
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, -110.f, -100.f));
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(110, 50), frame()->view()->scrollPosition());

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, -40.f, 0));
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(150, 50), frame()->view()->scrollPosition());
}

// Page scale should not impact top controls scrolling
TEST_F(TopControlsTest, MAYBE(PageScaleHasNoImpact))
{
    WebViewImpl* webView = initialize();
    webViewImpl()->setDefaultPageScaleLimits(0.25f, 5);
    webView->setPageScaleFactor(2.0);

    // Initialize top controls to be shown.
    webView->resizeWithTopControls(webView->size(), 50.f, true);
    webView->topControls().setShownRatio(1);

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());

    // Top controls should be scrolled partially and page should not scroll.
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, -20.f));
    EXPECT_FLOAT_EQ(30.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 0), frame()->view()->getScrollableArea()->scrollPositionDouble());

    // Top controls should consume 30px and become hidden. Excess scroll should be consumed by the page at 2x scale.
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, -70.f));
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 20), frame()->view()->getScrollableArea()->scrollPositionDouble());

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollEnd));

    // Change page scale and test.
    webView->setPageScaleFactor(0.5);

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 20), frame()->view()->getScrollableArea()->scrollPositionDouble());

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, 50.f));
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 20), frame()->view()->getScrollableArea()->scrollPositionDouble());

    // At 0.5x scale scrolling 10px should take us to the top of the page.
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollUpdate, 0, 10.f));
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 0), frame()->view()->getScrollableArea()->scrollPositionDouble());
}

// Some scroll deltas result in a shownRatio that can't be realized in a
// floating-point number. Make sure that if the top controls aren't fully
// scrolled, scrollBy doesn't return any excess delta. i.e. There should be no
// slippage between the content and top controls.
TEST_F(TopControlsTest, MAYBE(FloatingPointSlippage))
{
    WebViewImpl* webView = initialize();
    webViewImpl()->setDefaultPageScaleLimits(0.25f, 5);
    webView->setPageScaleFactor(2.0);

    // Initialize top controls to be shown.
    webView->resizeWithTopControls(webView->size(), 50.f, true);
    webView->topControls().setShownRatio(1);

    webView->topControls().scrollBegin();
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());

    // This will result in a 20px scroll to the top controls so the show ratio
    // will be 30/50 == 0.6 which is not representible in a float. Make sure
    // that scroll still consumes the whole delta.
    FloatSize remainingDelta = webView->topControls().scrollBy(FloatSize(0, 10));
    EXPECT_EQ(0, remainingDelta.height());
}

// Scrollable subregions should scroll before top controls
TEST_F(TopControlsTest, MAYBE(ScrollableSubregionScrollFirst))
{
    WebViewImpl* webView = initialize("overflow-scrolling.html");
    webView->resizeWithTopControls(webView->size(), 50.f, true);
    webView->topControls().setShownRatio(1);
    frame()->view()->getScrollableArea()->setScrollPosition(IntPoint(0, 50), ProgrammaticScroll);

    // Test scroll down
    // Scroll down should scroll the overflow div first but top controls and main frame should not scroll.
    verticalScroll(-800.f);
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 50), frame()->view()->scrollPosition());

    // Continued scroll down should start hiding top controls but main frame should not scroll.
    verticalScroll(-40.f);
    EXPECT_FLOAT_EQ(10.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 50), frame()->view()->scrollPosition());

    // Continued scroll down should scroll down the main frame
    verticalScroll(-40.f);
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 80), frame()->view()->scrollPosition());

    // Test scroll up
    // scroll up should scroll overflow div first
    verticalScroll(800.f);
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 80), frame()->view()->scrollPosition());

    // Continued scroll up should start showing top controls but main frame should not scroll.
    verticalScroll(40.f);
    EXPECT_FLOAT_EQ(40.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 80), frame()->view()->scrollPosition());

    // Continued scroll down up scroll up the main frame
    verticalScroll(40.f);
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 50), frame()->view()->scrollPosition());
}


// Scrollable iframes should scroll before top controls
TEST_F(TopControlsTest, MAYBE(ScrollableIframeScrollFirst))
{
    WebViewImpl* webView = initialize("iframe-scrolling.html");
    webView->resizeWithTopControls(webView->size(), 50.f, true);
    webView->topControls().setShownRatio(1);
    frame()->view()->getScrollableArea()->setScrollPosition(IntPoint(0, 50), ProgrammaticScroll);

    // Test scroll down
    // Scroll down should scroll the iframe first but top controls and main frame should not scroll.
    verticalScroll(-800.f);
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 50), frame()->view()->scrollPosition());

    // Continued scroll down should start hiding top controls but main frame should not scroll.
    verticalScroll(-40.f);
    EXPECT_FLOAT_EQ(10.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 50), frame()->view()->scrollPosition());

    // Continued scroll down should scroll down the main frame
    verticalScroll(-40.f);
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 80), frame()->view()->scrollPosition());

    // Test scroll up
    // scroll up should scroll iframe first
    verticalScroll(800.f);
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 80), frame()->view()->scrollPosition());

    // Continued scroll up should start showing top controls but main frame should not scroll.
    verticalScroll(40.f);
    EXPECT_FLOAT_EQ(40.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 80), frame()->view()->scrollPosition());

    // Continued scroll down up scroll up the main frame
    verticalScroll(40.f);
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 50), frame()->view()->scrollPosition());
}

// Top controls visibility should remain consistent when height is changed.
TEST_F(TopControlsTest, MAYBE(HeightChangeMaintainsVisibility))
{
    WebViewImpl* webView = initialize();
    webView->resizeWithTopControls(webView->size(), 20.f, false);
    webView->topControls().setShownRatio(0);

    webView->resizeWithTopControls(webView->size(), 20.f, false);
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());

    webView->resizeWithTopControls(webView->size(), 40.f, false);
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());

    // Scroll up to show top controls.
    verticalScroll(40.f);
    EXPECT_FLOAT_EQ(40.f, webView->topControls().contentOffset());

    // Changing height of a fully shown top controls should correctly adjust content offset
    webView->resizeWithTopControls(webView->size(), 30.f, false);
    EXPECT_FLOAT_EQ(30.f, webView->topControls().contentOffset());
}

// Zero delta should not have any effect on top controls.
TEST_F(TopControlsTest, MAYBE(ZeroHeightMeansNoEffect))
{
    WebViewImpl* webView = initialize();
    webView->resizeWithTopControls(webView->size(), 0, false);
    webView->topControls().setShownRatio(0);
    frame()->view()->getScrollableArea()->setScrollPosition(IntPoint(0, 100), ProgrammaticScroll);

    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());

    verticalScroll(20.f);
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 80), frame()->view()->scrollPosition());

    verticalScroll(-30.f);
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 110), frame()->view()->scrollPosition());

    webView->topControls().setShownRatio(1);
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());
}

// Top controls should not hide when scrolling up past limit
TEST_F(TopControlsTest, MAYBE(ScrollUpPastLimitDoesNotHide))
{
    WebViewImpl* webView = initialize();
    // Initialize top controls to be shown
    webView->resizeWithTopControls(webView->size(), 50.f, true);
    webView->topControls().setShownRatio(1);
    // Use 2x scale so that both visual viewport and frameview are scrollable
    webView->setPageScaleFactor(2.0);

    // Fully scroll frameview but visualviewport remains scrollable
    webView->mainFrame()->setScrollOffset(WebSize(0, 10000));
    visualViewport().setLocation(FloatPoint(0, 0));
    verticalScroll(-10.f);
    EXPECT_FLOAT_EQ(40, webView->topControls().contentOffset());

    webView->topControls().setShownRatio(1);
    // Fully scroll visual veiwport but frameview remains scrollable
    webView->mainFrame()->setScrollOffset(WebSize(0, 0));
    visualViewport().setLocation(FloatPoint(0, 10000));
    verticalScroll(-20.f);
    EXPECT_FLOAT_EQ(30, webView->topControls().contentOffset());

    webView->topControls().setShownRatio(1);
    // Fully scroll both frameview and visual viewport
    webView->mainFrame()->setScrollOffset(WebSize(0, 10000));
    visualViewport().setLocation(FloatPoint(0, 10000));
    verticalScroll(-30.f);
    // Top controls should not move because neither frameview nor visual viewport
    // are scrollable
    EXPECT_FLOAT_EQ(50.f, webView->topControls().contentOffset());
}

// Top controls should honor its constraints
TEST_F(TopControlsTest, MAYBE(StateConstraints))
{
    WebViewImpl* webView = initialize();
    webView->resizeWithTopControls(webView->size(), 50.f, false);
    frame()->view()->getScrollableArea()->setScrollPosition(IntPoint(0, 100), ProgrammaticScroll);

    // Setting permitted state should not change content offset
    webView->updateTopControlsState(WebTopControlsShown, WebTopControlsShown, false);
    EXPECT_FLOAT_EQ(0.f, webView->topControls().contentOffset());

    // Showing is permitted
    webView->topControls().setShownRatio(1);
    EXPECT_FLOAT_EQ(50, webView->topControls().contentOffset());

    // Only shown state is permitted so controls cannot hide
    verticalScroll(-20.f);
    EXPECT_FLOAT_EQ(50, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 120), frame()->view()->scrollPosition());

    // Setting permitted state should not change content offset
    webView->updateTopControlsState(WebTopControlsHidden, WebTopControlsHidden, false);
    EXPECT_FLOAT_EQ(50, webView->topControls().contentOffset());

    // Hiding is permitted
    webView->topControls().setShownRatio(0);
    EXPECT_FLOAT_EQ(0, webView->topControls().contentOffset());

    // Only hidden state is permitted so controls cannot show
    verticalScroll(30.f);
    EXPECT_FLOAT_EQ(0, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 90), frame()->view()->scrollPosition());

    // Setting permitted state should not change content offset
    webView->updateTopControlsState(WebTopControlsBoth, WebTopControlsBoth, false);
    EXPECT_FLOAT_EQ(0, webView->topControls().contentOffset());

    // Both states are permitted so controls can either show or hide
    verticalScroll(50.f);
    EXPECT_FLOAT_EQ(50, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 90), frame()->view()->scrollPosition());

    verticalScroll(-50.f);
    EXPECT_FLOAT_EQ(0, webView->topControls().contentOffset());
    EXPECT_POINT_EQ(IntPoint(0, 90), frame()->view()->scrollPosition());
}

// Ensure that top controls do not affect the layout by showing and hiding
// except for position: fixed elements.
TEST_F(TopControlsTest, MAYBE(DontAffectLayoutHeight))
{
    // Initialize with the top controls showing.
    WebViewImpl* webView = initialize("percent-height.html");
    webView->resizeWithTopControls(WebSize(400, 300), 100.f, true);
    webView->updateTopControlsState(
        WebTopControlsBoth, WebTopControlsShown, false);
    webView->topControls().setShownRatio(1);
    webView->updateAllLifecyclePhases();

    ASSERT_EQ(100.f, webView->topControls().contentOffset());

    // When the top controls are showing, there's 300px for the layout height so
    // 50% should result in both the position:fixed and position: absolute divs
    // having 150px of height.
    Element* absPos =
        getElementById(WebString::fromUTF8("abs"));
    Element* fixedPos =
        getElementById(WebString::fromUTF8("fixed"));
    EXPECT_FLOAT_EQ(150.f, absPos->getBoundingClientRect()->height());
    EXPECT_FLOAT_EQ(150.f, fixedPos->getBoundingClientRect()->height());

    // The layout size on the FrameView should not include the top controls.
    EXPECT_EQ(300, frame()->view()->layoutSize(IncludeScrollbars).height());

    // Hide the top controls.
    verticalScroll(-100.f);
    webView->resizeWithTopControls(WebSize(400, 400), 100.f, false);
    webView->updateAllLifecyclePhases();

    ASSERT_EQ(0.f, webView->topControls().contentOffset());

    // Hiding the top controls shouldn't change the height of the initial
    // containing block for non-position: fixed. Position: fixed however should
    // use the entire height of the viewport however.
    EXPECT_FLOAT_EQ(150.f, absPos->getBoundingClientRect()->height());
    EXPECT_FLOAT_EQ(200.f, fixedPos->getBoundingClientRect()->height());

    // The layout size should not change as a result of top controls hiding.
    EXPECT_EQ(300, frame()->view()->layoutSize(IncludeScrollbars).height());
}

// Ensure that top controls do not affect vh units.
TEST_F(TopControlsTest, MAYBE(DontAffectVHUnits))
{
    // Initialize with the top controls showing.
    WebViewImpl* webView = initialize("vh-height.html");
    webView->resizeWithTopControls(WebSize(400, 300), 100.f, true);
    webView->updateTopControlsState(
        WebTopControlsBoth, WebTopControlsShown, false);
    webView->topControls().setShownRatio(1);
    webView->updateAllLifecyclePhases();

    ASSERT_EQ(100.f, webView->topControls().contentOffset());

    // 'vh' units should be based on the viewport when the top controls are
    // hidden.
    Element* absPos =
        getElementById(WebString::fromUTF8("abs"));
    Element* fixedPos =
        getElementById(WebString::fromUTF8("fixed"));
    EXPECT_FLOAT_EQ(200.f, absPos->getBoundingClientRect()->height());
    EXPECT_FLOAT_EQ(200.f, fixedPos->getBoundingClientRect()->height());

    // The size used for viewport units should not be reduced by the top
    // controls.
    EXPECT_EQ(400, frame()->view()->viewportSizeForViewportUnits().height());

    // Hide the top controls.
    verticalScroll(-100.f);
    webView->resizeWithTopControls(WebSize(400, 400), 100.f, false);
    webView->updateAllLifecyclePhases();

    ASSERT_EQ(0.f, webView->topControls().contentOffset());

    // vh units should be static with respect to the top controls so neighter
    // <div> should change size are a result of the top controls hiding.
    EXPECT_FLOAT_EQ(200.f, absPos->getBoundingClientRect()->height());
    EXPECT_FLOAT_EQ(200.f, fixedPos->getBoundingClientRect()->height());

    // The viewport size used for vh units should not change as a result of top
    // controls hiding.
    EXPECT_EQ(400, frame()->view()->viewportSizeForViewportUnits().height());
}

// Ensure that on a legacy page (there's a non-1 minimum scale) 100vh units fill
// the viewport, with top controls hidden, when the viewport encompasses the
// layout width.
TEST_F(TopControlsTest, MAYBE(DontAffectVHUnitsWithScale))
{
    // Initialize with the top controls showing.
    WebViewImpl* webView = initialize("vh-height-width-800.html");
    webView->resizeWithTopControls(WebSize(400, 300), 100.f, true);
    webView->updateTopControlsState(
        WebTopControlsBoth, WebTopControlsShown, false);
    webView->topControls().setShownRatio(1);
    webView->updateAllLifecyclePhases();

    ASSERT_EQ(100.f, webView->topControls().contentOffset());

    // Device viewport is 400px but the page is width=800 so minimum-scale
    // should be 0.5. This is also the scale at which the viewport fills the
    // layout width.
    ASSERT_EQ(0.5f, webView->minimumPageScaleFactor());

    // We should size vh units so that 100vh fills the viewport at min-scale so
    // we have to account for the minimum page scale factor. Since both boxes
    // are 50vh, and layout scale = 0.5, we have a vh viewport of 400 / 0.5 = 800
    // so we expect 50vh to be 400px.
    Element* absPos =
        getElementById(WebString::fromUTF8("abs"));
    Element* fixedPos =
        getElementById(WebString::fromUTF8("fixed"));
    EXPECT_FLOAT_EQ(400.f, absPos->getBoundingClientRect()->height());
    EXPECT_FLOAT_EQ(400.f, fixedPos->getBoundingClientRect()->height());

    // The size used for viewport units should not be reduced by the top
    // controls.
    EXPECT_EQ(800, frame()->view()->viewportSizeForViewportUnits().height());

    // Hide the top controls.
    verticalScroll(-100.f);
    webView->resizeWithTopControls(WebSize(400, 400), 100.f, false);
    webView->updateAllLifecyclePhases();

    ASSERT_EQ(0.f, webView->topControls().contentOffset());

    // vh units should be static with respect to the top controls so neighter
    // <div> should change size are a result of the top controls hiding.
    EXPECT_FLOAT_EQ(400.f, absPos->getBoundingClientRect()->height());
    EXPECT_FLOAT_EQ(400.f, fixedPos->getBoundingClientRect()->height());

    // The viewport size used for vh units should not change as a result of top
    // controls hiding.
    EXPECT_EQ(800, frame()->view()->viewportSizeForViewportUnits().height());
}

// Ensure that on a legacy page (there's a non-1 minimum scale) whose viewport
// at minimum-scale is larger than the layout size, 100vh units fill the
// viewport, with top controls hidden, when the viewport is scaled such that
// its width equals the layout width.
TEST_F(TopControlsTest, MAYBE(DontAffectVHUnitsUseLayoutSize))
{
    // Initialize with the top controls showing.
    WebViewImpl* webView = initialize("vh-height-width-800-extra-wide.html");
    webView->resizeWithTopControls(WebSize(400, 300), 100.f, true);
    webView->updateTopControlsState(
        WebTopControlsBoth, WebTopControlsShown, false);
    webView->topControls().setShownRatio(1);
    webView->updateAllLifecyclePhases();

    ASSERT_EQ(100.f, webView->topControls().contentOffset());

    // Device viewport is 400px and page is width=800 but there's an element
    // that's 1600px wide so the minimum scale is 0.25 to encompass that.
    ASSERT_EQ(0.25f, webView->minimumPageScaleFactor());

    // The viewport will match the layout width at scale=0.5 so the height used
    // for vh should be (300 / 0.5) for the layout height + (100 / 0.5) for top
    // controls = 800.
    EXPECT_EQ(800, frame()->view()->viewportSizeForViewportUnits().height());
}

// This tests that the viewport remains anchored when top controls are brought
// in while the document is fully scrolled. This normally causes clamping of the
// visual viewport to keep it bounded by the layout viewport so we're testing
// that the viewport anchoring logic is working to keep the view unchanged.
TEST_F(TopControlsTest, MAYBE(AnchorViewportDuringTopControlsAdjustment))
{
    int contentHeight = 1016;
    int layoutViewportHeight = 500;
    int visualViewportHeight = 500;
    int topControlsHeight = 100;
    int pageScale = 2;
    int minScale = 1;

    // Initialize with the top controls showing.
    WebViewImpl* webView = initialize("large-div.html");
    webViewImpl()->setDefaultPageScaleLimits(minScale, 5);
    webView->resizeWithTopControls(
        WebSize(800, layoutViewportHeight),
        topControlsHeight,
        true);
    webView->updateTopControlsState(
        WebTopControlsBoth, WebTopControlsShown, false);
    webView->topControls().setShownRatio(1);
    webView->updateAllLifecyclePhases();

    FrameView* view = frame()->view();
    ScrollableArea* rootViewport = frame()->view()->getScrollableArea();

    int expectedVisualOffset =
        ((layoutViewportHeight + topControlsHeight / minScale) * pageScale
        - (visualViewportHeight + topControlsHeight))
        / pageScale;
    int expectedLayoutOffset =
        contentHeight - (layoutViewportHeight + topControlsHeight / minScale);
    int expectedRootOffset = expectedVisualOffset + expectedLayoutOffset;

    // Zoom in to 2X and fully scroll both viewports.
    webView->setPageScaleFactor(pageScale);
    {
        webView->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollBegin));
        webView->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollUpdate, 0, -10000));

        ASSERT_EQ(0.f, webView->topControls().contentOffset());

        EXPECT_EQ(expectedVisualOffset, visualViewport().location().y());
        EXPECT_EQ(expectedLayoutOffset,
            view->layoutViewportScrollableArea()->scrollPosition().y());
        EXPECT_EQ(expectedRootOffset, rootViewport->scrollPosition().y());

        webView->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollEnd));
    }

    // Commit the top controls resize so that the top controls do not shrink the
    // layout size. This should not have moved any of the viewports.
    webView->resizeWithTopControls(
        WebSize(800, layoutViewportHeight + topControlsHeight),
        topControlsHeight,
        false);
    webView->updateAllLifecyclePhases();
    ASSERT_EQ(expectedVisualOffset, visualViewport().location().y());
    ASSERT_EQ(expectedLayoutOffset,
        view->layoutViewportScrollableArea()->scrollPosition().y());
    ASSERT_EQ(expectedRootOffset, rootViewport->scrollPosition().y());

    // Now scroll back up just enough to show the top controls. The top controls
    // should shrink both viewports but the layout viewport by a greater amount.
    // This means the visual viewport's offset must be clamped to keep it within
    // the layout viewport. Make sure we adjust the scroll position to account
    // for this and keep the visual viewport at the same location relative to
    // the document (i.e. the user shouldn't see a movement).
    {
        webView->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollBegin));
        webView->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollUpdate, 0, 80));

        visualViewport().clampToBoundaries();
        view->setScrollPosition(view->scrollPosition(), ProgrammaticScroll);

        ASSERT_EQ(80.f, webView->topControls().contentOffset());
        EXPECT_EQ(expectedRootOffset, rootViewport->scrollPosition().y());

        webView->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollEnd));
    }
}

} // namespace blink
