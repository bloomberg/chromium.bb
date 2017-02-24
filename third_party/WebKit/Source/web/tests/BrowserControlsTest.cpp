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
#include "core/frame/BrowserControls.h"

#include "core/dom/ClientRect.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/VisualViewport.h"
#include "core/page/Page.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebElement.h"
#include "public/web/WebSettings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"

namespace blink {

// These tests cover browser controls scrolling on main-thread.
// The animation for completing a partial show/hide is done in compositor so
// it is not covered here.
class BrowserControlsTest : public ::testing::Test {
 public:
  BrowserControlsTest() : m_baseURL("http://www.test.com/") {
    registerMockedHttpURLLoad("large-div.html");
    registerMockedHttpURLLoad("overflow-scrolling.html");
    registerMockedHttpURLLoad("iframe-scrolling.html");
    registerMockedHttpURLLoad("iframe-scrolling-inner.html");
    registerMockedHttpURLLoad("percent-height.html");
    registerMockedHttpURLLoad("vh-height.html");
    registerMockedHttpURLLoad("vh-height-width-800.html");
    registerMockedHttpURLLoad("vh-height-width-800-extra-wide.html");
  }

  ~BrowserControlsTest() override {
    Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
  }

  WebViewImpl* initialize(const std::string& pageName = "large-div.html") {
    RuntimeEnabledFeatures::setInertTopControlsEnabled(true);

    // Load a page with large body and set viewport size to 400x400 to ensure
    // main frame is scrollable.
    m_helper.initializeAndLoad(m_baseURL + pageName, true, nullptr, nullptr,
                               nullptr, &configureSettings);

    webViewImpl()->resize(IntSize(400, 400));
    return webViewImpl();
  }

  static void configureSettings(WebSettings* settings) {
    settings->setJavaScriptEnabled(true);
    settings->setAcceleratedCompositingEnabled(true);
    settings->setPreferCompositingToLCDTextEnabled(true);
    // Android settings
    settings->setViewportEnabled(true);
    settings->setViewportMetaEnabled(true);
    settings->setShrinksViewportContentToFit(true);
    settings->setMainFrameResizesAreOrientationChanges(true);
  }

  void registerMockedHttpURLLoad(const std::string& fileName) {
    URLTestHelpers::registerMockedURLLoadFromBase(
        WebString::fromUTF8(m_baseURL), testing::webTestDataPath(),
        WebString::fromUTF8(fileName));
  }

  WebCoalescedInputEvent generateEvent(WebInputEvent::Type type,
                                       int deltaX = 0,
                                       int deltaY = 0) {
    WebGestureEvent event(type, WebInputEvent::NoModifiers,
                          WebInputEvent::TimeStampForTesting);
    event.sourceDevice = WebGestureDeviceTouchscreen;
    event.x = 100;
    event.y = 100;
    if (type == WebInputEvent::GestureScrollUpdate) {
      event.data.scrollUpdate.deltaX = deltaX;
      event.data.scrollUpdate.deltaY = deltaY;
    }
    return WebCoalescedInputEvent(event);
  }

  void verticalScroll(float deltaY) {
    webViewImpl()->handleInputEvent(
        generateEvent(WebInputEvent::GestureScrollBegin));
    webViewImpl()->handleInputEvent(
        generateEvent(WebInputEvent::GestureScrollUpdate, 0, deltaY));
    webViewImpl()->handleInputEvent(
        generateEvent(WebInputEvent::GestureScrollEnd));
  }

  Element* getElementById(const WebString& id) {
    return static_cast<Element*>(
        webViewImpl()->mainFrame()->document().getElementById(id));
  }

  WebViewImpl* webViewImpl() const { return m_helper.webView(); }
  LocalFrame* frame() const {
    return m_helper.webView()->mainFrameImpl()->frame();
  }
  VisualViewport& visualViewport() const {
    return m_helper.webView()->page()->frameHost().visualViewport();
  }

 private:
  std::string m_baseURL;
  FrameTestHelpers::WebViewHelper m_helper;
};

#define EXPECT_SIZE_EQ(expected, actual)                     \
  do {                                                       \
    EXPECT_FLOAT_EQ((expected).width(), (actual).width());   \
    EXPECT_FLOAT_EQ((expected).height(), (actual).height()); \
  } while (false)

// Disable these tests on Mac OSX until further investigation.
// Local build on Mac is OK but the bot fails. This is not an issue as
// Browser Controls are currently only used on Android.
#if OS(MACOSX)
#define MAYBE(test) DISABLED_##test
#else
#define MAYBE(test) test
#endif

// Scrolling down should hide browser controls.
TEST_F(BrowserControlsTest, MAYBE(HideOnScrollDown)) {
  WebViewImpl* webView = initialize();
  // initialize browser controls to be shown.
  webView->resizeWithBrowserControls(webView->size(), 50.f, true);
  webView->browserControls().setShownRatio(1);

  webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());

  // Browser controls should be scrolled partially and page should not scroll.
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, -25.f));
  EXPECT_FLOAT_EQ(25.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 0),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Browser controls should consume 25px and become hidden. Excess scroll
  // should be
  // consumed by the page.
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, -40.f));
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 15),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Only page should consume scroll
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, -20.f));
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 35),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());
}

// Scrolling up should show browser controls.
TEST_F(BrowserControlsTest, MAYBE(ShowOnScrollUp)) {
  WebViewImpl* webView = initialize();
  // initialize browser controls to be hidden.
  webView->resizeWithBrowserControls(webView->size(), 50.f, false);
  webView->browserControls().setShownRatio(0);

  webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());

  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, 10.f));
  EXPECT_FLOAT_EQ(10.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 0),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, 50.f));
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 0),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());
}

// Scrolling up after previous scroll downs should cause browser controls to be
// shown only after all previously scrolled down amount is compensated.
TEST_F(BrowserControlsTest, MAYBE(ScrollDownThenUp)) {
  WebViewImpl* webView = initialize();
  // initialize browser controls to be shown and position page at 100px.
  webView->resizeWithBrowserControls(webView->size(), 50.f, true);
  webView->browserControls().setShownRatio(1);
  frame()->view()->getScrollableArea()->setScrollOffset(ScrollOffset(0, 100),
                                                        ProgrammaticScroll);

  webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());

  // Scroll down to completely hide browser controls. Excess deltaY (100px)
  // should be consumed by the page.
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, -150.f));
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 200),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Scroll up and ensure the browser controls does not move until we recover
  // 100px previously scrolled.
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, 40.f));
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 160),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, 60.f));
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 100),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Now we have hit the threshold so further scroll up should be consumed by
  // browser controls.
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, 30.f));
  EXPECT_FLOAT_EQ(30.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 100),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Once top control is fully shown then page should consume any excess scroll.
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, 70.f));
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 50),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());
}

// Scrolling down should always cause visible browser controls to start hiding
// even if we have been scrolling up previously.
TEST_F(BrowserControlsTest, MAYBE(ScrollUpThenDown)) {
  WebViewImpl* webView = initialize();
  // initialize browser controls to be hidden and position page at 100px.
  webView->resizeWithBrowserControls(webView->size(), 50.f, false);
  webView->browserControls().setShownRatio(0);
  frame()->view()->getScrollableArea()->setScrollOffset(ScrollOffset(0, 100),
                                                        ProgrammaticScroll);

  webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());

  // Scroll up to completely show browser controls. Excess deltaY (50px) should
  // be consumed by the page.
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, 100.f));
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 50),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Scroll down and ensure only browser controls is scrolled
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, -40.f));
  EXPECT_FLOAT_EQ(10.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 50),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, -60.f));
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 100),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());
}

// Browser controls should not consume horizontal scroll.
TEST_F(BrowserControlsTest, MAYBE(HorizontalScroll)) {
  WebViewImpl* webView = initialize();
  // initialize browser controls to be shown.
  webView->resizeWithBrowserControls(webView->size(), 50.f, true);
  webView->browserControls().setShownRatio(1);

  webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());

  // Browser controls should not consume horizontal scroll.
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, -110.f, -100.f));
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(110, 50),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, -40.f, 0));
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(150, 50),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());
}

// Page scale should not impact browser controls scrolling
TEST_F(BrowserControlsTest, MAYBE(PageScaleHasNoImpact)) {
  WebViewImpl* webView = initialize();
  webViewImpl()->setDefaultPageScaleLimits(0.25f, 5);
  webView->setPageScaleFactor(2.0);

  // Initialize browser controls to be shown.
  webView->resizeWithBrowserControls(webView->size(), 50.f, true);
  webView->browserControls().setShownRatio(1);

  webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());

  // Browser controls should be scrolled partially and page should not scroll.
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, -20.f));
  EXPECT_FLOAT_EQ(30.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 0),
                 frame()->view()->getScrollableArea()->getScrollOffset());

  // Browser controls should consume 30px and become hidden. Excess scroll
  // should be consumed by the page at 2x scale.
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, -70.f));
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 20),
                 frame()->view()->getScrollableArea()->getScrollOffset());

  webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollEnd));

  // Change page scale and test.
  webView->setPageScaleFactor(0.5);

  webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 20),
                 frame()->view()->getScrollableArea()->getScrollOffset());

  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, 50.f));
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 20),
                 frame()->view()->getScrollableArea()->getScrollOffset());

  // At 0.5x scale scrolling 10px should take us to the top of the page.
  webView->handleInputEvent(
      generateEvent(WebInputEvent::GestureScrollUpdate, 0, 10.f));
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 0),
                 frame()->view()->getScrollableArea()->getScrollOffset());
}

// Some scroll deltas result in a shownRatio that can't be realized in a
// floating-point number. Make sure that if the browser controls aren't fully
// scrolled, scrollBy doesn't return any excess delta. i.e. There should be no
// slippage between the content and browser controls.
TEST_F(BrowserControlsTest, MAYBE(FloatingPointSlippage)) {
  WebViewImpl* webView = initialize();
  webViewImpl()->setDefaultPageScaleLimits(0.25f, 5);
  webView->setPageScaleFactor(2.0);

  // Initialize browser controls to be shown.
  webView->resizeWithBrowserControls(webView->size(), 50.f, true);
  webView->browserControls().setShownRatio(1);

  webView->browserControls().scrollBegin();
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());

  // This will result in a 20px scroll to the browser controls so the show ratio
  // will be 30/50 == 0.6 which is not representible in a float. Make sure
  // that scroll still consumes the whole delta.
  FloatSize remainingDelta =
      webView->browserControls().scrollBy(FloatSize(0, 10));
  EXPECT_EQ(0, remainingDelta.height());
}

// Scrollable subregions should scroll before browser controls
TEST_F(BrowserControlsTest, MAYBE(ScrollableSubregionScrollFirst)) {
  WebViewImpl* webView = initialize("overflow-scrolling.html");
  webView->resizeWithBrowserControls(webView->size(), 50.f, true);
  webView->browserControls().setShownRatio(1);
  frame()->view()->getScrollableArea()->setScrollOffset(ScrollOffset(0, 50),
                                                        ProgrammaticScroll);

  // Test scroll down
  // Scroll down should scroll the overflow div first but browser controls and
  // main frame should not scroll.
  verticalScroll(-800.f);
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 50),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Continued scroll down should start hiding browser controls but main frame
  // should not scroll.
  verticalScroll(-40.f);
  EXPECT_FLOAT_EQ(10.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 50),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Continued scroll down should scroll down the main frame
  verticalScroll(-40.f);
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 80),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Test scroll up
  // scroll up should scroll overflow div first
  verticalScroll(800.f);
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 80),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Continued scroll up should start showing browser controls but main frame
  // should not scroll.
  verticalScroll(40.f);
  EXPECT_FLOAT_EQ(40.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 80),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Continued scroll down up scroll up the main frame
  verticalScroll(40.f);
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 50),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());
}

// Scrollable iframes should scroll before browser controls
TEST_F(BrowserControlsTest, MAYBE(ScrollableIframeScrollFirst)) {
  WebViewImpl* webView = initialize("iframe-scrolling.html");
  webView->resizeWithBrowserControls(webView->size(), 50.f, true);
  webView->browserControls().setShownRatio(1);
  frame()->view()->getScrollableArea()->setScrollOffset(ScrollOffset(0, 50),
                                                        ProgrammaticScroll);

  // Test scroll down
  // Scroll down should scroll the iframe first but browser controls and main
  // frame should not scroll.
  verticalScroll(-800.f);
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 50),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Continued scroll down should start hiding browser controls but main frame
  // should not scroll.
  verticalScroll(-40.f);
  EXPECT_FLOAT_EQ(10.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 50),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Continued scroll down should scroll down the main frame
  verticalScroll(-40.f);
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 80),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Test scroll up
  // scroll up should scroll iframe first
  verticalScroll(800.f);
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 80),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Continued scroll up should start showing browser controls but main frame
  // should not scroll.
  verticalScroll(40.f);
  EXPECT_FLOAT_EQ(40.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 80),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Continued scroll down up scroll up the main frame
  verticalScroll(40.f);
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 50),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());
}

// Browser controls visibility should remain consistent when height is changed.
TEST_F(BrowserControlsTest, MAYBE(HeightChangeMaintainsVisibility)) {
  WebViewImpl* webView = initialize();
  webView->resizeWithBrowserControls(webView->size(), 20.f, false);
  webView->browserControls().setShownRatio(0);

  webView->resizeWithBrowserControls(webView->size(), 20.f, false);
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());

  webView->resizeWithBrowserControls(webView->size(), 40.f, false);
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());

  // Scroll up to show browser controls.
  verticalScroll(40.f);
  EXPECT_FLOAT_EQ(40.f, webView->browserControls().contentOffset());

  // Changing height of a fully shown browser controls should correctly adjust
  // content offset
  webView->resizeWithBrowserControls(webView->size(), 30.f, false);
  EXPECT_FLOAT_EQ(30.f, webView->browserControls().contentOffset());
}

// Zero delta should not have any effect on browser controls.
TEST_F(BrowserControlsTest, MAYBE(ZeroHeightMeansNoEffect)) {
  WebViewImpl* webView = initialize();
  webView->resizeWithBrowserControls(webView->size(), 0, false);
  webView->browserControls().setShownRatio(0);
  frame()->view()->getScrollableArea()->setScrollOffset(ScrollOffset(0, 100),
                                                        ProgrammaticScroll);

  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());

  verticalScroll(20.f);
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 80),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  verticalScroll(-30.f);
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 110),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  webView->browserControls().setShownRatio(1);
  EXPECT_FLOAT_EQ(0.f, webView->browserControls().contentOffset());
}

// Browser controls should not hide when scrolling up past limit
TEST_F(BrowserControlsTest, MAYBE(ScrollUpPastLimitDoesNotHide)) {
  WebViewImpl* webView = initialize();
  // Initialize browser controls to be shown
  webView->resizeWithBrowserControls(webView->size(), 50.f, true);
  webView->browserControls().setShownRatio(1);
  // Use 2x scale so that both visual viewport and frameview are scrollable
  webView->setPageScaleFactor(2.0);

  // Fully scroll frameview but visualviewport remains scrollable
  webView->mainFrame()->setScrollOffset(WebSize(0, 10000));
  visualViewport().setLocation(FloatPoint(0, 0));
  verticalScroll(-10.f);
  EXPECT_FLOAT_EQ(40, webView->browserControls().contentOffset());

  webView->browserControls().setShownRatio(1);
  // Fully scroll visual veiwport but frameview remains scrollable
  webView->mainFrame()->setScrollOffset(WebSize(0, 0));
  visualViewport().setLocation(FloatPoint(0, 10000));
  verticalScroll(-20.f);
  EXPECT_FLOAT_EQ(30, webView->browserControls().contentOffset());

  webView->browserControls().setShownRatio(1);
  // Fully scroll both frameview and visual viewport
  webView->mainFrame()->setScrollOffset(WebSize(0, 10000));
  visualViewport().setLocation(FloatPoint(0, 10000));
  verticalScroll(-30.f);
  // Browser controls should not move because neither frameview nor visual
  // viewport
  // are scrollable
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());
}

// Browser controls should honor its constraints
TEST_F(BrowserControlsTest, MAYBE(StateConstraints)) {
  WebViewImpl* webView = initialize();
  webView->resizeWithBrowserControls(webView->size(), 50.f, false);
  frame()->view()->getScrollableArea()->setScrollOffset(ScrollOffset(0, 100),
                                                        ProgrammaticScroll);

  // Setting permitted state should change the content offset to match the
  // constraint.
  webView->updateBrowserControlsState(WebBrowserControlsShown,
                                      WebBrowserControlsShown, false);
  EXPECT_FLOAT_EQ(50.f, webView->browserControls().contentOffset());

  // Only shown state is permitted so controls cannot hide
  verticalScroll(-20.f);
  EXPECT_FLOAT_EQ(50, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 120),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Setting permitted state should change content offset to match the
  // constraint.
  webView->updateBrowserControlsState(WebBrowserControlsHidden,
                                      WebBrowserControlsHidden, false);
  EXPECT_FLOAT_EQ(0, webView->browserControls().contentOffset());

  // Only hidden state is permitted so controls cannot show
  verticalScroll(30.f);
  EXPECT_FLOAT_EQ(0, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 90),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Setting permitted state to "both" should not change content offset.
  webView->updateBrowserControlsState(WebBrowserControlsBoth,
                                      WebBrowserControlsBoth, false);
  EXPECT_FLOAT_EQ(0, webView->browserControls().contentOffset());

  // Both states are permitted so controls can either show or hide
  verticalScroll(50.f);
  EXPECT_FLOAT_EQ(50, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 90),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  verticalScroll(-50.f);
  EXPECT_FLOAT_EQ(0, webView->browserControls().contentOffset());
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 90),
      frame()->view()->layoutViewportScrollableArea()->getScrollOffset());

  // Setting permitted state to "both" should not change an in-flight offset.
  verticalScroll(20.f);
  EXPECT_FLOAT_EQ(20, webView->browserControls().contentOffset());
  webView->updateBrowserControlsState(WebBrowserControlsBoth,
                                      WebBrowserControlsBoth, false);
  EXPECT_FLOAT_EQ(20, webView->browserControls().contentOffset());

  // An animated state change shouldn't cause a change to the content offset
  // since it'll be driven from the compositor.
  webView->updateBrowserControlsState(WebBrowserControlsHidden,
                                      WebBrowserControlsHidden, true);
  EXPECT_FLOAT_EQ(20, webView->browserControls().contentOffset());

  webView->updateBrowserControlsState(WebBrowserControlsShown,
                                      WebBrowserControlsShown, true);
  EXPECT_FLOAT_EQ(20, webView->browserControls().contentOffset());

  // Setting just the constraint should affect the content offset.
  webView->updateBrowserControlsState(WebBrowserControlsHidden,
                                      WebBrowserControlsBoth, false);
  EXPECT_FLOAT_EQ(0, webView->browserControls().contentOffset());

  webView->updateBrowserControlsState(WebBrowserControlsShown,
                                      WebBrowserControlsBoth, false);
  EXPECT_FLOAT_EQ(50, webView->browserControls().contentOffset());
}

// Ensure that browser controls do not affect the layout by showing and hiding
// except for position: fixed elements.
TEST_F(BrowserControlsTest, MAYBE(DontAffectLayoutHeight)) {
  // Initialize with the browser controls showing.
  WebViewImpl* webView = initialize("percent-height.html");
  webView->resizeWithBrowserControls(WebSize(400, 300), 100.f, true);
  webView->updateBrowserControlsState(WebBrowserControlsBoth,
                                      WebBrowserControlsShown, false);
  webView->browserControls().setShownRatio(1);
  webView->updateAllLifecyclePhases();

  ASSERT_EQ(100.f, webView->browserControls().contentOffset());

  // When the browser controls are showing, there's 300px for the layout height
  // so
  // 50% should result in both the position:fixed and position: absolute divs
  // having 150px of height.
  Element* absPos = getElementById(WebString::fromUTF8("abs"));
  Element* fixedPos = getElementById(WebString::fromUTF8("fixed"));
  EXPECT_FLOAT_EQ(150.f, absPos->getBoundingClientRect()->height());
  EXPECT_FLOAT_EQ(150.f, fixedPos->getBoundingClientRect()->height());

  // The layout size on the FrameView should not include the browser controls.
  EXPECT_EQ(300, frame()->view()->layoutSize(IncludeScrollbars).height());

  // Hide the browser controls.
  verticalScroll(-100.f);
  webView->resizeWithBrowserControls(WebSize(400, 400), 100.f, false);
  webView->updateAllLifecyclePhases();

  ASSERT_EQ(0.f, webView->browserControls().contentOffset());

  // Hiding the browser controls shouldn't change the height of the initial
  // containing block for non-position: fixed. Position: fixed however should
  // use the entire height of the viewport however.
  EXPECT_FLOAT_EQ(150.f, absPos->getBoundingClientRect()->height());
  EXPECT_FLOAT_EQ(200.f, fixedPos->getBoundingClientRect()->height());

  // The layout size should not change as a result of browser controls hiding.
  EXPECT_EQ(300, frame()->view()->layoutSize(IncludeScrollbars).height());
}

// Ensure that browser controls do not affect the layout by showing and hiding
// except for position: fixed elements.
TEST_F(BrowserControlsTest, MAYBE(AffectLayoutHeightWhenConstrained)) {
  // Initialize with the browser controls showing.
  WebViewImpl* webView = initialize("percent-height.html");
  webView->resizeWithBrowserControls(WebSize(400, 300), 100.f, true);
  webView->updateBrowserControlsState(WebBrowserControlsBoth,
                                      WebBrowserControlsShown, false);
  webView->browserControls().setShownRatio(1);
  webView->updateAllLifecyclePhases();

  Element* absPos = getElementById(WebString::fromUTF8("abs"));
  Element* fixedPos = getElementById(WebString::fromUTF8("fixed"));

  ASSERT_EQ(100.f, webView->browserControls().contentOffset());

  // Hide the browser controls.
  verticalScroll(-100.f);
  webView->resizeWithBrowserControls(WebSize(400, 400), 100.f, false);
  webView->updateAllLifecyclePhases();
  ASSERT_EQ(300, frame()->view()->layoutSize(IncludeScrollbars).height());

  // Now lock the controls in a hidden state. The layout and elements should
  // resize without a WebView::resize.
  webView->updateBrowserControlsState(WebBrowserControlsHidden,
                                      WebBrowserControlsBoth, false);

  EXPECT_FLOAT_EQ(200.f, absPos->getBoundingClientRect()->height());
  EXPECT_FLOAT_EQ(200.f, fixedPos->getBoundingClientRect()->height());

  EXPECT_EQ(400, frame()->view()->layoutSize(IncludeScrollbars).height());

  // Unlock the controls, the sizes should change even though the controls are
  // still hidden.
  webView->updateBrowserControlsState(WebBrowserControlsBoth,
                                      WebBrowserControlsBoth, false);

  EXPECT_FLOAT_EQ(150.f, absPos->getBoundingClientRect()->height());
  EXPECT_FLOAT_EQ(200.f, fixedPos->getBoundingClientRect()->height());

  EXPECT_EQ(300, frame()->view()->layoutSize(IncludeScrollbars).height());

  // Now lock the controls in a shown state.
  webView->updateBrowserControlsState(WebBrowserControlsShown,
                                      WebBrowserControlsBoth, false);
  webView->resizeWithBrowserControls(WebSize(400, 300), 100.f, true);

  EXPECT_FLOAT_EQ(150.f, absPos->getBoundingClientRect()->height());
  EXPECT_FLOAT_EQ(150.f, fixedPos->getBoundingClientRect()->height());

  EXPECT_EQ(300, frame()->view()->layoutSize(IncludeScrollbars).height());

  // Shown -> Hidden
  webView->resizeWithBrowserControls(WebSize(400, 400), 100.f, false);
  webView->updateBrowserControlsState(WebBrowserControlsHidden,
                                      WebBrowserControlsBoth, false);

  EXPECT_FLOAT_EQ(200.f, absPos->getBoundingClientRect()->height());
  EXPECT_FLOAT_EQ(200.f, fixedPos->getBoundingClientRect()->height());

  EXPECT_EQ(400, frame()->view()->layoutSize(IncludeScrollbars).height());

  // Go from Unlocked and showing, to locked and hidden but issue the resize
  // before the constraint update to check for race issues.
  webView->updateBrowserControlsState(WebBrowserControlsBoth,
                                      WebBrowserControlsShown, false);
  webView->resizeWithBrowserControls(WebSize(400, 300), 100.f, true);
  ASSERT_EQ(300, frame()->view()->layoutSize(IncludeScrollbars).height());
  webView->updateAllLifecyclePhases();

  webView->resizeWithBrowserControls(WebSize(400, 400), 100.f, false);
  webView->updateBrowserControlsState(WebBrowserControlsHidden,
                                      WebBrowserControlsHidden, false);

  EXPECT_FLOAT_EQ(200.f, absPos->getBoundingClientRect()->height());
  EXPECT_FLOAT_EQ(200.f, fixedPos->getBoundingClientRect()->height());

  EXPECT_EQ(400, frame()->view()->layoutSize(IncludeScrollbars).height());
}

// Ensure that browser controls do not affect vh units.
TEST_F(BrowserControlsTest, MAYBE(DontAffectVHUnits)) {
  // Initialize with the browser controls showing.
  WebViewImpl* webView = initialize("vh-height.html");
  webView->resizeWithBrowserControls(WebSize(400, 300), 100.f, true);
  webView->updateBrowserControlsState(WebBrowserControlsBoth,
                                      WebBrowserControlsShown, false);
  webView->browserControls().setShownRatio(1);
  webView->updateAllLifecyclePhases();

  ASSERT_EQ(100.f, webView->browserControls().contentOffset());

  // 'vh' units should be based on the viewport when the browser controls are
  // hidden.
  Element* absPos = getElementById(WebString::fromUTF8("abs"));
  Element* fixedPos = getElementById(WebString::fromUTF8("fixed"));
  EXPECT_FLOAT_EQ(200.f, absPos->getBoundingClientRect()->height());
  EXPECT_FLOAT_EQ(200.f, fixedPos->getBoundingClientRect()->height());

  // The size used for viewport units should not be reduced by the top
  // controls.
  EXPECT_EQ(400, frame()->view()->viewportSizeForViewportUnits().height());

  // Hide the browser controls.
  verticalScroll(-100.f);
  webView->resizeWithBrowserControls(WebSize(400, 400), 100.f, false);
  webView->updateAllLifecyclePhases();

  ASSERT_EQ(0.f, webView->browserControls().contentOffset());

  // vh units should be static with respect to the browser controls so neighter
  // <div> should change size are a result of the browser controls hiding.
  EXPECT_FLOAT_EQ(200.f, absPos->getBoundingClientRect()->height());
  EXPECT_FLOAT_EQ(200.f, fixedPos->getBoundingClientRect()->height());

  // The viewport size used for vh units should not change as a result of top
  // controls hiding.
  EXPECT_EQ(400, frame()->view()->viewportSizeForViewportUnits().height());
}

// Ensure that on a legacy page (there's a non-1 minimum scale) 100vh units fill
// the viewport, with browser controls hidden, when the viewport encompasses the
// layout width.
TEST_F(BrowserControlsTest, MAYBE(DontAffectVHUnitsWithScale)) {
  // Initialize with the browser controls showing.
  WebViewImpl* webView = initialize("vh-height-width-800.html");
  webView->resizeWithBrowserControls(WebSize(400, 300), 100.f, true);
  webView->updateBrowserControlsState(WebBrowserControlsBoth,
                                      WebBrowserControlsShown, false);
  webView->browserControls().setShownRatio(1);
  webView->updateAllLifecyclePhases();

  ASSERT_EQ(100.f, webView->browserControls().contentOffset());

  // Device viewport is 400px but the page is width=800 so minimum-scale
  // should be 0.5. This is also the scale at which the viewport fills the
  // layout width.
  ASSERT_EQ(0.5f, webView->minimumPageScaleFactor());

  // We should size vh units so that 100vh fills the viewport at min-scale so
  // we have to account for the minimum page scale factor. Since both boxes
  // are 50vh, and layout scale = 0.5, we have a vh viewport of 400 / 0.5 = 800
  // so we expect 50vh to be 400px.
  Element* absPos = getElementById(WebString::fromUTF8("abs"));
  Element* fixedPos = getElementById(WebString::fromUTF8("fixed"));
  EXPECT_FLOAT_EQ(400.f, absPos->getBoundingClientRect()->height());
  EXPECT_FLOAT_EQ(400.f, fixedPos->getBoundingClientRect()->height());

  // The size used for viewport units should not be reduced by the top
  // controls.
  EXPECT_EQ(800, frame()->view()->viewportSizeForViewportUnits().height());

  // Hide the browser controls.
  verticalScroll(-100.f);
  webView->resizeWithBrowserControls(WebSize(400, 400), 100.f, false);
  webView->updateAllLifecyclePhases();

  ASSERT_EQ(0.f, webView->browserControls().contentOffset());

  // vh units should be static with respect to the browser controls so neighter
  // <div> should change size are a result of the browser controls hiding.
  EXPECT_FLOAT_EQ(400.f, absPos->getBoundingClientRect()->height());
  EXPECT_FLOAT_EQ(400.f, fixedPos->getBoundingClientRect()->height());

  // The viewport size used for vh units should not change as a result of top
  // controls hiding.
  EXPECT_EQ(800, frame()->view()->viewportSizeForViewportUnits().height());
}

// Ensure that on a legacy page (there's a non-1 minimum scale) whose viewport
// at minimum-scale is larger than the layout size, 100vh units fill the
// viewport, with browser controls hidden, when the viewport is scaled such that
// its width equals the layout width.
TEST_F(BrowserControlsTest, MAYBE(DontAffectVHUnitsUseLayoutSize)) {
  // Initialize with the browser controls showing.
  WebViewImpl* webView = initialize("vh-height-width-800-extra-wide.html");
  webView->resizeWithBrowserControls(WebSize(400, 300), 100.f, true);
  webView->updateBrowserControlsState(WebBrowserControlsBoth,
                                      WebBrowserControlsShown, false);
  webView->browserControls().setShownRatio(1);
  webView->updateAllLifecyclePhases();

  ASSERT_EQ(100.f, webView->browserControls().contentOffset());

  // Device viewport is 400px and page is width=800 but there's an element
  // that's 1600px wide so the minimum scale is 0.25 to encompass that.
  ASSERT_EQ(0.25f, webView->minimumPageScaleFactor());

  // The viewport will match the layout width at scale=0.5 so the height used
  // for vh should be (300 / 0.5) for the layout height + (100 / 0.5) for top
  // controls = 800.
  EXPECT_EQ(800, frame()->view()->viewportSizeForViewportUnits().height());
}

// This tests that the viewport remains anchored when browser controls are
// brought in while the document is fully scrolled. This normally causes
// clamping of the visual viewport to keep it bounded by the layout viewport
// so we're testing that the viewport anchoring logic is working to keep the
// view unchanged.
TEST_F(BrowserControlsTest,
       MAYBE(AnchorViewportDuringbrowserControlsAdjustment)) {
  int contentHeight = 1016;
  int layoutViewportHeight = 500;
  int visualViewportHeight = 500;
  int browserControlsHeight = 100;
  int pageScale = 2;
  int minScale = 1;

  // Initialize with the browser controls showing.
  WebViewImpl* webView = initialize("large-div.html");
  webViewImpl()->setDefaultPageScaleLimits(minScale, 5);
  webView->resizeWithBrowserControls(WebSize(800, layoutViewportHeight),
                                     browserControlsHeight, true);
  webView->updateBrowserControlsState(WebBrowserControlsBoth,
                                      WebBrowserControlsShown, false);
  webView->browserControls().setShownRatio(1);
  webView->updateAllLifecyclePhases();

  FrameView* view = frame()->view();
  ScrollableArea* rootViewport = frame()->view()->getScrollableArea();

  int expectedVisualOffset =
      ((layoutViewportHeight + browserControlsHeight / minScale) * pageScale -
       (visualViewportHeight + browserControlsHeight)) /
      pageScale;
  int expectedLayoutOffset =
      contentHeight - (layoutViewportHeight + browserControlsHeight / minScale);
  int expectedRootOffset = expectedVisualOffset + expectedLayoutOffset;

  // Zoom in to 2X and fully scroll both viewports.
  webView->setPageScaleFactor(pageScale);
  {
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
    webView->handleInputEvent(
        generateEvent(WebInputEvent::GestureScrollUpdate, 0, -10000));

    ASSERT_EQ(0.f, webView->browserControls().contentOffset());

    EXPECT_EQ(expectedVisualOffset,
              visualViewport().getScrollOffset().height());
    EXPECT_EQ(expectedLayoutOffset,
              view->layoutViewportScrollableArea()->getScrollOffset().height());
    EXPECT_EQ(expectedRootOffset, rootViewport->getScrollOffset().height());

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollEnd));
  }

  // Commit the browser controls resize so that the browser controls do not
  // shrink the layout size. This should not have moved any of the viewports.
  webView->resizeWithBrowserControls(
      WebSize(800, layoutViewportHeight + browserControlsHeight),
      browserControlsHeight, false);
  webView->updateAllLifecyclePhases();
  ASSERT_EQ(expectedVisualOffset, visualViewport().getScrollOffset().height());
  ASSERT_EQ(expectedLayoutOffset,
            view->layoutViewportScrollableArea()->getScrollOffset().height());
  ASSERT_EQ(expectedRootOffset, rootViewport->getScrollOffset().height());

  // Now scroll back up just enough to show the browser controls. The browser
  // controls should shrink both viewports but the layout viewport by a greater
  // amount. This means the visual viewport's offset must be clamped to keep it
  // within the layout viewport. Make sure we adjust the scroll position to
  // account for this and keep the visual viewport at the same location relative
  // to the document (i.e. the user shouldn't see a movement).
  {
    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollBegin));
    webView->handleInputEvent(
        generateEvent(WebInputEvent::GestureScrollUpdate, 0, 80));

    visualViewport().clampToBoundaries();
    view->setScrollOffset(view->getScrollOffset(), ProgrammaticScroll);

    ASSERT_EQ(80.f, webView->browserControls().contentOffset());
    EXPECT_EQ(expectedRootOffset, rootViewport->getScrollOffset().height());

    webView->handleInputEvent(generateEvent(WebInputEvent::GestureScrollEnd));
  }
}

// Ensure that vh units are correct when browser controls are in a locked
// state. That is, normally we need to add the browser controls height to vh
// units since 100vh includes the browser controls even if they're hidden while
// the ICB height does not. When the controls are locked hidden, the ICB size
// is the full viewport height so there's no need to add the browser controls
// height.  crbug.com/688738.
TEST_F(BrowserControlsTest, MAYBE(ViewportUnitsWhenControlsLocked)) {
  // Initialize with the browser controls showing.
  WebViewImpl* webView = initialize("vh-height.html");
  webView->resizeWithBrowserControls(WebSize(400, 300), 100.f, true);
  webView->updateBrowserControlsState(WebBrowserControlsBoth,
                                      WebBrowserControlsShown, false);
  webView->browserControls().setShownRatio(1);
  webView->updateAllLifecyclePhases();

  ASSERT_EQ(100.f, webView->browserControls().contentOffset());
  ASSERT_EQ(300, frame()->view()->layoutSize().height());

  Element* absPos = getElementById(WebString::fromUTF8("abs"));
  Element* fixedPos = getElementById(WebString::fromUTF8("fixed"));

  // Lock the browser controls to hidden.
  {
    webView->updateBrowserControlsState(WebBrowserControlsHidden,
                                        WebBrowserControlsHidden, false);
    webView->resizeWithBrowserControls(WebSize(400, 400), 100.f, false);
    webView->updateAllLifecyclePhases();

    ASSERT_EQ(0.f, webView->browserControls().contentOffset());
    ASSERT_EQ(400, frame()->view()->layoutSize().height());

    // Make sure we're not adding the browser controls height to the vh units
    // as when they're locked to hidden, the ICB fills the entire viewport
    // already.
    EXPECT_FLOAT_EQ(200.f, absPos->getBoundingClientRect()->height());
    EXPECT_FLOAT_EQ(200.f, fixedPos->getBoundingClientRect()->height());
    EXPECT_EQ(400, frame()->view()->viewportSizeForViewportUnits().height());
  }

  // Lock the browser controls to shown. This should cause the vh units to
  // behave as usual by including the browser controls region in 100vh.
  {
    webView->updateBrowserControlsState(WebBrowserControlsShown,
                                        WebBrowserControlsShown, false);
    webView->resizeWithBrowserControls(WebSize(400, 300), 100.f, true);
    webView->updateAllLifecyclePhases();

    ASSERT_EQ(100.f, webView->browserControls().contentOffset());
    ASSERT_EQ(300, frame()->view()->layoutSize().height());

    // Make sure we're not adding the browser controls height to the vh units as
    // when they're locked to hidden, the ICB fills the entire viewport already.
    EXPECT_FLOAT_EQ(200.f, absPos->getBoundingClientRect()->height());
    EXPECT_FLOAT_EQ(200.f, fixedPos->getBoundingClientRect()->height());
    EXPECT_EQ(400, frame()->view()->viewportSizeForViewportUnits().height());
  }
}

}  // namespace blink
