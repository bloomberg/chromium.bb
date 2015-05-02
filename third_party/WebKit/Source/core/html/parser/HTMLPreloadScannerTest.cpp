// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/parser/HTMLPreloadScanner.h"

#include "core/MediaTypeNames.h"
#include "core/css/MediaValuesCached.h"
#include "core/html/parser/HTMLParserOptions.h"
#include "core/html/parser/HTMLResourcePreloader.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>

namespace blink {

typedef struct {
    const char* baseURL;
    const char* inputHTML;
    const char* preloadedURL;
    const char* outputBaseURL;
    Resource::Type type;
    int resourceWidth;
} TestCase;

class MockHTMLResourcePreloader : public ResourcePreloader {
public:
    void preloadRequestVerification(Resource::Type type, const String& url, const String& baseURL, int width)
    {
        EXPECT_EQ(m_preloadRequest->resourceType(), type);
        EXPECT_STREQ(m_preloadRequest->resourceURL().ascii().data(),  url.ascii().data());
        EXPECT_STREQ(m_preloadRequest->baseURL().ascii().data(), baseURL.ascii().data());
        EXPECT_EQ(m_preloadRequest->resourceWidth(), width);
    }

protected:
    virtual void preload(PassOwnPtr<PreloadRequest> preloadRequest)
    {
        m_preloadRequest = preloadRequest;
    }

private:

    OwnPtr<PreloadRequest> m_preloadRequest;
};

class HTMLPreloadScannerTest : public testing::Test {
protected:
    HTMLPreloadScannerTest()
        : m_dummyPageHolder(DummyPageHolder::create())
    {
    }

    PassRefPtr<MediaValues> createMediaValues()
    {
        MediaValuesCached::MediaValuesCachedData data;
        data.viewportWidth = 500;
        data.viewportHeight = 600;
        data.deviceWidth = 500;
        data.deviceHeight = 500;
        data.devicePixelRatio = 2.0;
        data.colorBitsPerComponent = 24;
        data.monochromeBitsPerComponent = 0;
        data.primaryPointerType = PointerTypeFine;
        data.defaultFontSize = 16;
        data.threeDEnabled = true;
        data.mediaType = MediaTypeNames::screen;
        data.strictMode = true;
        data.displayMode = WebDisplayModeBrowser;
        return MediaValuesCached::create(data);
    }

    virtual void SetUp()
    {
        HTMLParserOptions options(&m_dummyPageHolder->document());
        KURL documentURL(ParsedURLString, "http://whatever.test/");
        m_scanner = HTMLPreloadScanner::create(options, documentURL, CachedDocumentParameters::create(&m_dummyPageHolder->document(), createMediaValues()));

    }

    void test(TestCase testCase)
    {
        MockHTMLResourcePreloader preloader;
        KURL baseURL(ParsedURLString, testCase.baseURL);
        m_scanner->appendToEnd(String(testCase.inputHTML));
        m_scanner->scan(&preloader, baseURL);

        preloader.preloadRequestVerification(testCase.type, testCase.preloadedURL, testCase.outputBaseURL, testCase.resourceWidth);
    }

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
    OwnPtr<HTMLPreloadScanner> m_scanner;
};

TEST_F(HTMLPreloadScannerTest, testImages)
{
    TestCase testCases[] = {
        {"http://example.test", "<img src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<meta name=viewport content='width=160'><img srcset='bla.gif 320w, blabla.gif 640w'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {0, 0, 0, 0, Resource::Raw, 0} // Do not remove the terminator line.
    };

    for (unsigned i = 0; testCases[i].inputHTML; ++i) {
        TestCase testCase = testCases[i];
        test(testCase);
    }
}

}
