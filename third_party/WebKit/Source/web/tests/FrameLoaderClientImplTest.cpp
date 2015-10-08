/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "web/FrameLoaderClientImpl.h"

#include "core/loader/FrameLoader.h"
#include "platform/weborigin/KURL.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::_;
using testing::Mock;
using testing::Return;

namespace blink {
namespace {

class MockWebFrameClient : public WebFrameClient {
public:
    ~MockWebFrameClient() override { }

    MOCK_METHOD1(userAgentOverride, WebString(WebLocalFrame*));
};

class FrameLoaderClientImplTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ON_CALL(m_webFrameClient, userAgentOverride(_)).WillByDefault(Return(WebString()));

        FrameTestHelpers::TestWebViewClient webViewClient;
        m_webView = WebView::create(&webViewClient);
        // FIXME: http://crbug.com/363843. This needs to find a better way to
        // not create graphics layers.
        m_webView->settings()->setAcceleratedCompositingEnabled(false);
        m_mainFrame = WebLocalFrame::create(WebTreeScopeType::Document, &m_webFrameClient);
        m_webView->setMainFrame(m_mainFrame);
    }

    void TearDown() override
    {
        m_webView->close();
        m_mainFrame->close();
    }

    WebString userAgent()
    {
        // The test always returns the same user agent .
        WTF::CString userAgent = frameLoaderClient().userAgent().utf8();
        return WebString::fromUTF8(userAgent.data(), userAgent.length());
    }

    WebLocalFrameImpl* mainFrame() { return toWebLocalFrameImpl(m_webView->mainFrame()); }
    Document& document() { return *toWebLocalFrameImpl(m_mainFrame)->frame()->document(); }
    MockWebFrameClient& webFrameClient() { return m_webFrameClient; }
    FrameLoaderClient& frameLoaderClient() { return *toFrameLoaderClientImpl(toWebLocalFrameImpl(m_webView->mainFrame())->frame()->loader().client()); }

private:
    MockWebFrameClient m_webFrameClient;
    WebView* m_webView;
    WebFrame* m_mainFrame;
};

TEST_F(FrameLoaderClientImplTest, UserAgentOverride)
{
    const WebString defaultUserAgent = userAgent();
    const WebString overrideUserAgent = WebString::fromUTF8("dummy override");

    // Override the user agent and make sure we get it back.
    EXPECT_CALL(webFrameClient(), userAgentOverride(_)).WillOnce(Return(overrideUserAgent));
    EXPECT_TRUE(overrideUserAgent.equals(userAgent()));
    Mock::VerifyAndClearExpectations(&webFrameClient());

    // Remove the override and make sure we get the original back.
    EXPECT_CALL(webFrameClient(), userAgentOverride(_)).WillOnce(Return(WebString()));
    EXPECT_TRUE(defaultUserAgent.equals(userAgent()));
}

} // namespace
} // namespace blink
