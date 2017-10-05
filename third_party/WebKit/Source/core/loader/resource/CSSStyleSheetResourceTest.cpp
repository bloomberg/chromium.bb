// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/resource/CSSStyleSheetResource.h"

#include <memory>
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
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/fetch_initiator_type_names.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/TextEncoding.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class Document;

namespace {

class CSSStyleSheetResourceTest : public ::testing::Test {
 protected:
  CSSStyleSheetResourceTest() {
    original_memory_cache_ =
        ReplaceMemoryCacheForTesting(MemoryCache::Create());
    page_ = DummyPageHolder::Create();
    GetDocument().SetURL(KURL(NullURL(), "https://localhost/"));
  }

  ~CSSStyleSheetResourceTest() override {
    ReplaceMemoryCacheForTesting(original_memory_cache_.Release());
  }

  Document& GetDocument() { return page_->GetDocument(); }

  Persistent<MemoryCache> original_memory_cache_;
  std::unique_ptr<DummyPageHolder> page_;
};

TEST_F(CSSStyleSheetResourceTest, DuplicateResourceNotCached) {
  const char kUrl[] = "https://localhost/style.css";
  KURL image_url(NullURL(), kUrl);
  KURL css_url(NullURL(), kUrl);

  // Emulate using <img> to do async stylesheet preloads.

  Resource* image_resource = ImageResource::CreateForTest(image_url);
  ASSERT_TRUE(image_resource);
  GetMemoryCache()->Add(image_resource);
  ASSERT_TRUE(GetMemoryCache()->Contains(image_resource));

  CSSStyleSheetResource* css_resource =
      CSSStyleSheetResource::CreateForTest(css_url, UTF8Encoding());
  css_resource->ResponseReceived(
      ResourceResponse(css_url, "style/css", 0, g_null_atom), nullptr);
  css_resource->Finish();

  CSSParserContext* parser_context =
      CSSParserContext::Create(kHTMLStandardMode);
  StyleSheetContents* contents = StyleSheetContents::Create(parser_context);
  CSSStyleSheet* sheet = CSSStyleSheet::Create(contents, GetDocument());
  EXPECT_TRUE(sheet);

  contents->CheckLoaded();
  css_resource->SaveParsedStyleSheet(contents);

  // Verify that the cache will have a mapping for |imageResource| at |url|.
  // The underlying |contents| for the stylesheet resource must have a
  // matching reference status.
  EXPECT_TRUE(GetMemoryCache()->Contains(image_resource));
  EXPECT_FALSE(GetMemoryCache()->Contains(css_resource));
  EXPECT_FALSE(contents->IsReferencedFromResource());
  EXPECT_FALSE(css_resource->RestoreParsedStyleSheet(parser_context));
}

}  // namespace
}  // namespace blink
