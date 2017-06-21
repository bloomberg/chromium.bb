// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/CSSPreloadScanner.h"

#include <memory>
#include "core/frame/Settings.h"
#include "core/html/parser/HTMLResourcePreloader.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/heap/Heap.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/TextEncoding.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class MockHTMLResourcePreloader : public HTMLResourcePreloader {
  WTF_MAKE_NONCOPYABLE(MockHTMLResourcePreloader);

 public:
  explicit MockHTMLResourcePreloader(Document& document,
                                     const char* expected_referrer = nullptr)
      : HTMLResourcePreloader(document),
        expected_referrer_(expected_referrer) {}

  void Preload(std::unique_ptr<PreloadRequest> preload_request,
               const NetworkHintsInterface&) override {
    if (expected_referrer_) {
      Resource* resource = preload_request->Start(GetDocument());
      if (resource) {
        EXPECT_EQ(expected_referrer_,
                  resource->GetResourceRequest().HttpReferrer());
      }
    }
  }

  const char* expected_referrer_;
};

class PreloadRecordingCSSPreloaderResourceClient final
    : public CSSPreloaderResourceClient {
 public:
  PreloadRecordingCSSPreloaderResourceClient(Resource* resource,
                                             HTMLResourcePreloader* preloader)
      : CSSPreloaderResourceClient(resource, preloader) {}

  void FetchPreloads(PreloadRequestStream& preloads) override {
    for (const auto& it : preloads) {
      preload_urls_.push_back(it->ResourceURL());
      preload_referrer_policies_.push_back(it->GetReferrerPolicy());
    }
    CSSPreloaderResourceClient::FetchPreloads(preloads);
  }

  Vector<String> preload_urls_;
  Vector<ReferrerPolicy> preload_referrer_policies_;
};

class CSSPreloadScannerTest : public ::testing::Test {};

}  // namespace

TEST_F(CSSPreloadScannerTest, ScanFromResourceClient) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  dummy_page_holder->GetDocument()
      .GetSettings()
      ->SetCSSExternalScannerNoPreload(true);

  MockHTMLResourcePreloader* preloader =
      new MockHTMLResourcePreloader(dummy_page_holder->GetDocument());

  KURL url(kParsedURLString, "http://127.0.0.1/foo.css");
  CSSStyleSheetResource* resource =
      CSSStyleSheetResource::CreateForTest(url, UTF8Encoding());
  resource->SetStatus(ResourceStatus::kPending);

  PreloadRecordingCSSPreloaderResourceClient* resource_client =
      new PreloadRecordingCSSPreloaderResourceClient(resource, preloader);

  const char* data = "@import url('http://127.0.0.1/preload.css');";
  resource->AppendData(data, strlen(data));

  EXPECT_EQ(Resource::kPreloadNotReferenced, resource->GetPreloadResult());
  EXPECT_EQ(1u, resource_client->preload_urls_.size());
  EXPECT_EQ("http://127.0.0.1/preload.css",
            resource_client->preload_urls_.front());
}

// Regression test for crbug.com/608310 where the client is destroyed but was
// not removed from the resource's client list.
TEST_F(CSSPreloadScannerTest, DestroyClientBeforeDataSent) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  dummy_page_holder->GetDocument()
      .GetSettings()
      ->SetCSSExternalScannerNoPreload(true);

  Persistent<MockHTMLResourcePreloader> preloader =
      new MockHTMLResourcePreloader(dummy_page_holder->GetDocument());

  KURL url(kParsedURLString, "http://127.0.0.1/foo.css");
  Persistent<CSSStyleSheetResource> resource =
      CSSStyleSheetResource::CreateForTest(url, UTF8Encoding());
  resource->SetStatus(ResourceStatus::kPending);

  new PreloadRecordingCSSPreloaderResourceClient(resource, preloader);

  // Destroys the resourceClient.
  ThreadState::Current()->CollectAllGarbage();

  const char* data = "@import url('http://127.0.0.1/preload.css');";
  // Should not crash.
  resource->AppendData(data, strlen(data));
}

// Regression test for crbug.com/646869 where the client's data is cleared
// before didAppendFirstData is called.
TEST_F(CSSPreloadScannerTest, DontReadFromClearedData) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  dummy_page_holder->GetDocument()
      .GetSettings()
      ->SetCSSExternalScannerNoPreload(true);

  MockHTMLResourcePreloader* preloader =
      new MockHTMLResourcePreloader(dummy_page_holder->GetDocument());

  KURL url(kParsedURLString, "http://127.0.0.1/foo.css");
  CSSStyleSheetResource* resource =
      CSSStyleSheetResource::CreateForTest(url, UTF8Encoding());

  const char* data = "@import url('http://127.0.0.1/preload.css');";
  resource->AppendData(data, strlen(data));
  ResourceError error(kErrorDomainBlinkInternal, 0, url.GetString(), "");
  resource->FinishAsError(error);

  // Should not crash.
  PreloadRecordingCSSPreloaderResourceClient* resource_client =
      new PreloadRecordingCSSPreloaderResourceClient(resource, preloader);

  EXPECT_EQ(0u, resource_client->preload_urls_.size());
}

// Regression test for crbug.com/645331, where a resource client gets callbacks
// after the document is shutdown and we have no frame.
TEST_F(CSSPreloadScannerTest, DoNotExpectValidDocument) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  dummy_page_holder->GetDocument()
      .GetSettings()
      ->SetCSSExternalScannerNoPreload(true);

  MockHTMLResourcePreloader* preloader =
      new MockHTMLResourcePreloader(dummy_page_holder->GetDocument());

  KURL url(kParsedURLString, "http://127.0.0.1/foo.css");
  CSSStyleSheetResource* resource =
      CSSStyleSheetResource::CreateForTest(url, UTF8Encoding());
  resource->SetStatus(ResourceStatus::kPending);

  PreloadRecordingCSSPreloaderResourceClient* resource_client =
      new PreloadRecordingCSSPreloaderResourceClient(resource, preloader);

  dummy_page_holder->GetDocument().Shutdown();

  const char* data = "@import url('http://127.0.0.1/preload.css');";
  resource->AppendData(data, strlen(data));

  // Do not expect to gather any preloads, as the document loader is invalid,
  // which means we can't notify WebLoadingBehaviorData of the preloads.
  EXPECT_EQ(0u, resource_client->preload_urls_.size());
}

TEST_F(CSSPreloadScannerTest, ReferrerPolicyHeader) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  dummy_page_holder->GetDocument().GetSettings()->SetCSSExternalScannerPreload(
      true);

  MockHTMLResourcePreloader* preloader = new MockHTMLResourcePreloader(
      dummy_page_holder->GetDocument(), "http://127.0.0.1/foo.css");

  KURL url(kParsedURLString, "http://127.0.0.1/foo.css");
  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);
  response.SetHTTPHeaderField("referrer-policy", "unsafe-url");
  CSSStyleSheetResource* resource =
      CSSStyleSheetResource::CreateForTest(url, UTF8Encoding());
  resource->SetStatus(ResourceStatus::kPending);
  resource->SetResponse(response);

  PreloadRecordingCSSPreloaderResourceClient* resource_client =
      new PreloadRecordingCSSPreloaderResourceClient(resource, preloader);

  KURL preload_url(kParsedURLString, "http://127.0.0.1/preload.css");
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      preload_url, WrappedResourceResponse(ResourceResponse()), "");

  const char* data = "@import url('http://127.0.0.1/preload.css');";
  resource->AppendData(data, strlen(data));

  EXPECT_EQ(Resource::kPreloadNotReferenced, resource->GetPreloadResult());
  EXPECT_EQ(1u, resource_client->preload_urls_.size());
  EXPECT_EQ("http://127.0.0.1/preload.css",
            resource_client->preload_urls_.front());
  EXPECT_EQ(kReferrerPolicyAlways,
            resource_client->preload_referrer_policies_.front());
}

}  // namespace blink
