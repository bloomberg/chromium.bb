// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/CSSPreloadScanner.h"

#include "core/fetch/FetchContext.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/Settings.h"
#include "core/html/parser/HTMLResourcePreloader.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Heap.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class PreloadSuppressingCSSPreloaderResourceClient final
    : public CSSPreloaderResourceClient {
 public:
  PreloadSuppressingCSSPreloaderResourceClient(Resource* resource,
                                               HTMLResourcePreloader* preloader)
      : CSSPreloaderResourceClient(resource, preloader) {}
  void fetchPreloads(PreloadRequestStream& preloads) override {
    PreloadRequestStream movedPreloads;
    movedPreloads.swap(preloads);
    for (PreloadRequestStream::iterator it = movedPreloads.begin();
         it != movedPreloads.end(); ++it) {
      m_preloads.append(std::move(*it));
    }
  }

  PreloadRequestStream m_preloads;
};

class CSSPreloadScannerTest : public ::testing::Test {};

TEST_F(CSSPreloadScannerTest, ScanFromResourceClient) {
  std::unique_ptr<DummyPageHolder> dummyPageHolder =
      DummyPageHolder::create(IntSize(500, 500));
  dummyPageHolder->document().settings()->setCSSExternalScannerNoPreload(true);

  HTMLResourcePreloader* preloader =
      HTMLResourcePreloader::create(dummyPageHolder->document());

  KURL url(ParsedURLString, "http://127.0.0.1/foo.css");
  CSSStyleSheetResource* resource =
      CSSStyleSheetResource::createForTest(ResourceRequest(url), "utf-8");
  resource->setStatus(Resource::Pending);

  PreloadSuppressingCSSPreloaderResourceClient* resourceClient =
      new PreloadSuppressingCSSPreloaderResourceClient(resource, preloader);

  const char* data = "@import url('http://127.0.0.1/preload.css');";
  resource->appendData(data, strlen(data));

  EXPECT_EQ(1u, resourceClient->m_preloads.size());
  EXPECT_EQ("http://127.0.0.1/preload.css",
            resourceClient->m_preloads.first()->resourceURL());
}

// Regression test for crbug.com/608310 where the client is destroyed but was
// not removed from the resource's client list.
TEST_F(CSSPreloadScannerTest, DestroyClientBeforeDataSent) {
  std::unique_ptr<DummyPageHolder> dummyPageHolder =
      DummyPageHolder::create(IntSize(500, 500));
  dummyPageHolder->document().settings()->setCSSExternalScannerNoPreload(true);

  Persistent<HTMLResourcePreloader> preloader =
      HTMLResourcePreloader::create(dummyPageHolder->document());

  KURL url(ParsedURLString, "http://127.0.0.1/foo.css");
  Persistent<CSSStyleSheetResource> resource =
      CSSStyleSheetResource::createForTest(ResourceRequest(url), "utf-8");
  resource->setStatus(Resource::Pending);

  new PreloadSuppressingCSSPreloaderResourceClient(resource, preloader);

  // Destroys the resourceClient.
  ThreadState::current()->collectAllGarbage();

  const char* data = "@import url('http://127.0.0.1/preload.css');";
  // Should not crash.
  resource->appendData(data, strlen(data));
}

}  // namespace blink
