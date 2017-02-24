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

#include "web/LocalFrameClientImpl.h"

#include "core/loader/FrameLoader.h"
#include "platform/weborigin/KURL.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

using testing::_;
using testing::Mock;
using testing::Return;

namespace blink {
namespace {

class MockWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  ~MockWebFrameClient() override {}

  MOCK_METHOD0(userAgentOverride, WebString());
};

class LocalFrameClientImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ON_CALL(m_webFrameClient, userAgentOverride())
        .WillByDefault(Return(WebString()));

    FrameTestHelpers::TestWebViewClient webViewClient;
    m_webView = WebView::create(&webViewClient, WebPageVisibilityStateVisible);
    // FIXME: http://crbug.com/363843. This needs to find a better way to
    // not create graphics layers.
    m_webView->settings()->setAcceleratedCompositingEnabled(false);
    m_mainFrame = WebLocalFrame::create(WebTreeScopeType::Document,
                                        &m_webFrameClient, nullptr, nullptr);
    m_webView->setMainFrame(m_mainFrame);
  }

  void TearDown() override { m_webView->close(); }

  WebString userAgent() {
    // The test always returns the same user agent .
    WTF::CString userAgent = localFrameClient().userAgent().utf8();
    return WebString::fromUTF8(userAgent.data(), userAgent.length());
  }

  WebLocalFrameImpl* mainFrame() {
    return toWebLocalFrameImpl(m_webView->mainFrame());
  }
  Document& document() {
    return *toWebLocalFrameImpl(m_mainFrame)->frame()->document();
  }
  MockWebFrameClient& webFrameClient() { return m_webFrameClient; }
  LocalFrameClient& localFrameClient() {
    return *toLocalFrameClientImpl(toWebLocalFrameImpl(m_webView->mainFrame())
                                       ->frame()
                                       ->loader()
                                       .client());
  }

 private:
  MockWebFrameClient m_webFrameClient;
  WebView* m_webView;
  WebLocalFrame* m_mainFrame;
};

TEST_F(LocalFrameClientImplTest, UserAgentOverride) {
  const WebString defaultUserAgent = userAgent();
  const WebString overrideUserAgent = WebString::fromUTF8("dummy override");

  // Override the user agent and make sure we get it back.
  EXPECT_CALL(webFrameClient(), userAgentOverride())
      .WillOnce(Return(overrideUserAgent));
  EXPECT_TRUE(overrideUserAgent.equals(userAgent()));
  Mock::VerifyAndClearExpectations(&webFrameClient());

  // Remove the override and make sure we get the original back.
  EXPECT_CALL(webFrameClient(), userAgentOverride())
      .WillOnce(Return(WebString()));
  EXPECT_TRUE(defaultUserAgent.equals(userAgent()));
}

}  // namespace
}  // namespace blink
