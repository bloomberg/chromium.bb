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

#include "core/frame/WebLocalFrameBase.h"
#include "core/loader/FrameLoader.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/WTFString.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/tests/FrameTestHelpers.h"

using testing::_;
using testing::Mock;
using testing::Return;

namespace blink {
namespace {

class MockWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  ~MockWebFrameClient() override {}

  MOCK_METHOD0(UserAgentOverride, WebString());
};

class LocalFrameClientImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ON_CALL(web_frame_client_, UserAgentOverride())
        .WillByDefault(Return(WebString()));

    FrameTestHelpers::TestWebViewClient web_view_client;
    web_view_ =
        WebView::Create(&web_view_client, kWebPageVisibilityStateVisible);
    // FIXME: http://crbug.com/363843. This needs to find a better way to
    // not create graphics layers.
    web_view_->GetSettings()->SetAcceleratedCompositingEnabled(false);
    main_frame_ = WebLocalFrame::Create(WebTreeScopeType::kDocument,
                                        &web_frame_client_, nullptr, nullptr);
    web_view_->SetMainFrame(main_frame_);
  }

  void TearDown() override { web_view_->Close(); }

  WebString UserAgent() {
    // The test always returns the same user agent .
    WTF::CString user_agent = GetLocalFrameClient().UserAgent().Utf8();
    return WebString::FromUTF8(user_agent.data(), user_agent.length());
  }

  WebLocalFrameBase* MainFrame() {
    return ToWebLocalFrameBase(web_view_->MainFrame());
  }
  Document& GetDocument() {
    return *ToWebLocalFrameBase(main_frame_)->GetFrame()->GetDocument();
  }
  MockWebFrameClient& WebFrameClient() { return web_frame_client_; }
  LocalFrameClient& GetLocalFrameClient() {
    return *ToLocalFrameClientImpl(ToWebLocalFrameBase(web_view_->MainFrame())
                                       ->GetFrame()
                                       ->Loader()
                                       .Client());
  }

 private:
  MockWebFrameClient web_frame_client_;
  WebView* web_view_;
  WebLocalFrame* main_frame_;
};

TEST_F(LocalFrameClientImplTest, UserAgentOverride) {
  const WebString default_user_agent = UserAgent();
  const WebString override_user_agent = WebString::FromUTF8("dummy override");

  // Override the user agent and make sure we get it back.
  EXPECT_CALL(WebFrameClient(), UserAgentOverride())
      .WillOnce(Return(override_user_agent));
  EXPECT_TRUE(override_user_agent.Equals(UserAgent()));
  Mock::VerifyAndClearExpectations(&WebFrameClient());

  // Remove the override and make sure we get the original back.
  EXPECT_CALL(WebFrameClient(), UserAgentOverride())
      .WillOnce(Return(WebString()));
  EXPECT_TRUE(default_user_agent.Equals(UserAgent()));
}

}  // namespace
}  // namespace blink
