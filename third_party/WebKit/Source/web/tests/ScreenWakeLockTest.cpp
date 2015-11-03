// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/wake_lock/ScreenWakeLock.h"

#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentInit.h"
#include "core/frame/LocalDOMWindow.h"
#include "platform/heap/Handle.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebPageVisibilityState.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/platform/modules/wake_lock/WebWakeLockClient.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include <gtest/gtest.h>

namespace {

using blink::ScreenWakeLock;
using blink::WebWakeLockClient;

class TestWebWakeLockClient: public WebWakeLockClient {
public:
    TestWebWakeLockClient(): m_keepScreenAwake(false) { }

    void requestKeepScreenAwake(bool keepScreenAwake) override
    {
        m_keepScreenAwake = keepScreenAwake;
    }

    bool keepScreenAwake() const { return m_keepScreenAwake; }

private:
    bool m_keepScreenAwake;
};

class TestWebFrameClient: public blink::FrameTestHelpers::TestWebFrameClient {
public:
    WebWakeLockClient* wakeLockClient() override
    {
        return &m_testWebWakeLockClient;
    }

    const TestWebWakeLockClient& testWebWakeLockClient() const
    {
        return m_testWebWakeLockClient;
    }

private:
    TestWebWakeLockClient m_testWebWakeLockClient;
};

class ScreenWakeLockTest: public testing::Test {
protected:
    void SetUp() override
    {
        m_webViewHelper.initialize(true, &m_testWebFrameClient);
        blink::URLTestHelpers::registerMockedURLFromBaseURL(
            blink::WebString::fromUTF8("http://example.com/"),
            blink::WebString::fromUTF8("foo.html"));
        loadFrame();
    }

    void TearDown() override
    {
        blink::Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    void loadFrame()
    {
        blink::FrameTestHelpers::loadFrame(
            m_webViewHelper.webView()->mainFrame(),
            "http://example.com/foo.html");
        m_webViewHelper.webViewImpl()->updateAllLifecyclePhases();
    }

    blink::LocalFrame* frame()
    {
        ASSERT(m_webViewHelper.webViewImpl());
        ASSERT(m_webViewHelper.webViewImpl()->mainFrameImpl());
        return m_webViewHelper.webViewImpl()->mainFrameImpl()->frame();
    }

    blink::Screen* screen()
    {
        ASSERT(frame());
        ASSERT(frame()->localDOMWindow());
        return frame()->localDOMWindow()->screen();
    }

    bool screenKeepAwake()
    {
        ASSERT(screen());
        return ScreenWakeLock::keepAwake(*screen());
    }

    bool clientKeepScreenAwake()
    {
        return m_testWebFrameClient.testWebWakeLockClient().keepScreenAwake();
    }

    void setKeepAwake(bool keepAwake)
    {
        ASSERT(screen());
        ScreenWakeLock::setKeepAwake(*screen(), keepAwake);
    }

    void show()
    {
        ASSERT(m_webViewHelper.webView());
        m_webViewHelper.webView()->setVisibilityState(
            blink::WebPageVisibilityStateVisible, false);
    }

    void hide()
    {
        ASSERT(m_webViewHelper.webView());
        m_webViewHelper.webView()->setVisibilityState(
            blink::WebPageVisibilityStateHidden, false);
    }

    // Order of these members is important as we need to make sure that
    // m_testWebFrameClient outlives m_webViewHelper (destruction order)
    TestWebFrameClient m_testWebFrameClient;
    blink::FrameTestHelpers::WebViewHelper m_webViewHelper;
};

TEST_F(ScreenWakeLockTest, setAndReset)
{
    ASSERT_FALSE(screenKeepAwake());
    ASSERT_FALSE(clientKeepScreenAwake());

    setKeepAwake(true);
    EXPECT_TRUE(screenKeepAwake());
    EXPECT_TRUE(clientKeepScreenAwake());

    setKeepAwake(false);
    EXPECT_FALSE(screenKeepAwake());
    EXPECT_FALSE(clientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, hideWhenSet)
{
    ASSERT_FALSE(screenKeepAwake());
    ASSERT_FALSE(clientKeepScreenAwake());

    setKeepAwake(true);
    hide();

    EXPECT_TRUE(screenKeepAwake());
    EXPECT_FALSE(clientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, setWhenHidden)
{
    ASSERT_FALSE(screenKeepAwake());
    ASSERT_FALSE(clientKeepScreenAwake());

    hide();
    setKeepAwake(true);

    EXPECT_TRUE(screenKeepAwake());
    EXPECT_FALSE(clientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, showWhenSet)
{
    ASSERT_FALSE(screenKeepAwake());
    ASSERT_FALSE(clientKeepScreenAwake());

    hide();
    setKeepAwake(true);
    show();

    EXPECT_TRUE(screenKeepAwake());
    EXPECT_TRUE(clientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, navigate)
{
    ASSERT_FALSE(screenKeepAwake());
    ASSERT_FALSE(clientKeepScreenAwake());

    setKeepAwake(true);
    loadFrame();

    EXPECT_FALSE(screenKeepAwake());
    EXPECT_FALSE(clientKeepScreenAwake());
}

} // namespace
