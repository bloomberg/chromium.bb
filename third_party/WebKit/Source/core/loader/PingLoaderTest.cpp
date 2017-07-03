// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/PingLoader.h"

#include "core/frame/LocalFrame.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoader.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/loader/fetch/SubstituteData.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class PingLocalFrameClient : public EmptyLocalFrameClient {
 public:
  void DispatchWillSendRequest(ResourceRequest& request) override {
    if (request.HttpContentType() == "text/ping")
      ping_request_ = request;
  }

  const ResourceRequest& PingRequest() const { return ping_request_; }

 private:
  ResourceRequest ping_request_;
};

class PingLoaderTest : public ::testing::Test {
 public:
  void SetUp() override {
    client_ = new PingLocalFrameClient;
    page_holder_ = DummyPageHolder::Create(IntSize(1, 1), nullptr, client_);
  }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void SetDocumentURL(const KURL& url) {
    FrameLoadRequest request(nullptr, ResourceRequest(url),
                             SubstituteData(SharedBuffer::Create(), "text/html",
                                            "UTF-8", NullURL()));
    page_holder_->GetFrame().Loader().Load(request);
    blink::testing::RunPendingTasks();
    ASSERT_EQ(url.GetString(), page_holder_->GetDocument().Url().GetString());
  }

  const ResourceRequest& PingAndGetRequest(const KURL& ping_url) {
    KURL destination_url(kParsedURLString, "http://navigation.destination");
    URLTestHelpers::RegisterMockedURLLoad(
        ping_url, testing::CoreTestDataPath("bar.html"), "text/html");
    PingLoader::SendLinkAuditPing(&page_holder_->GetFrame(), ping_url,
                                  destination_url);
    const ResourceRequest& ping_request = client_->PingRequest();
    if (!ping_request.IsNull()) {
      EXPECT_EQ(destination_url.GetString(),
                ping_request.HttpHeaderField("Ping-To"));
    }
    // Serve the ping request, since it will otherwise bleed in to the next
    // test, and once begun there is no way to cancel it directly.
    Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
    return ping_request;
  }

 private:
  Persistent<PingLocalFrameClient> client_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(PingLoaderTest, HTTPSToHTTPS) {
  KURL ping_url(kParsedURLString, "https://localhost/bar.html");
  SetDocumentURL(KURL(kParsedURLString, "https://127.0.0.1:8000/foo.html"));
  const ResourceRequest& ping_request = PingAndGetRequest(ping_url);
  ASSERT_FALSE(ping_request.IsNull());
  EXPECT_EQ(ping_url, ping_request.Url());
  EXPECT_EQ(String(), ping_request.HttpHeaderField("Ping-From"));
}

TEST_F(PingLoaderTest, HTTPToHTTPS) {
  KURL document_url(kParsedURLString, "http://127.0.0.1:8000/foo.html");
  KURL ping_url(kParsedURLString, "https://localhost/bar.html");
  SetDocumentURL(document_url);
  const ResourceRequest& ping_request = PingAndGetRequest(ping_url);
  ASSERT_FALSE(ping_request.IsNull());
  EXPECT_EQ(ping_url, ping_request.Url());
  EXPECT_EQ(document_url.GetString(),
            ping_request.HttpHeaderField("Ping-From"));
}

TEST_F(PingLoaderTest, NonHTTPPingTarget) {
  SetDocumentURL(KURL(kParsedURLString, "http://127.0.0.1:8000/foo.html"));
  const ResourceRequest& ping_request =
      PingAndGetRequest(KURL(kParsedURLString, "ftp://localhost/bar.html"));
  ASSERT_TRUE(ping_request.IsNull());
}

}  // namespace

}  // namespace blink
