// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/document_loader.h"

#include <utility>
#include "base/auto_reset.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/scoped_fake_plugin_registry.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

class DocumentLoaderTest : public testing::Test {
 protected:
  void SetUp() override {
    web_view_helper_.Initialize();
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("https://example.com/foo.html"),
        test::CoreTestDataPath("foo.html"));
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("https://example.com:8000/foo.html"),
        test::CoreTestDataPath("foo.html"));
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  class ScopedLoaderDelegate {
   public:
    ScopedLoaderDelegate(WebURLLoaderTestDelegate* delegate) {
      url_test_helpers::SetLoaderDelegate(delegate);
    }
    ~ScopedLoaderDelegate() { url_test_helpers::SetLoaderDelegate(nullptr); }
  };

  WebLocalFrameImpl* MainFrame() { return web_view_helper_.LocalMainFrame(); }

  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(DocumentLoaderTest, SingleChunk) {
  class TestDelegate : public WebURLLoaderTestDelegate {
   public:
    void DidReceiveData(WebURLLoaderClient* original_client,
                        const char* data,
                        int data_length) override {
      EXPECT_EQ(34, data_length) << "foo.html was not served in a single chunk";
      original_client->DidReceiveData(data, data_length);
    }
  } delegate;

  ScopedLoaderDelegate loader_delegate(&delegate);
  frame_test_helpers::LoadFrame(MainFrame(), "https://example.com/foo.html");

  // TODO(dcheng): How should the test verify that the original callback is
  // invoked? The test currently still passes even if the test delegate
  // forgets to invoke the callback.
}

// Test normal case of DocumentLoader::dataReceived(): data in multiple chunks,
// with no reentrancy.
TEST_F(DocumentLoaderTest, MultiChunkNoReentrancy) {
  class TestDelegate : public WebURLLoaderTestDelegate {
   public:
    void DidReceiveData(WebURLLoaderClient* original_client,
                        const char* data,
                        int data_length) override {
      EXPECT_EQ(34, data_length) << "foo.html was not served in a single chunk";
      // Chunk the reply into one byte chunks.
      for (int i = 0; i < data_length; ++i)
        original_client->DidReceiveData(&data[i], 1);
    }
  } delegate;

  ScopedLoaderDelegate loader_delegate(&delegate);
  frame_test_helpers::LoadFrame(MainFrame(), "https://example.com/foo.html");
}

// Finally, test reentrant callbacks to DocumentLoader::BodyDataReceived().
TEST_F(DocumentLoaderTest, MultiChunkWithReentrancy) {
  // This test delegate chunks the response stage into three distinct stages:
  // 1. The first BodyDataReceived() callback, which triggers frame detach
  //    due to committing a provisional load.
  // 2. The middle part of the response, which is dispatched to
  //    BodyDataReceived() reentrantly.
  // 3. The final chunk, which is dispatched normally at the top-level.
  class MainFrameClient : public WebURLLoaderTestDelegate,
                          public frame_test_helpers::TestWebFrameClient {
   public:
    // WebURLLoaderTestDelegate overrides:
    bool FillNavigationParamsResponse(WebNavigationParams* params) override {
      params->response = WebURLResponse(params->url);
      params->response.SetMimeType("application/pdf");
      params->response.SetHttpStatusCode(200);

      String data("<html><body>foo</body></html>");
      for (wtf_size_t i = 0; i < data.length(); i++)
        data_.push_back(data[i]);

      auto body_loader = std::make_unique<StaticDataNavigationBodyLoader>();
      body_loader_ = body_loader.get();
      params->body_loader = std::move(body_loader);
      return true;
    }

    void Serve() {
      {
        // Serve the first byte to the real WebURLLoaderCLient, which
        // should trigger frameDetach() due to committing a provisional
        // load.
        base::AutoReset<bool> dispatching(&dispatching_did_receive_data_, true);
        DispatchOneByte();
      }

      // Serve the remaining bytes to complete the load.
      EXPECT_FALSE(data_.IsEmpty());
      while (!data_.IsEmpty())
        DispatchOneByte();

      body_loader_->Finish();
      body_loader_ = nullptr;
    }

    // WebLocalFrameClient overrides:
    void RunScriptsAtDocumentElementAvailable() override {
      if (dispatching_did_receive_data_) {
        // This should be called by the first BodyDataReceived() call, since
        // it should create a plugin document structure and trigger this.
        EXPECT_GT(data_.size(), 10u);
        // Dispatch BodyDataReceived() callbacks for part of the remaining
        // data, saving the rest to be dispatched at the top-level as
        // normal.
        while (data_.size() > 10)
          DispatchOneByte();
        served_reentrantly_ = true;
      }
      TestWebFrameClient::RunScriptsAtDocumentElementAvailable();
    }

    void DispatchOneByte() {
      char c = data_.TakeFirst();
      body_loader_->Write(&c, 1);
    }

    bool ServedReentrantly() const { return served_reentrantly_; }

   private:
    Deque<char> data_;
    bool dispatching_did_receive_data_ = false;
    bool served_reentrantly_ = false;
    StaticDataNavigationBodyLoader* body_loader_ = nullptr;
  };

  // We use a plugin document triggered by "application/pdf" mime type,
  // because that gives us reliable way to get a WebLocalFrameClient callback
  // from inside BodyDataReceived() call.
  ScopedFakePluginRegistry fake_plugins;
  MainFrameClient main_frame_client;
  web_view_helper_.Initialize(&main_frame_client);
  web_view_helper_.GetWebView()->GetPage()->GetSettings().SetPluginsEnabled(
      true);

  {
    ScopedLoaderDelegate loader_delegate(&main_frame_client);
    frame_test_helpers::LoadFrameDontWait(
        MainFrame(), url_test_helpers::ToKURL("https://example.com/foo.html"));
    main_frame_client.Serve();
    frame_test_helpers::PumpPendingRequestsForFrameToLoad(MainFrame());
  }

  // Sanity check that we did actually test reeentrancy.
  EXPECT_TRUE(main_frame_client.ServedReentrantly());

  // MainFrameClient is stack-allocated, so manually Reset to avoid UAF.
  web_view_helper_.Reset();
}

TEST_F(DocumentLoaderTest, isCommittedButEmpty) {
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("about:blank");
  EXPECT_TRUE(To<LocalFrame>(web_view_impl->GetPage()->MainFrame())
                  ->Loader()
                  .GetDocumentLoader()
                  ->IsCommittedButEmpty());
}

TEST_F(DocumentLoaderTest, MixedContentOptOutSetIfHeaderReceived) {
  WebURL url =
      url_test_helpers::ToKURL("https://examplenoupgrade.com/foo.html");
  WebURLResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField("mixed-content", "noupgrade");
  response.SetMimeType("text/html");
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      url, test::CoreTestDataPath("foo.html"), response);
  WebViewImpl* web_view_impl = web_view_helper_.InitializeAndLoad(
      "https://examplenoupgrade.com/foo.html");
  EXPECT_TRUE(To<LocalFrame>(web_view_impl->GetPage()->MainFrame())
                  ->GetDocument()
                  ->GetMixedAutoUpgradeOptOut());
}

TEST_F(DocumentLoaderTest, MixedContentOptOutNotSetIfNoHeaderReceived) {
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");
  EXPECT_FALSE(To<LocalFrame>(web_view_impl->GetPage()->MainFrame())
                   ->Loader()
                   .GetDocumentLoader()
                   ->GetFrame()
                   ->GetDocument()
                   ->GetMixedAutoUpgradeOptOut());
}

class DocumentLoaderSimTest : public SimTest {};

TEST_F(DocumentLoaderSimTest, DocumentOpenUpdatesUrl) {
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Write("<iframe src='javascript:42;'></iframe>");

  auto* child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* child_document = child_frame->GetFrame()->GetDocument();
  EXPECT_TRUE(child_document->HasPendingJavaScriptUrlsForTest());

  main_resource.Write(
      "<script>"
      "window[0].document.open();"
      "window[0].document.write('hello');"
      "window[0].document.close();"
      "</script>");

  main_resource.Finish();

  // document.open() should have cancelled the pending JavaScript URLs.
  EXPECT_FALSE(child_document->HasPendingJavaScriptUrlsForTest());

  // Per https://whatwg.org/C/dynamic-markup-insertion.html#document-open-steps,
  // the URL associated with the Document should match the URL of the entry
  // Document.
  EXPECT_EQ(KURL("https://example.com"), child_document->Url());
  // Similarly, the URL of the DocumentLoader should also match.
  EXPECT_EQ(KURL("https://example.com"), child_document->Loader()->Url());
}

TEST_F(DocumentLoaderTest, CommitsDeferredOnSameOriginNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& same_origin_url =
      KURL(NullURL(), "https://www.example.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                same_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_TRUE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_F(DocumentLoaderTest, CommitsNotDeferredOnDifferentOriginNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& other_origin_url =
      KURL(NullURL(), "https://www.another.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                other_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_FALSE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_F(DocumentLoaderTest, CommitsNotDeferredOnDifferentPortNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com:8000/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com:8000/foo.html");

  const KURL& different_port_url =
      KURL(NullURL(), "https://www.example.com:8080/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                different_port_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_FALSE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_F(DocumentLoaderTest, CommitsNotDeferredOnDataURLNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& data_url = KURL(NullURL(), "data:,Hello%2C%20World!");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                data_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_FALSE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

}  // namespace blink
