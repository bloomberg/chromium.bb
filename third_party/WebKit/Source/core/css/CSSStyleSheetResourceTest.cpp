// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/CSSStyleSheetResource.h"

#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserSelector.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchContext.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "platform/network/ResourceRequest.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebUnitTestSupport.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;

namespace {

class CSSStyleSheetResourceTest : public ::testing::Test {
protected:
    CSSStyleSheetResourceTest()
    {
        m_originalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());
        m_page = DummyPageHolder::create();
        document()->setURL(KURL(KURL(), "https://localhost/"));
    }

    ~CSSStyleSheetResourceTest() override
    {
        replaceMemoryCacheForTesting(m_originalMemoryCache.release());
    }

    Document* document() { return &m_page->document(); }

    Persistent<MemoryCache> m_originalMemoryCache;
    OwnPtr<DummyPageHolder> m_page;
};

TEST_F(CSSStyleSheetResourceTest, PruneCanCauseEviction)
{
    {
        // We need this scope to remove all references.
        KURL imageURL(KURL(), "https://localhost/image");
        KURL cssURL(KURL(), "https://localhost/style.css");

        // We need to disable loading because we manually give a response to
        // the image resource.
        document()->fetcher()->setAutoLoadImages(false);

        ResourcePtr<CSSStyleSheetResource> cssResource = CSSStyleSheetResource::createForTest(ResourceRequest(cssURL), "utf-8");
        memoryCache()->add(cssResource.get());
        cssResource->responseReceived(ResourceResponse(cssURL, "style/css", 0, nullAtom, String()), nullptr);
        cssResource->finish();

        RefPtrWillBeRawPtr<StyleSheetContents> contents = StyleSheetContents::create(CSSParserContext(HTMLStandardMode, nullptr));
        RefPtrWillBeRawPtr<CSSStyleSheet> sheet = CSSStyleSheet::create(contents, document());
        EXPECT_TRUE(sheet);
        RefPtrWillBeRawPtr<CSSCrossfadeValue> crossfade = CSSCrossfadeValue::create(
            CSSImageValue::create(String("image"), imageURL),
            CSSImageValue::create(String("image"), imageURL),
            CSSPrimitiveValue::create(1.0, CSSPrimitiveValue::UnitType::Number));
        Vector<OwnPtr<CSSParserSelector>> selectors;
        selectors.append(adoptPtr(new CSSParserSelector()));
        selectors[0]->setMatch(CSSSelector::Id);
        selectors[0]->setValue("foo");
        CSSProperty property(CSSPropertyBackground, crossfade);
        contents->parserAppendRule(
            StyleRule::create(CSSSelectorList::adoptSelectorVector(selectors), ImmutableStylePropertySet::create(&property, 1, HTMLStandardMode)));

        crossfade->loadSubimages(document());
        ResourcePtr<Resource> imageResource = memoryCache()->resourceForURL(imageURL, MemoryCache::defaultCacheIdentifier());
        ASSERT_TRUE(imageResource);
        ResourceResponse imageResponse;
        imageResponse.setURL(imageURL);
        imageResponse.setHTTPHeaderField("cache-control", "no-store");
        imageResource->responseReceived(imageResponse, nullptr);

        contents->checkLoaded();
        cssResource->saveParsedStyleSheet(contents);

        memoryCache()->update(cssResource.get(), cssResource->size(), cssResource->size(), false);
        memoryCache()->update(imageResource.get(), imageResource->size(), imageResource->size(), false);
        if (!memoryCache()->isInSameLRUListForTest(cssResource.get(), imageResource.get())) {
            // We assume that the LRU list is determined by |size / accessCount|.
            for (size_t i = 0; i < cssResource->size() + 1; ++i)
                memoryCache()->update(cssResource.get(), cssResource->size(), cssResource->size(), true);
            for (size_t i = 0; i < imageResource->size() + 1; ++i)
                memoryCache()->update(imageResource.get(), imageResource->size(), imageResource->size(), true);
        }
        ASSERT_TRUE(memoryCache()->isInSameLRUListForTest(cssResource.get(), imageResource.get()));
    }
    Heap::collectAllGarbage();
    // This operation should not lead to crash!
    memoryCache()->pruneAll();
}

} // namespace
} // namespace blink
