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

#include "core/loader/FrameFetchContext.h"

#include "core/fetch/FetchInitiatorInfo.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameOwner.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLDocument.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/EmptyClients.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class StubFrameLoaderClientWithParent final : public EmptyFrameLoaderClient {
public:
    static PassOwnPtrWillBeRawPtr<StubFrameLoaderClientWithParent> create(Frame* parent)
    {
        return adoptPtrWillBeNoop(new StubFrameLoaderClientWithParent(parent));
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_parent);
        EmptyFrameLoaderClient::trace(visitor);
    }

    Frame* parent() const override { return m_parent.get(); }

private:
    explicit StubFrameLoaderClientWithParent(Frame* parent)
        : m_parent(parent)
    {
    }

    RawPtrWillBeMember<Frame> m_parent;
};

class StubFrameOwner : public NoBaseWillBeGarbageCollectedFinalized<StubFrameOwner>, public FrameOwner {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(StubFrameOwner);
public:
    static PassOwnPtrWillBeRawPtr<StubFrameOwner> create()
    {
        return adoptPtrWillBeNoop(new StubFrameOwner);
    }

    DEFINE_INLINE_VIRTUAL_TRACE() { FrameOwner::trace(visitor); }

    bool isLocal() const override { return false; }
    SandboxFlags sandboxFlags() const override { return SandboxNone; }
    void dispatchLoad() override { }
    void renderFallbackContent() override { }
    ScrollbarMode scrollingMode() const override { return ScrollbarAuto; }
    int marginWidth() const override { return -1; }
    int marginHeight() const override { return -1; }
};

class FrameFetchContextTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        dummyPageHolder = DummyPageHolder::create(IntSize(500, 500));
        dummyPageHolder->page().setDeviceScaleFactor(1.0);
        documentLoader = DocumentLoader::create(&dummyPageHolder->frame(), ResourceRequest("http://www.example.com"), SubstituteData());
        document = toHTMLDocument(&dummyPageHolder->document());
        fetchContext = static_cast<FrameFetchContext*>(&documentLoader->fetcher()->context());
        owner = StubFrameOwner::create();
        FrameFetchContext::provideDocumentToContext(*fetchContext, document.get());
    }

    void TearDown() override
    {
        documentLoader->detachFromFrame();
        documentLoader.clear();

        if (childFrame) {
            childDocumentLoader->detachFromFrame();
            childDocumentLoader.clear();
            childFrame->detach(FrameDetachType::Remove);
        }
    }

    FrameFetchContext* createChildFrame()
    {
        childClient = StubFrameLoaderClientWithParent::create(document->frame());
        childFrame = LocalFrame::create(childClient.get(), document->frame()->host(), owner.get());
        childFrame->setView(FrameView::create(childFrame.get(), IntSize(500, 500)));
        childFrame->init();
        childDocumentLoader = DocumentLoader::create(childFrame.get(), ResourceRequest("http://www.example.com"), SubstituteData());
        childDocument = childFrame->document();
        FrameFetchContext* childFetchContext = static_cast<FrameFetchContext*>(&childDocumentLoader->fetcher()->context());
        FrameFetchContext::provideDocumentToContext(*childFetchContext, childDocument.get());
        return childFetchContext;
    }

    OwnPtr<DummyPageHolder> dummyPageHolder;
    // We don't use the DocumentLoader directly in any tests, but need to keep it around as long
    // as the ResourceFetcher and Document live due to indirect usage.
    RefPtrWillBePersistent<DocumentLoader> documentLoader;
    RefPtrWillBePersistent<Document> document;
    Persistent<FrameFetchContext> fetchContext;

    OwnPtrWillBePersistent<StubFrameLoaderClientWithParent> childClient;
    RefPtrWillBePersistent<LocalFrame> childFrame;
    RefPtrWillBePersistent<DocumentLoader> childDocumentLoader;
    RefPtrWillBePersistent<Document> childDocument;
    OwnPtrWillBePersistent<StubFrameOwner> owner;
};

class FrameFetchContextUpgradeTest : public FrameFetchContextTest {
public:
    FrameFetchContextUpgradeTest()
        : exampleOrigin(SecurityOrigin::create(KURL(ParsedURLString, "https://example.test/")))
        , secureOrigin(SecurityOrigin::create(KURL(ParsedURLString, "https://secureorigin.test/image.png")))
    {
    }

protected:
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
    }

    void expectHTTPSHeader(const char* input, WebURLRequest::FrameType frameType, bool shouldPrefer)
    {
        KURL inputURL(ParsedURLString, input);

        FetchRequest fetchRequest = FetchRequest(ResourceRequest(inputURL), FetchInitiatorInfo());
        fetchRequest.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextScript);
        fetchRequest.mutableResourceRequest().setFrameType(frameType);

        fetchContext->upgradeInsecureRequest(fetchRequest);

        EXPECT_STREQ(shouldPrefer ? "1" : "",
            fetchRequest.resourceRequest().httpHeaderField(HTTPNames::Upgrade_Insecure_Requests).utf8().data());
    }

    RefPtr<SecurityOrigin> exampleOrigin;
    RefPtr<SecurityOrigin> secureOrigin;
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

    FrameFetchContext::provideDocumentToContext(*fetchContext, document.get());
    document->setInsecureRequestsPolicy(SecurityContext::InsecureRequestsUpgrade);

    for (const auto& test : tests) {
        document->insecureNavigationsToUpgrade()->clear();

        // We always upgrade for FrameTypeNone and FrameTypeNested.
        expectUpgrade(test.original, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeNone, test.upgraded);
        expectUpgrade(test.original, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeNested, test.upgraded);

        // We do not upgrade for FrameTypeTopLevel or FrameTypeAuxiliary...
        expectUpgrade(test.original, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeTopLevel, test.original);
        expectUpgrade(test.original, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeAuxiliary, test.original);

        // unless the request context is RequestContextForm.
        expectUpgrade(test.original, WebURLRequest::RequestContextForm, WebURLRequest::FrameTypeTopLevel, test.upgraded);
        expectUpgrade(test.original, WebURLRequest::RequestContextForm, WebURLRequest::FrameTypeAuxiliary, test.upgraded);

        // Or unless the host of the resource is in the document's InsecureNavigationsSet:
        document->addInsecureNavigationUpgrade(exampleOrigin->host().impl()->hash());
        expectUpgrade(test.original, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeTopLevel, test.upgraded);
        expectUpgrade(test.original, WebURLRequest::RequestContextScript, WebURLRequest::FrameTypeAuxiliary, test.upgraded);
    }
}

TEST_F(FrameFetchContextUpgradeTest, DoNotUpgradeInsecureResourceRequests)
{
    FrameFetchContext::provideDocumentToContext(*fetchContext, document.get());
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

TEST_F(FrameFetchContextUpgradeTest, SendHTTPSHeader)
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
        { "https://example.test/page.html", WebURLRequest::FrameTypeAuxiliary, true },
        { "https://example.test/page.html", WebURLRequest::FrameTypeNested, true },
        { "https://example.test/page.html", WebURLRequest::FrameTypeNone, false },
        { "https://example.test/page.html", WebURLRequest::FrameTypeTopLevel, true }
    };

    // This should work correctly both when the FrameFetchContext has a Document, and
    // when it doesn't (e.g. during main frame navigations), so run through the tests
    // both before and after providing a document to the context.
    for (const auto& test : tests) {
        document->setInsecureRequestsPolicy(SecurityContext::InsecureRequestsDoNotUpgrade);
        expectHTTPSHeader(test.toRequest, test.frameType, test.shouldPrefer);

        document->setInsecureRequestsPolicy(SecurityContext::InsecureRequestsUpgrade);
        expectHTTPSHeader(test.toRequest, test.frameType, test.shouldPrefer);
    }

    FrameFetchContext::provideDocumentToContext(*fetchContext, document.get());

    for (const auto& test : tests) {
        document->setInsecureRequestsPolicy(SecurityContext::InsecureRequestsDoNotUpgrade);
        expectHTTPSHeader(test.toRequest, test.frameType, test.shouldPrefer);

        document->setInsecureRequestsPolicy(SecurityContext::InsecureRequestsUpgrade);
        expectHTTPSHeader(test.toRequest, test.frameType, test.shouldPrefer);
    }
}

class FrameFetchContextHintsTest : public FrameFetchContextTest {
public:
    FrameFetchContextHintsTest() { }

protected:
    void expectHeader(const char* input, const char* headerName, bool isPresent, const char* headerValue, float width = 0)
    {
        KURL inputURL(ParsedURLString, input);
        FetchRequest fetchRequest = FetchRequest(ResourceRequest(inputURL), FetchInitiatorInfo());
        if (width > 0) {
            FetchRequest::ResourceWidth resourceWidth;
            resourceWidth.width = width;
            resourceWidth.isSet = true;
            fetchRequest.setResourceWidth(resourceWidth);
        }
        fetchContext->addClientHintsIfNecessary(fetchRequest);

        EXPECT_STREQ(isPresent ? headerValue : "",
            fetchRequest.resourceRequest().httpHeaderField(headerName).utf8().data());
    }
};

TEST_F(FrameFetchContextHintsTest, MonitorDPRHints)
{
    expectHeader("http://www.example.com/1.gif", "DPR", false, "");
    ClientHintsPreferences preferences;
    preferences.setShouldSendDPR(true);
    document->clientHintsPreferences().updateFrom(preferences);
    expectHeader("http://www.example.com/1.gif", "DPR", true, "1");
    dummyPageHolder->page().setDeviceScaleFactor(2.5);
    expectHeader("http://www.example.com/1.gif", "DPR", true, "2.5");
    expectHeader("http://www.example.com/1.gif", "Width", false, "");
    expectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorResourceWidthHints)
{
    expectHeader("http://www.example.com/1.gif", "Width", false, "");
    ClientHintsPreferences preferences;
    preferences.setShouldSendResourceWidth(true);
    document->clientHintsPreferences().updateFrom(preferences);
    expectHeader("http://www.example.com/1.gif", "Width", true, "500", 500);
    expectHeader("http://www.example.com/1.gif", "Width", true, "667", 666.6666);
    expectHeader("http://www.example.com/1.gif", "DPR", false, "");
    dummyPageHolder->page().setDeviceScaleFactor(2.5);
    expectHeader("http://www.example.com/1.gif", "Width", true, "1250", 500);
    expectHeader("http://www.example.com/1.gif", "Width", true, "1667", 666.6666);
}

TEST_F(FrameFetchContextHintsTest, MonitorViewportWidthHints)
{
    expectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
    ClientHintsPreferences preferences;
    preferences.setShouldSendViewportWidth(true);
    document->clientHintsPreferences().updateFrom(preferences);
    expectHeader("http://www.example.com/1.gif", "Viewport-Width", true, "500");
    dummyPageHolder->frameView().setLayoutSizeFixedToFrameSize(false);
    dummyPageHolder->frameView().setLayoutSize(IntSize(800, 800));
    expectHeader("http://www.example.com/1.gif", "Viewport-Width", true, "800");
    expectHeader("http://www.example.com/1.gif", "Viewport-Width", true, "800", 666.6666);
    expectHeader("http://www.example.com/1.gif", "DPR", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorAllHints)
{
    expectHeader("http://www.example.com/1.gif", "DPR", false, "");
    expectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
    expectHeader("http://www.example.com/1.gif", "Width", false, "");

    ClientHintsPreferences preferences;
    preferences.setShouldSendDPR(true);
    preferences.setShouldSendResourceWidth(true);
    preferences.setShouldSendViewportWidth(true);
    document->clientHintsPreferences().updateFrom(preferences);
    expectHeader("http://www.example.com/1.gif", "DPR", true, "1");
    expectHeader("http://www.example.com/1.gif", "Width", true, "400", 400);
    expectHeader("http://www.example.com/1.gif", "Viewport-Width", true, "500");
}

TEST_F(FrameFetchContextTest, MainResource)
{
    // Default case
    ResourceRequest request("http://www.example.com");
    EXPECT_EQ(UseProtocolCachePolicy, fetchContext->resourceRequestCachePolicy(request, Resource::MainResource));

    // Post
    ResourceRequest postRequest("http://www.example.com");
    postRequest.setHTTPMethod("POST");
    EXPECT_EQ(ReloadIgnoringCacheData, fetchContext->resourceRequestCachePolicy(postRequest, Resource::MainResource));

    // Re-post
    document->frame()->loader().setLoadType(FrameLoadTypeBackForward);
    EXPECT_EQ(ReturnCacheDataDontLoad, fetchContext->resourceRequestCachePolicy(postRequest, Resource::MainResource));

    // Enconding overriden
    document->frame()->loader().setLoadType(FrameLoadTypeStandard);
    document->frame()->host()->setOverrideEncoding("foo");
    EXPECT_EQ(ReturnCacheDataElseLoad, fetchContext->resourceRequestCachePolicy(request, Resource::MainResource));
    document->frame()->host()->setOverrideEncoding(AtomicString());

    // FrameLoadTypeSame
    document->frame()->loader().setLoadType(FrameLoadTypeSame);
    EXPECT_EQ(ReloadIgnoringCacheData, fetchContext->resourceRequestCachePolicy(request, Resource::MainResource));

    // Conditional request
    document->frame()->loader().setLoadType(FrameLoadTypeStandard);
    ResourceRequest conditional("http://www.example.com");
    conditional.setHTTPHeaderField(HTTPNames::If_Modified_Since, "foo");
    EXPECT_EQ(ReloadIgnoringCacheData, fetchContext->resourceRequestCachePolicy(conditional, Resource::MainResource));

    // Set up a child frame
    FrameFetchContext* childFetchContext = createChildFrame();

    // Child frame as part of back/forward
    document->frame()->loader().setLoadType(FrameLoadTypeBackForward);
    EXPECT_EQ(ReturnCacheDataElseLoad, childFetchContext->resourceRequestCachePolicy(request, Resource::MainResource));

    // Child frame as part of reload
    document->frame()->loader().setLoadType(FrameLoadTypeReload);
    EXPECT_EQ(ReloadIgnoringCacheData, childFetchContext->resourceRequestCachePolicy(request, Resource::MainResource));

    // Child frame as part of end to end reload
    document->frame()->loader().setLoadType(FrameLoadTypeReloadFromOrigin);
    EXPECT_EQ(ReloadBypassingCache, childFetchContext->resourceRequestCachePolicy(request, Resource::MainResource));
}

TEST_F(FrameFetchContextTest, ModifyPriorityForExperiments)
{
    Settings* settings = document->frame()->settings();
    FetchRequest request(ResourceRequest("http://www.example.com"), FetchInitiatorInfo());

    FetchRequest preloadRequest(ResourceRequest("http://www.example.com"), FetchInitiatorInfo());
    preloadRequest.setForPreload(true);

    FetchRequest deferredRequest(ResourceRequest("http://www.example.com"), FetchInitiatorInfo());
    deferredRequest.setDefer(FetchRequest::LazyLoad);

    // Start with all experiments disabled.
    settings->setFEtchIncreaseAsyncScriptPriority(false);
    settings->setFEtchIncreaseFontPriority(false);
    settings->setFEtchDeferLateScripts(false);
    settings->setFEtchIncreasePriorities(false);

    // Base case, no priority change. Note that this triggers m_imageFetched, which will matter for setFetchDeferLateScripts() case below.
    EXPECT_EQ(ResourceLoadPriorityVeryLow, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityVeryLow, Resource::Image, request, ResourcePriority::NotVisible));

    // Image visibility should increase priority
    EXPECT_EQ(ResourceLoadPriorityLow, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityVeryLow, Resource::Image, request, ResourcePriority::Visible));

    // Font priority with and without fetchIncreaseFontPriority()
    EXPECT_EQ(ResourceLoadPriorityMedium, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Font, request, ResourcePriority::NotVisible));
    settings->setFEtchIncreaseFontPriority(true);
    EXPECT_EQ(ResourceLoadPriorityHigh, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Font, request, ResourcePriority::NotVisible));

    // Basic script cases
    EXPECT_EQ(ResourceLoadPriorityMedium, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Script, request, ResourcePriority::NotVisible));
    EXPECT_EQ(ResourceLoadPriorityMedium, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Script, preloadRequest, ResourcePriority::NotVisible));

    // Enable deferring late scripts. Preload priority should drop.
    settings->setFEtchDeferLateScripts(true);
    EXPECT_EQ(ResourceLoadPriorityLow, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Script, preloadRequest, ResourcePriority::NotVisible));

    // Enable increasing priority of async scripts.
    EXPECT_EQ(ResourceLoadPriorityLow, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Script, deferredRequest, ResourcePriority::NotVisible));
    settings->setFEtchIncreaseAsyncScriptPriority(true);
    EXPECT_EQ(ResourceLoadPriorityMedium, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Script, deferredRequest, ResourcePriority::NotVisible));

    // Enable increased priorities for the remainder.
    settings->setFEtchIncreasePriorities(true);

    // Re-test image priority based on visibility with increased priorities
    EXPECT_EQ(ResourceLoadPriorityLow, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityVeryLow, Resource::Image, request, ResourcePriority::NotVisible));
    EXPECT_EQ(ResourceLoadPriorityHigh, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityVeryLow, Resource::Image, request, ResourcePriority::Visible));

    // Re-test font priority with increased prioriries
    settings->setFEtchIncreaseFontPriority(false);
    EXPECT_EQ(ResourceLoadPriorityHigh, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Font, request, ResourcePriority::NotVisible));
    settings->setFEtchIncreaseFontPriority(true);
    EXPECT_EQ(ResourceLoadPriorityVeryHigh, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Font, request, ResourcePriority::NotVisible));

    // Re-test basic script cases and deferring late script case with increased prioriries
    settings->setFEtchDeferLateScripts(false);
    EXPECT_EQ(ResourceLoadPriorityVeryHigh, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Script, request, ResourcePriority::NotVisible));
    EXPECT_EQ(ResourceLoadPriorityHigh, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Script, preloadRequest, ResourcePriority::NotVisible));

    // Re-test deferring late scripts.
    settings->setFEtchDeferLateScripts(true);
    EXPECT_EQ(ResourceLoadPriorityMedium, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Script, preloadRequest, ResourcePriority::NotVisible));

    // Re-test increasing priority of async scripts. Should ignore general incraesed priorities.
    settings->setFEtchIncreaseAsyncScriptPriority(false);
    EXPECT_EQ(ResourceLoadPriorityLow, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Script, deferredRequest, ResourcePriority::NotVisible));
    settings->setFEtchIncreaseAsyncScriptPriority(true);
    EXPECT_EQ(ResourceLoadPriorityMedium, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Script, deferredRequest, ResourcePriority::NotVisible));

    // Ensure we don't go out of bounds
    settings->setFEtchIncreasePriorities(true);
    EXPECT_EQ(ResourceLoadPriorityVeryHigh, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityVeryHigh, Resource::Script, request, ResourcePriority::NotVisible));
    settings->setFEtchIncreasePriorities(false);
    settings->setFEtchDeferLateScripts(true);
    EXPECT_EQ(ResourceLoadPriorityVeryLow, fetchContext->modifyPriorityForExperiments(ResourceLoadPriorityVeryLow, Resource::Script, preloadRequest, ResourcePriority::NotVisible));
}

TEST_F(FrameFetchContextTest, ModifyPriorityForLowPriorityIframes)
{
    Settings* settings = document->frame()->settings();
    settings->setLowPriorityIframes(false);
    FetchRequest request(ResourceRequest("http://www.example.com"), FetchInitiatorInfo());
    FrameFetchContext* childFetchContext = createChildFrame();

    // No low priority iframes, expect default values.
    EXPECT_EQ(ResourceLoadPriorityVeryHigh, childFetchContext->modifyPriorityForExperiments(ResourceLoadPriorityVeryHigh, Resource::MainResource, request, ResourcePriority::NotVisible));
    EXPECT_EQ(ResourceLoadPriorityMedium, childFetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Script, request, ResourcePriority::NotVisible));

    // Low priority iframes enabled, everything should be low priority
    settings->setLowPriorityIframes(true);
    EXPECT_EQ(ResourceLoadPriorityVeryLow, childFetchContext->modifyPriorityForExperiments(ResourceLoadPriorityVeryHigh, Resource::MainResource, request, ResourcePriority::NotVisible));
    EXPECT_EQ(ResourceLoadPriorityVeryLow, childFetchContext->modifyPriorityForExperiments(ResourceLoadPriorityMedium, Resource::Script, request, ResourcePriority::NotVisible));
}

TEST_F(FrameFetchContextTest, EnableDataSaver)
{
    Settings* settings = document->frame()->settings();
    settings->setDataSaverEnabled(true);
    ResourceRequest resourceRequest("http://www.example.com");
    fetchContext->addAdditionalRequestHeaders(resourceRequest, FetchMainResource);
    EXPECT_STREQ("on", resourceRequest.httpHeaderField("Save-Data").utf8().data());
}

TEST_F(FrameFetchContextTest, DisabledDataSaver)
{
    ResourceRequest resourceRequest("http://www.example.com");
    fetchContext->addAdditionalRequestHeaders(resourceRequest, FetchMainResource);
    EXPECT_STREQ("", resourceRequest.httpHeaderField("Save-Data").utf8().data());
}

} // namespace
