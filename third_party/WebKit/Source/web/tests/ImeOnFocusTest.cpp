// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "public/platform/Platform.h"
#include "public/platform/WebUnitTestSupport.h"
#include "web/tests/FrameTestHelpers.h"
#include "web/tests/URLTestHelpers.h"

#include <gtest/gtest.h>

using namespace blink;
using blink::FrameTestHelpers::runPendingTasks;

namespace {

class ImeRequestTrackingWebViewClient : public FrameTestHelpers::TestWebViewClient {
public:
    ImeRequestTrackingWebViewClient() :
        m_imeRequestCount(0)
    {
    }

    // WebWidgetClient methods
    virtual void showImeIfNeeded() OVERRIDE
    {
        ++m_imeRequestCount;
    }

    // Local methds
    void reset()
    {
        m_imeRequestCount = 0;
    }

    int imeRequestCount()
    {
        return m_imeRequestCount;
    }

private:
    int m_imeRequestCount;
};

class ImeOnFocusTest : public testing::Test {
public:
    ImeOnFocusTest()
        : m_baseURL("http://www.test.com/")
    {
    }

    virtual void TearDown()
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

protected:
    void sendGestureTap(WebView*, blink::IntPoint);
    void runImeOnFocusTest(std::string, int, blink::IntPoint tapPoint = blink::IntPoint(-1, -1));

    std::string m_baseURL;
    FrameTestHelpers::WebViewHelper m_webViewHelper;
};

void ImeOnFocusTest::sendGestureTap(WebView* webView, blink::IntPoint clientPoint)
{
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureTap;
    webGestureEvent.x = clientPoint.x();
    webGestureEvent.y = clientPoint.y();
    webGestureEvent.globalX = clientPoint.x();
    webGestureEvent.globalY = clientPoint.y();
    webGestureEvent.data.tap.tapCount = 1;
    webGestureEvent.data.tap.width = 10;
    webGestureEvent.data.tap.height = 10;

    webView->handleInputEvent(webGestureEvent);
    FrameTestHelpers::runPendingTasks();
}

void ImeOnFocusTest::runImeOnFocusTest(std::string file, int expectedImeRequestCount, blink::IntPoint tapPoint)
{
    ImeRequestTrackingWebViewClient client;
    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL), WebString::fromUTF8(file));
    WebView* webView = m_webViewHelper.initialize(true, 0, &client);
    m_webViewHelper.webView()->setPageScaleFactorLimits(1, 1);
    webView->resize(WebSize(800, 1200));

    EXPECT_EQ(0, client.imeRequestCount());
    FrameTestHelpers::loadFrame(webView->mainFrame(), m_baseURL + file);
    if (tapPoint.x() >= 0 && tapPoint.y() >= 0)
        sendGestureTap(webView, tapPoint);
    EXPECT_EQ(expectedImeRequestCount, client.imeRequestCount());

    m_webViewHelper.reset();
}

TEST_F(ImeOnFocusTest, OnLoad)
{
    runImeOnFocusTest("ime-on-focus-on-load.html", 0);
}

TEST_F(ImeOnFocusTest, OnAutofocus)
{
    runImeOnFocusTest("ime-on-focus-on-autofocus.html", 0);
}

TEST_F(ImeOnFocusTest, OnUserGesture)
{
    runImeOnFocusTest("ime-on-focus-on-user-gesture.html", 1, blink::IntPoint(50, 50));
}

}
