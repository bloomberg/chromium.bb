/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "config.h"
#include "core/fetch/ResourceFetcher.h"

#include <gtest/gtest.h>
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourcePtr.h"
#include "core/html/HTMLDocument.h"
#include "core/loader/DocumentLoader.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

class ResourceFetcherTest : public ::testing::Test {
public:
    ResourceFetcherTest()
        : secureURL(ParsedURLString, "https://example.test/image.png")
        , secureOrigin(SecurityOrigin::create(secureURL))
    {
    }

protected:
    virtual void SetUp()
    {
        // Create a ResourceFetcher that has a real DocumentLoader and Document, but is not attached to a LocalFrame.
        // Technically, we're concerned about what happens after a LocalFrame is detached (rather than before
        // any attach occurs), but ResourceFetcher can't tell the difference.
        documentLoader = DocumentLoader::create(0, ResourceRequest(secureURL), SubstituteData());
        document = Document::create();
        fetcher = documentLoader->fetcher();
        fetcher->setDocument(document.get());
    }

    void expectUpgrade(const char* input, const char* expected)
    {
        KURL inputURL(ParsedURLString, input);
        KURL expectedURL(ParsedURLString, expected);

        FetchRequest fetchRequest = FetchRequest(ResourceRequest(inputURL), FetchInitiatorInfo());
        fetcher->maybeUpgradeInsecureRequestURL(fetchRequest);
        EXPECT_STREQ(expectedURL.string().utf8().data(), fetchRequest.resourceRequest().url().string().utf8().data());
        EXPECT_EQ(expectedURL.protocol(), fetchRequest.resourceRequest().url().protocol());
        EXPECT_EQ(expectedURL.host(), fetchRequest.resourceRequest().url().host());
        EXPECT_EQ(expectedURL.port(), fetchRequest.resourceRequest().url().port());
        EXPECT_EQ(expectedURL.hasPort(), fetchRequest.resourceRequest().url().hasPort());
        EXPECT_EQ(expectedURL.path(), fetchRequest.resourceRequest().url().path());
    }

    KURL secureURL;
    RefPtr<SecurityOrigin> secureOrigin;

    // We don't use the DocumentLoader directly in any tests, but need to keep it around as long
    // as the ResourceFetcher and Document live due to indirect usage.
    RefPtr<DocumentLoader> documentLoader;
    RefPtrWillBePersistent<ResourceFetcher> fetcher;
    RefPtrWillBePersistent<Document> document;
};

TEST_F(ResourceFetcherTest, StartLoadAfterFrameDetach)
{
    EXPECT_EQ(fetcher->frame(), static_cast<LocalFrame*>(0));

    // Try to request a url. The request should fail, no resource should be returned,
    // and no resource should be present in the cache.
    FetchRequest fetchRequest = FetchRequest(ResourceRequest(secureURL), FetchInitiatorInfo());
    ResourcePtr<ImageResource> image = fetcher->fetchImage(fetchRequest);
    EXPECT_EQ(image.get(), static_cast<ImageResource*>(0));
    EXPECT_EQ(memoryCache()->resourceForURL(secureURL), static_cast<Resource*>(0));
}

TEST_F(ResourceFetcherTest, UpgradeInsecureResourceRequests)
{
    document->setSecurityOrigin(secureOrigin);
    document->setInsecureContentPolicy(SecurityContext::InsecureContentUpgrade);

    expectUpgrade("http://example.test/image.png", "https://example.test/image.png");
    expectUpgrade("http://example.test:80/image.png", "https://example.test:443/image.png");
    expectUpgrade("http://example.test:1212/image.png", "https://example.test:1212/image.png");

    expectUpgrade("https://example.test/image.png", "https://example.test/image.png");
    expectUpgrade("https://example.test:80/image.png", "https://example.test:80/image.png");
    expectUpgrade("https://example.test:1212/image.png", "https://example.test:1212/image.png");

    expectUpgrade("ftp://example.test/image.png", "ftp://example.test/image.png");
    expectUpgrade("ftp://example.test:21/image.png", "ftp://example.test:21/image.png");
    expectUpgrade("ftp://example.test:1212/image.png", "ftp://example.test:1212/image.png");
}

TEST_F(ResourceFetcherTest, DoNotUpgradeInsecureResourceRequests)
{
    document->setSecurityOrigin(secureOrigin);
    document->setInsecureContentPolicy(SecurityContext::InsecureContentDoNotUpgrade);

    expectUpgrade("http://example.test/image.png", "http://example.test/image.png");
    expectUpgrade("http://example.test:80/image.png", "http://example.test:80/image.png");
    expectUpgrade("http://example.test:1212/image.png", "http://example.test:1212/image.png");

    expectUpgrade("https://example.test/image.png", "https://example.test/image.png");
    expectUpgrade("https://example.test:80/image.png", "https://example.test:80/image.png");
    expectUpgrade("https://example.test:1212/image.png", "https://example.test:1212/image.png");

    expectUpgrade("ftp://example.test/image.png", "ftp://example.test/image.png");
    expectUpgrade("ftp://example.test:21/image.png", "ftp://example.test:21/image.png");
    expectUpgrade("ftp://example.test:1212/image.png", "ftp://example.test:1212/image.png");
}

TEST_F(ResourceFetcherTest, MonitorInsecureResourceRequests)
{
    document->setSecurityOrigin(secureOrigin);
    document->setInsecureContentPolicy(SecurityContext::InsecureContentMonitor);

    expectUpgrade("http://example.test/image.png", "http://example.test/image.png");
    expectUpgrade("http://example.test:80/image.png", "http://example.test:80/image.png");
    expectUpgrade("http://example.test:1212/image.png", "http://example.test:1212/image.png");

    expectUpgrade("https://example.test/image.png", "https://example.test/image.png");
    expectUpgrade("https://example.test:80/image.png", "https://example.test:80/image.png");
    expectUpgrade("https://example.test:1212/image.png", "https://example.test:1212/image.png");

    expectUpgrade("ftp://example.test/image.png", "ftp://example.test/image.png");
    expectUpgrade("ftp://example.test:21/image.png", "ftp://example.test:21/image.png");
    expectUpgrade("ftp://example.test:1212/image.png", "ftp://example.test:1212/image.png");
}

} // namespace
