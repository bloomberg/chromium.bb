// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/resource/CSSStyleSheetResource.h"

#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserSelector.h"
#include "core/dom/Document.h"
#include "core/loader/resource/ImageResource.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/ResourceRequest.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class Document;

namespace {

class CSSStyleSheetResourceTest : public ::testing::Test {
 protected:
  CSSStyleSheetResourceTest() {
    m_originalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());
    m_page = DummyPageHolder::create();
    document().setURL(KURL(KURL(), "https://localhost/"));
  }

  ~CSSStyleSheetResourceTest() override {
    replaceMemoryCacheForTesting(m_originalMemoryCache.release());
  }

  Document& document() { return m_page->document(); }

  Persistent<MemoryCache> m_originalMemoryCache;
  std::unique_ptr<DummyPageHolder> m_page;
};

TEST_F(CSSStyleSheetResourceTest, DuplicateResourceNotCached) {
  const char url[] = "https://localhost/style.css";
  KURL imageURL(KURL(), url);
  KURL cssURL(KURL(), url);

  // Emulate using <img> to do async stylesheet preloads.

  Resource* imageResource = ImageResource::create(ResourceRequest(imageURL));
  ASSERT_TRUE(imageResource);
  memoryCache()->add(imageResource);
  ASSERT_TRUE(memoryCache()->contains(imageResource));

  CSSStyleSheetResource* cssResource =
      CSSStyleSheetResource::createForTest(ResourceRequest(cssURL), "utf-8");
  cssResource->responseReceived(
      ResourceResponse(cssURL, "style/css", 0, nullAtom), nullptr);
  cssResource->finish();

  CSSParserContext* parserContext = CSSParserContext::create(HTMLStandardMode);
  StyleSheetContents* contents = StyleSheetContents::create(parserContext);
  CSSStyleSheet* sheet = CSSStyleSheet::create(contents, document());
  EXPECT_TRUE(sheet);

  contents->checkLoaded();
  cssResource->saveParsedStyleSheet(contents);

  // Verify that the cache will have a mapping for |imageResource| at |url|.
  // The underlying |contents| for the stylesheet resource must have a
  // matching reference status.
  EXPECT_TRUE(memoryCache()->contains(imageResource));
  EXPECT_FALSE(memoryCache()->contains(cssResource));
  EXPECT_FALSE(contents->isReferencedFromResource());
  EXPECT_FALSE(cssResource->restoreParsedStyleSheet(parserContext));
}

}  // namespace
}  // namespace blink
