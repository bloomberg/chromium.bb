/*
 * Copyright (c) 2015, Google Inc. All rights reserved.
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
#include "core/loader/FrameFetchContext.h"

#include "core/fetch/FetchInitiatorInfo.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include <gtest/gtest.h>

namespace blink {

class FrameFetchContextUpgradeTest : public ::testing::Test {
public:
    FrameFetchContextUpgradeTest()
        : secureURL(ParsedURLString, "https://secureorigin.test/image.png")
        , exampleOrigin(SecurityOrigin::create(KURL(ParsedURLString, "https://example.test/")))
        , secureOrigin(SecurityOrigin::create(secureURL))
    {
    }

protected:
    virtual void SetUp()
    {
        dummyPageHolder = DummyPageHolder::create(IntSize(500, 500));
        dummyPageHolder->page().setDeviceScaleFactor(1.0);
        documentLoader = DocumentLoader::create(&dummyPageHolder->frame(), ResourceRequest("http://www.example.com"), SubstituteData());
        document = toHTMLDocument(&dummyPageHolder->document());
        fetchContext = &static_cast<FrameFetchContext&>(documentLoader->fetcher()->context());
        fetchContext->setDocument(document.get());
    }

    void expectUpgrade(const char* input, const char* expected)
    {
        expectUpgrade(input, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeNone, expected);
    }

    void expectUpgrade(const char* input, WebURLRequest::RequestContext requestContext, WebURLRequest::FrameType frameType, const char* expected)
    {
        KURL inputURL(ParsedURLString, input);
        KURL expectedURL(ParsedURLString, expected);

        FetchRequest fetchRequest = FetchRequest(ResourceRequest(inputURL), FetchInitiatorInfo());
        fetchRequest.mutableResourceRequest().setRequestContext(requestContext);
        fetchRequest.mutableResourceRequest().setFrameType(frameType);

        fetchContext->upgradeInsecureRequest(fetchRequest);

        EXPECT_STREQ(expectedURL.string().utf8().data(), fetchRequest.resourceRequest().url().string().utf8().data());
        EXPECT_EQ(expectedURL.protocol(), fetchRequest.resourceRequest().url().protocol());
        EXPECT_EQ(expectedURL.host(), fetchRequest.resourceRequest().url().host());
        EXPECT_EQ(expectedURL.port(), fetchRequest.resourceRequest().url().port());
        EXPECT_EQ(expectedURL.hasPort(), fetchRequest.resourceRequest().url().hasPort());
        EXPECT_EQ(expectedURL.path(), fetchRequest.resourceRequest().url().path());

        bool expectUpgrade = inputURL != expectedURL;

        EXPECT_STREQ(expectUpgrade ? "1" : "",
            fetchRequest.resourceRequest().httpHeaderField("Upgraded").utf8().data());
    }

    void expectPreferHeader(const char* input, WebURLRequest::FrameType frameType, bool shouldPrefer)
    {
        KURL inputURL(ParsedURLString, input);

        FetchRequest fetchRequest = FetchRequest(ResourceRequest(inputURL), FetchInitiatorInfo());
        fetchRequest.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextScript);
        fetchRequest.mutableResourceRequest().setFrameType(frameType);

        fetchContext->upgradeInsecureRequest(fetchRequest);

        EXPECT_STREQ(shouldPrefer ? "tls" : "",
            fetchRequest.resourceRequest().httpHeaderField("Prefer").utf8().data());
    }

    KURL secureURL;
    RefPtr<SecurityOrigin> exampleOrigin;
    RefPtr<SecurityOrigin> secureOrigin;

    OwnPtr<DummyPageHolder> dummyPageHolder;
    // We don't use the DocumentLoader directly in any tests, but need to keep it around as long
    // as the ResourceFetcher and Document live due to indirect usage.
    RefPtr<DocumentLoader> documentLoader;
    RefPtrWillBePersistent<Document> document;
    FrameFetchContext* fetchContext;
};

TEST_F(FrameFetchContextUpgradeTest, UpgradeInsecureResourceRequests)
{
    struct TestCase {
        const char* original;
        const char* upgraded;
    } tests[] = {
        { "http://example.test/image.png", "https://example.test/image.png" },
        { "http://example.test:80/image.png", "https://example.test:443/image.png" },
        { "http://example.test:1212/image.png", "https://example.test:1212/image.png" },

        { "https://example.test/image.png", "https://example.test/image.png" },
        { "https://example.test:80/image.png", "https://example.test:80/image.png" },
        { "https://example.test:1212/image.png", "https://example.test:1212/image.png" },

        { "ftp://example.test/image.png", "ftp://example.test/image.png" },
        { "ftp://example.test:21/image.png", "ftp://example.test:21/image.png" },
        { "ftp://example.test:1212/image.png", "ftp://example.test:1212/image.png" },
    };

    document->setInsecureRequestsPolicy(SecurityContext::InsecureRequestsUpgrade);

    for (auto test : tests) {
        // secureOrigin's host is 'secureorigin.test', not 'example.test'
        document->setSecurityOrigin(secureOrigin);

        // We always upgrade for FrameTypeNone and FrameTypeNested.
        expectUpgrade(test.original, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeNone, test.upgraded);
        expectUpgrade(test.original, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeNested, test.upgraded);

        // We do not upgrade for FrameTypeTopLevel or FrameTypeAuxiliary...
        expectUpgrade(test.original, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeTopLevel, test.original);
        expectUpgrade(test.original, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeAuxiliary, test.original);

        // unless the request context is RequestContextForm.
        expectUpgrade(test.original, WebURLRequest::RequestContextForm, WebURLRequest::FrameTypeTopLevel, test.upgraded);
        expectUpgrade(test.original, WebURLRequest::RequestContextForm, WebURLRequest::FrameTypeAuxiliary, test.upgraded);

        // Or unless the host of the document matches the host of the resource:
        document->setSecurityOrigin(exampleOrigin);
        expectUpgrade(test.original, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeTopLevel, test.upgraded);
        expectUpgrade(test.original, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeAuxiliary, test.upgraded);
    }
}

TEST_F(FrameFetchContextUpgradeTest, DoNotUpgradeInsecureResourceRequests)
{
    document->setSecurityOrigin(secureOrigin);
    document->setInsecureRequestsPolicy(SecurityContext::InsecureRequestsDoNotUpgrade);

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

TEST_F(FrameFetchContextUpgradeTest, SendPreferHeader)
{
    struct TestCase {
        const char* toRequest;
        WebURLRequest::FrameType frameType;
        bool shouldPrefer;
    } tests[] = {
        { "http://example.test/page.html", WebURLRequest::FrameTypeAuxiliary, true },
        { "http://example.test/page.html", WebURLRequest::FrameTypeNested, true },
        { "http://example.test/page.html", WebURLRequest::FrameTypeNone, false },
        { "http://example.test/page.html", WebURLRequest::FrameTypeTopLevel, true },
        { "https://example.test/page.html", WebURLRequest::FrameTypeAuxiliary, false },
        { "https://example.test/page.html", WebURLRequest::FrameTypeNested, false },
        { "https://example.test/page.html", WebURLRequest::FrameTypeNone, false },
        { "https://example.test/page.html", WebURLRequest::FrameTypeTopLevel, false }
    };

    for (auto test : tests) {
        document->setInsecureRequestsPolicy(SecurityContext::InsecureRequestsDoNotUpgrade);
        expectPreferHeader(test.toRequest, test.frameType, test.shouldPrefer);

        document->setInsecureRequestsPolicy(SecurityContext::InsecureRequestsUpgrade);
        expectPreferHeader(test.toRequest, test.frameType, test.shouldPrefer);
    }
}

class FrameFetchContextHintsTest : public ::testing::Test {
public:
    FrameFetchContextHintsTest() { }

protected:
    virtual void SetUp()
    {
        dummyPageHolder = DummyPageHolder::create(IntSize(500, 500));
        dummyPageHolder->page().setDeviceScaleFactor(1.0);
        documentLoader = DocumentLoader::create(&dummyPageHolder->frame(), ResourceRequest("http://www.example.com"), SubstituteData());
        document = toHTMLDocument(&dummyPageHolder->document());
        fetchContext = &static_cast<FrameFetchContext&>(documentLoader->fetcher()->context());
        fetchContext->setDocument(document.get());
    }

    void expectHeader(const char* input, const char* headerName, bool isPresent, const char* headerValue)
    {
        KURL inputURL(ParsedURLString, input);
        FetchRequest fetchRequest = FetchRequest(ResourceRequest(inputURL), FetchInitiatorInfo());
        fetchContext->addClientHintsIfNecessary(fetchRequest);

        EXPECT_STREQ(isPresent ? headerValue : "",
            fetchRequest.resourceRequest().httpHeaderField(headerName).utf8().data());
    }

    OwnPtr<DummyPageHolder> dummyPageHolder;
    // We don't use the DocumentLoader directly in any tests, but need to keep it around as long
    // as the ResourceFetcher and Document live due to indirect usage.
    RefPtr<DocumentLoader> documentLoader;
    RefPtrWillBePersistent<Document> document;
    FrameFetchContext* fetchContext;
};

TEST_F(FrameFetchContextHintsTest, MonitorDPRHints)
{
    expectHeader("http://www.example.com/1.gif", "DPR", false, "");
    dummyPageHolder->frame().setShouldSendDPRHint();
    expectHeader("http://www.example.com/1.gif", "DPR", true, "1");
    dummyPageHolder->page().setDeviceScaleFactor(2.5);
    expectHeader("http://www.example.com/1.gif", "DPR", true, "2.5");
    expectHeader("http://www.example.com/1.gif", "RW", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorRWHints)
{
    expectHeader("http://www.example.com/1.gif", "RW", false, "");
    dummyPageHolder->frame().setShouldSendRWHint();
    expectHeader("http://www.example.com/1.gif", "RW", true, "500");
    dummyPageHolder->frameView().setLayoutSizeFixedToFrameSize(false);
    dummyPageHolder->frameView().setLayoutSize(IntSize(800, 800));
    expectHeader("http://www.example.com/1.gif", "RW", true, "800");
    expectHeader("http://www.example.com/1.gif", "DPR", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorBothHints)
{
    expectHeader("http://www.example.com/1.gif", "DPR", false, "");
    expectHeader("http://www.example.com/1.gif", "RW", false, "");

    dummyPageHolder->frame().setShouldSendDPRHint();
    dummyPageHolder->frame().setShouldSendRWHint();
    expectHeader("http://www.example.com/1.gif", "DPR", true, "1");
    expectHeader("http://www.example.com/1.gif", "RW", true, "500");
}

} // namespace

