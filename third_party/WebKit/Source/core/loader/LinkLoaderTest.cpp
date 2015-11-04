// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/loader/LinkLoader.h"

#include "core/fetch/ResourceFetcher.h"
#include "core/frame/Settings.h"
#include "core/html/LinkRelAttribute.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/LinkLoaderClient.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/network/ResourceLoadPriority.h"

#include <base/macros.h>
#include <gtest/gtest.h>

namespace blink {

class MockLinkLoaderClient : public LinkLoaderClient {
public:
    MockLinkLoaderClient(bool shouldLoad)
        : m_shouldLoad(shouldLoad)
    {
    }

    bool shouldLoadLink() override
    {
        return m_shouldLoad;
    }

    void linkLoaded() override {}
    void linkLoadingErrored() override {}
    void didStartLinkPrerender() override {}
    void didStopLinkPrerender() override {}
    void didSendLoadForLinkPrerender() override {}
    void didSendDOMContentLoadedForLinkPrerender() override {}

private:
    bool m_shouldLoad;
};

class NetworkHintsMock : public NetworkHintsInterface {
public:
    NetworkHintsMock()
        : m_didDnsPrefetch(false)
        , m_didPreconnect(false)
    {
    }

    void dnsPrefetchHost(const String& host) const override
    {
        m_didDnsPrefetch = true;
    }

    void preconnectHost(const KURL& host, const CrossOriginAttributeValue crossOrigin) const override
    {
        m_didPreconnect = true;
        m_isHTTPS = host.protocolIs("https");
        m_isCrossOrigin = (crossOrigin == CrossOriginAttributeAnonymous);
    }

    bool didDnsPrefetch() { return m_didDnsPrefetch; }
    bool didPreconnect() { return m_didPreconnect; }
    bool isHTTPS() { return m_isHTTPS; }
    bool isCrossOrigin() { return m_isCrossOrigin; }

private:
    mutable bool m_didDnsPrefetch;
    mutable bool m_didPreconnect;
    mutable bool m_isHTTPS;
    mutable bool m_isCrossOrigin;

};

TEST(LinkLoaderTest, Preload)
{
    struct TestCase {
        const char* href;
        const char* as;
        const ResourceLoadPriority priority;
        const bool shouldLoad;
    } cases[] = {
        {"data://example.test/cat.jpg", "image", ResourceLoadPriorityVeryLow, true},
        {"data://example.test/cat.js", "script", ResourceLoadPriorityMedium, true},
        {"data://example.test/cat.css", "stylesheet", ResourceLoadPriorityHigh, true},
        {"data://example.test/cat.blob", "blabla", ResourceLoadPriorityUnresolved, false},
    };

    // Test the cases with a single header
    for (const auto& testCase : cases) {
        OwnPtr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create(IntSize(500, 500));
        dummyPageHolder->frame().settings()->setScriptEnabled(true);
        MockLinkLoaderClient loaderClient(testCase.shouldLoad);
        LinkLoader loader(&loaderClient);
        KURL hrefURL = KURL(KURL(), testCase.href);
        loader.loadLink(LinkRelAttribute("preload"),
            AtomicString(),
            String(),
            testCase.as,
            hrefURL,
            dummyPageHolder->document(),
            NetworkHintsMock());
        ASSERT(dummyPageHolder->document().fetcher());
        WillBeHeapListHashSet<RawPtrWillBeMember<Resource>>* preloads = dummyPageHolder->document().fetcher()->preloads();
        if (testCase.shouldLoad)
            ASSERT_NE(nullptr, preloads);
        if (preloads) {
            if (testCase.priority == ResourceLoadPriorityUnresolved) {
                ASSERT_EQ((unsigned)0, preloads->size());
            } else {
                ASSERT_EQ((unsigned)1, preloads->size());
                if (preloads->size() > 0)
                    ASSERT_EQ(testCase.priority, preloads->begin().get()->get()->resourceRequest().priority());
            }
        }
    }
}

TEST(LinkLoaderTest, DNSPrefetch)
{
    struct {
        const char* href;
        const bool shouldLoad;
    } cases[] = {
        {"http://example.com/", true},
        {"https://example.com/", true},
        {"//example.com/", true},
    };

    // TODO(yoav): Test (and fix) shouldLoad = false

    // Test the cases with a single header
    for (const auto& testCase : cases) {
        OwnPtr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create(IntSize(500, 500));
        dummyPageHolder->document().settings()->setDNSPrefetchingEnabled(true);
        MockLinkLoaderClient loaderClient(testCase.shouldLoad);
        LinkLoader loader(&loaderClient);
        KURL hrefURL = KURL(KURL(ParsedURLStringTag(), String("http://example.com")), testCase.href);
        NetworkHintsMock networkHints;
        loader.loadLink(LinkRelAttribute("dns-prefetch"),
            AtomicString(),
            String(),
            String(),
            hrefURL,
            dummyPageHolder->document(),
            networkHints);
        ASSERT_FALSE(networkHints.didPreconnect());
        ASSERT_EQ(testCase.shouldLoad, networkHints.didDnsPrefetch());
    }
}

TEST(LinkLoaderTest, Preconnect)
{
    struct {
        const char* href;
        const char* crossOrigin;
        const bool shouldLoad;
        const bool isHTTPS;
        const bool isCrossOrigin;
    } cases[] = {
        {"http://example.com/", nullptr, true, false, false},
        {"https://example.com/", nullptr, true, true, false},
        {"http://example.com/", "anonymous", true, false, true},
        {"//example.com/", nullptr, true, false, false},
    };

    // Test the cases with a single header
    for (const auto& testCase : cases) {
        OwnPtr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create(IntSize(500, 500));
        MockLinkLoaderClient loaderClient(testCase.shouldLoad);
        LinkLoader loader(&loaderClient);
        KURL hrefURL = KURL(KURL(ParsedURLStringTag(), String("http://example.com")), testCase.href);
        NetworkHintsMock networkHints;
        loader.loadLink(LinkRelAttribute("preconnect"),
            testCase.crossOrigin,
            String(),
            String(),
            hrefURL,
            dummyPageHolder->document(),
            networkHints);
        ASSERT_EQ(testCase.shouldLoad, networkHints.didPreconnect());
        ASSERT_EQ(testCase.isHTTPS, networkHints.isHTTPS());
        ASSERT_EQ(testCase.isCrossOrigin, networkHints.isCrossOrigin());
    }
}

} // namespace blink
