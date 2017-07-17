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

#include <memory>
#include "core/dom/Document.h"
#include "core/frame/FrameOwner.h"
#include "core/frame/FrameTypes.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/SubresourceFilter.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/loader/fetch/UniqueIdentifier.h"
#include "platform/loader/testing/MockResource.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebClientHintsType.h"
#include "public/platform/WebDocumentSubresourceFilter.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;

class StubLocalFrameClientWithParent final : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClientWithParent* Create(Frame* parent) {
    return new StubLocalFrameClientWithParent(parent);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(parent_);
    EmptyLocalFrameClient::Trace(visitor);
  }

  Frame* Parent() const override { return parent_.Get(); }

 private:
  explicit StubLocalFrameClientWithParent(Frame* parent) : parent_(parent) {}

  Member<Frame> parent_;
};

class FrameFetchContextMockLocalFrameClient : public EmptyLocalFrameClient {
 public:
  FrameFetchContextMockLocalFrameClient() : EmptyLocalFrameClient() {}
  MOCK_METHOD1(DidDisplayContentWithCertificateErrors, void(const KURL&));
  MOCK_METHOD2(DispatchDidLoadResourceFromMemoryCache,
               void(const ResourceRequest&, const ResourceResponse&));
  MOCK_METHOD0(UserAgent, String());
  MOCK_METHOD0(MayUseClientLoFiForImageRequests, bool());
  MOCK_METHOD0(IsClientLoFiActiveForFrame, bool());
};

class FixedPolicySubresourceFilter : public WebDocumentSubresourceFilter {
 public:
  FixedPolicySubresourceFilter(LoadPolicy policy, int* filtered_load_counter)
      : policy_(policy), filtered_load_counter_(filtered_load_counter) {}

  LoadPolicy GetLoadPolicy(const WebURL& resource_url,
                           WebURLRequest::RequestContext) override {
    return policy_;
  }

  LoadPolicy GetLoadPolicyForWebSocketConnect(const WebURL& url) override {
    return policy_;
  }

  void ReportDisallowedLoad() override { ++*filtered_load_counter_; }

  bool ShouldLogToConsole() override { return false; }

 private:
  const LoadPolicy policy_;
  int* filtered_load_counter_;
};

class FrameFetchContextTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dummy_page_holder = DummyPageHolder::Create(IntSize(500, 500));
    dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(1.0);
    document = &dummy_page_holder->GetDocument();
    fetch_context =
        static_cast<FrameFetchContext*>(&document->Fetcher()->Context());
    owner = DummyFrameOwner::Create();
    FrameFetchContext::ProvideDocumentToContext(*fetch_context, document.Get());
  }

  void TearDown() override {
    if (child_frame)
      child_frame->Detach(FrameDetachType::kRemove);
  }

  FrameFetchContext* CreateChildFrame() {
    child_client = StubLocalFrameClientWithParent::Create(document->GetFrame());
    child_frame = LocalFrame::Create(
        child_client.Get(), *document->GetFrame()->GetPage(), owner.Get());
    child_frame->SetView(
        LocalFrameView::Create(*child_frame, IntSize(500, 500)));
    child_frame->Init();
    child_document = child_frame->GetDocument();
    FrameFetchContext* child_fetch_context = static_cast<FrameFetchContext*>(
        &child_frame->Loader().GetDocumentLoader()->Fetcher()->Context());
    FrameFetchContext::ProvideDocumentToContext(*child_fetch_context,
                                                child_document.Get());
    return child_fetch_context;
  }

  std::unique_ptr<DummyPageHolder> dummy_page_holder;
  // We don't use the DocumentLoader directly in any tests, but need to keep it
  // around as long as the ResourceFetcher and Document live due to indirect
  // usage.
  Persistent<Document> document;
  Persistent<FrameFetchContext> fetch_context;

  Persistent<StubLocalFrameClientWithParent> child_client;
  Persistent<LocalFrame> child_frame;
  Persistent<Document> child_document;
  Persistent<DummyFrameOwner> owner;
};

class FrameFetchContextSubresourceFilterTest : public FrameFetchContextTest {
 protected:
  void SetUp() override {
    FrameFetchContextTest::SetUp();
    filtered_load_callback_counter_ = 0;
  }

  void TearDown() override {
    document->Loader()->SetSubresourceFilter(nullptr);
    FrameFetchContextTest::TearDown();
  }

  int GetFilteredLoadCallCount() const {
    return filtered_load_callback_counter_;
  }

  void SetFilterPolicy(WebDocumentSubresourceFilter::LoadPolicy policy) {
    document->Loader()->SetSubresourceFilter(SubresourceFilter::Create(
        *document, WTF::MakeUnique<FixedPolicySubresourceFilter>(
                       policy, &filtered_load_callback_counter_)));
  }

  ResourceRequestBlockedReason CanRequest() {
    return CanRequestInternal(SecurityViolationReportingPolicy::kReport);
  }

  ResourceRequestBlockedReason CanRequestPreload() {
    return CanRequestInternal(
        SecurityViolationReportingPolicy::kSuppressReporting);
  }

 private:
  ResourceRequestBlockedReason CanRequestInternal(
      SecurityViolationReportingPolicy reporting_policy) {
    KURL input_url(kParsedURLString, "http://example.com/");
    ResourceRequest resource_request(input_url);
    resource_request.SetFetchCredentialsMode(
        WebURLRequest::kFetchCredentialsModeOmit);
    ResourceLoaderOptions options;
    return fetch_context->CanRequest(
        Resource::kImage, resource_request, input_url, options,
        reporting_policy, FetchParameters::kUseDefaultOriginRestrictionForType);
  }

  int filtered_load_callback_counter_;
};

// This test class sets up a mock frame loader client.
class FrameFetchContextMockedLocalFrameClientTest
    : public FrameFetchContextTest {
 protected:
  void SetUp() override {
    url = KURL(NullURL(), "https://example.test/foo");
    main_resource_url = KURL(NullURL(), "https://www.example.test");
    client = new ::testing::NiceMock<FrameFetchContextMockLocalFrameClient>();
    dummy_page_holder =
        DummyPageHolder::Create(IntSize(500, 500), nullptr, client);
    dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(1.0);
    document = &dummy_page_holder->GetDocument();
    document->SetURL(main_resource_url);
    fetch_context =
        static_cast<FrameFetchContext*>(&document->Fetcher()->Context());
    owner = DummyFrameOwner::Create();
    FrameFetchContext::ProvideDocumentToContext(*fetch_context, document.Get());
  }

  KURL url;
  KURL main_resource_url;

  Persistent<::testing::NiceMock<FrameFetchContextMockLocalFrameClient>> client;
};

class FrameFetchContextModifyRequestTest : public FrameFetchContextTest {
 public:
  FrameFetchContextModifyRequestTest()
      : example_origin(SecurityOrigin::Create(
            KURL(kParsedURLString, "https://example.test/"))),
        secure_origin(SecurityOrigin::Create(
            KURL(kParsedURLString, "https://secureorigin.test/image.png"))) {}

 protected:
  void ExpectUpgrade(const char* input, const char* expected) {
    ExpectUpgrade(input, WebURLRequest::kRequestContextScript,
                  WebURLRequest::kFrameTypeNone, expected);
  }

  void ExpectUpgrade(const char* input,
                     WebURLRequest::RequestContext request_context,
                     WebURLRequest::FrameType frame_type,
                     const char* expected) {
    KURL input_url(kParsedURLString, input);
    KURL expected_url(kParsedURLString, expected);

    ResourceRequest resource_request(input_url);
    resource_request.SetRequestContext(request_context);
    resource_request.SetFrameType(frame_type);

    fetch_context->ModifyRequestForCSP(resource_request);

    EXPECT_EQ(expected_url.GetString(), resource_request.Url().GetString());
    EXPECT_EQ(expected_url.Protocol(), resource_request.Url().Protocol());
    EXPECT_EQ(expected_url.Host(), resource_request.Url().Host());
    EXPECT_EQ(expected_url.Port(), resource_request.Url().Port());
    EXPECT_EQ(expected_url.HasPort(), resource_request.Url().HasPort());
    EXPECT_EQ(expected_url.GetPath(), resource_request.Url().GetPath());
  }

  void ExpectUpgradeInsecureRequestHeader(const char* input,
                                          WebURLRequest::FrameType frame_type,
                                          bool should_prefer) {
    KURL input_url(kParsedURLString, input);

    ResourceRequest resource_request(input_url);
    resource_request.SetRequestContext(WebURLRequest::kRequestContextScript);
    resource_request.SetFrameType(frame_type);

    fetch_context->ModifyRequestForCSP(resource_request);

    EXPECT_EQ(
        should_prefer ? String("1") : String(),
        resource_request.HttpHeaderField(HTTPNames::Upgrade_Insecure_Requests));

    // Calling modifyRequestForCSP more than once shouldn't affect the
    // header.
    if (should_prefer) {
      fetch_context->ModifyRequestForCSP(resource_request);
      EXPECT_EQ("1", resource_request.HttpHeaderField(
                         HTTPNames::Upgrade_Insecure_Requests));
    }
  }

  void ExpectSetRequiredCSPRequestHeader(
      const char* input,
      WebURLRequest::FrameType frame_type,
      const AtomicString& expected_required_csp) {
    KURL input_url(kParsedURLString, input);
    ResourceRequest resource_request(input_url);
    resource_request.SetRequestContext(WebURLRequest::kRequestContextScript);
    resource_request.SetFrameType(frame_type);

    fetch_context->ModifyRequestForCSP(resource_request);

    EXPECT_EQ(expected_required_csp,
              resource_request.HttpHeaderField(HTTPNames::Sec_Required_CSP));
  }

  void SetFrameOwnerBasedOnFrameType(WebURLRequest::FrameType frame_type,
                                     HTMLIFrameElement* iframe,
                                     const AtomicString& potential_value) {
    if (frame_type != WebURLRequest::kFrameTypeNested) {
      document->GetFrame()->SetOwner(nullptr);
      return;
    }

    iframe->setAttribute(HTMLNames::cspAttr, potential_value);
    document->GetFrame()->SetOwner(iframe);
  }

  RefPtr<SecurityOrigin> example_origin;
  RefPtr<SecurityOrigin> secure_origin;
};

TEST_F(FrameFetchContextModifyRequestTest, UpgradeInsecureResourceRequests) {
  struct TestCase {
    const char* original;
    const char* upgraded;
  } tests[] = {
      {"http://example.test/image.png", "https://example.test/image.png"},
      {"http://example.test:80/image.png",
       "https://example.test:443/image.png"},
      {"http://example.test:1212/image.png",
       "https://example.test:1212/image.png"},

      {"https://example.test/image.png", "https://example.test/image.png"},
      {"https://example.test:80/image.png",
       "https://example.test:80/image.png"},
      {"https://example.test:1212/image.png",
       "https://example.test:1212/image.png"},

      {"ftp://example.test/image.png", "ftp://example.test/image.png"},
      {"ftp://example.test:21/image.png", "ftp://example.test:21/image.png"},
      {"ftp://example.test:1212/image.png",
       "ftp://example.test:1212/image.png"},
  };

  FrameFetchContext::ProvideDocumentToContext(*fetch_context, document.Get());
  document->SetInsecureRequestPolicy(kUpgradeInsecureRequests);

  for (const auto& test : tests) {
    document->InsecureNavigationsToUpgrade()->clear();

    // We always upgrade for FrameTypeNone and FrameTypeNested.
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextScript,
                  WebURLRequest::kFrameTypeNone, test.upgraded);
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextScript,
                  WebURLRequest::kFrameTypeNested, test.upgraded);

    // We do not upgrade for FrameTypeTopLevel or FrameTypeAuxiliary...
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextScript,
                  WebURLRequest::kFrameTypeTopLevel, test.original);
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextScript,
                  WebURLRequest::kFrameTypeAuxiliary, test.original);

    // unless the request context is RequestContextForm.
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextForm,
                  WebURLRequest::kFrameTypeTopLevel, test.upgraded);
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextForm,
                  WebURLRequest::kFrameTypeAuxiliary, test.upgraded);

    // Or unless the host of the resource is in the document's
    // InsecureNavigationsSet:
    document->AddInsecureNavigationUpgrade(
        example_origin->Host().Impl()->GetHash());
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextScript,
                  WebURLRequest::kFrameTypeTopLevel, test.upgraded);
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextScript,
                  WebURLRequest::kFrameTypeAuxiliary, test.upgraded);
  }
}

TEST_F(FrameFetchContextModifyRequestTest,
       DoNotUpgradeInsecureResourceRequests) {
  FrameFetchContext::ProvideDocumentToContext(*fetch_context, document.Get());
  document->SetSecurityOrigin(secure_origin);
  document->SetInsecureRequestPolicy(kLeaveInsecureRequestsAlone);

  ExpectUpgrade("http://example.test/image.png",
                "http://example.test/image.png");
  ExpectUpgrade("http://example.test:80/image.png",
                "http://example.test:80/image.png");
  ExpectUpgrade("http://example.test:1212/image.png",
                "http://example.test:1212/image.png");

  ExpectUpgrade("https://example.test/image.png",
                "https://example.test/image.png");
  ExpectUpgrade("https://example.test:80/image.png",
                "https://example.test:80/image.png");
  ExpectUpgrade("https://example.test:1212/image.png",
                "https://example.test:1212/image.png");

  ExpectUpgrade("ftp://example.test/image.png", "ftp://example.test/image.png");
  ExpectUpgrade("ftp://example.test:21/image.png",
                "ftp://example.test:21/image.png");
  ExpectUpgrade("ftp://example.test:1212/image.png",
                "ftp://example.test:1212/image.png");
}

TEST_F(FrameFetchContextModifyRequestTest, SendUpgradeInsecureRequestHeader) {
  struct TestCase {
    const char* to_request;
    WebURLRequest::FrameType frame_type;
    bool should_prefer;
  } tests[] = {
      {"http://example.test/page.html", WebURLRequest::kFrameTypeAuxiliary,
       true},
      {"http://example.test/page.html", WebURLRequest::kFrameTypeNested, true},
      {"http://example.test/page.html", WebURLRequest::kFrameTypeNone, false},
      {"http://example.test/page.html", WebURLRequest::kFrameTypeTopLevel,
       true},
      {"https://example.test/page.html", WebURLRequest::kFrameTypeAuxiliary,
       true},
      {"https://example.test/page.html", WebURLRequest::kFrameTypeNested, true},
      {"https://example.test/page.html", WebURLRequest::kFrameTypeNone, false},
      {"https://example.test/page.html", WebURLRequest::kFrameTypeTopLevel,
       true}};

  // This should work correctly both when the FrameFetchContext has a Document,
  // and when it doesn't (e.g. during main frame navigations), so run through
  // the tests both before and after providing a document to the context.
  for (const auto& test : tests) {
    document->SetInsecureRequestPolicy(kLeaveInsecureRequestsAlone);
    ExpectUpgradeInsecureRequestHeader(test.to_request, test.frame_type,
                                       test.should_prefer);

    document->SetInsecureRequestPolicy(kUpgradeInsecureRequests);
    ExpectUpgradeInsecureRequestHeader(test.to_request, test.frame_type,
                                       test.should_prefer);
  }

  FrameFetchContext::ProvideDocumentToContext(*fetch_context, document.Get());

  for (const auto& test : tests) {
    document->SetInsecureRequestPolicy(kLeaveInsecureRequestsAlone);
    ExpectUpgradeInsecureRequestHeader(test.to_request, test.frame_type,
                                       test.should_prefer);

    document->SetInsecureRequestPolicy(kUpgradeInsecureRequests);
    ExpectUpgradeInsecureRequestHeader(test.to_request, test.frame_type,
                                       test.should_prefer);
  }
}

TEST_F(FrameFetchContextModifyRequestTest, SendRequiredCSPHeader) {
  struct TestCase {
    const char* to_request;
    WebURLRequest::FrameType frame_type;
  } tests[] = {
      {"https://example.test/page.html", WebURLRequest::kFrameTypeAuxiliary},
      {"https://example.test/page.html", WebURLRequest::kFrameTypeNested},
      {"https://example.test/page.html", WebURLRequest::kFrameTypeNone},
      {"https://example.test/page.html", WebURLRequest::kFrameTypeTopLevel}};

  HTMLIFrameElement* iframe = HTMLIFrameElement::Create(*document);
  const AtomicString& required_csp = AtomicString("default-src 'none'");
  const AtomicString& another_required_csp = AtomicString("default-src 'self'");

  for (const auto& test : tests) {
    SetFrameOwnerBasedOnFrameType(test.frame_type, iframe, required_csp);
    ExpectSetRequiredCSPRequestHeader(
        test.to_request, test.frame_type,
        test.frame_type == WebURLRequest::kFrameTypeNested ? required_csp
                                                           : g_null_atom);

    SetFrameOwnerBasedOnFrameType(test.frame_type, iframe,
                                  another_required_csp);
    ExpectSetRequiredCSPRequestHeader(
        test.to_request, test.frame_type,
        test.frame_type == WebURLRequest::kFrameTypeNested
            ? another_required_csp
            : g_null_atom);
  }
}

// Tests that PopulateResourceRequest() checks report-only CSP headers, so that
// any violations are reported before the request is modified.
TEST_F(FrameFetchContextTest, PopulateResourceRequestChecksReportOnlyCSP) {
  ContentSecurityPolicy* policy = document->GetContentSecurityPolicy();
  policy->DidReceiveHeader(
      "upgrade-insecure-requests; script-src https://foo.test",
      kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceHTTP);
  policy->DidReceiveHeader("script-src https://bar.test",
                           kContentSecurityPolicyHeaderTypeReport,
                           kContentSecurityPolicyHeaderSourceHTTP);
  KURL url(NullURL(), "http://baz.test");
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextScript);
  resource_request.SetFetchCredentialsMode(
      WebURLRequest::kFetchCredentialsModeOmit);
  ResourceLoaderOptions options;
  fetch_context->PopulateResourceRequest(
      url, Resource::kScript, ClientHintsPreferences(),
      FetchParameters::ResourceWidth(), options,
      SecurityViolationReportingPolicy::kReport, resource_request);
  EXPECT_EQ(1u, policy->violation_reports_sent_.size());
  // Check that the resource was upgraded to a secure URL.
  EXPECT_EQ(KURL(NullURL(), "https://baz.test"), resource_request.Url());
}

class FrameFetchContextHintsTest : public FrameFetchContextTest {
 public:
  FrameFetchContextHintsTest() {}

 protected:
  void ExpectHeader(const char* input,
                    const char* header_name,
                    bool is_present,
                    const char* header_value,
                    float width = 0) {
    ClientHintsPreferences hints_preferences;

    FetchParameters::ResourceWidth resource_width;
    if (width > 0) {
      resource_width.width = width;
      resource_width.is_set = true;
    }

    KURL input_url(kParsedURLString, input);
    ResourceRequest resource_request(input_url);

    fetch_context->AddClientHintsIfNecessary(hints_preferences, resource_width,
                                             resource_request);

    EXPECT_EQ(is_present ? String(header_value) : String(),
              resource_request.HttpHeaderField(header_name));
  }
};

TEST_F(FrameFetchContextHintsTest, MonitorDeviceRAMHints) {
  ExpectHeader("http://www.example.com/1.gif", "device-ram", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(kWebClientHintsTypeDeviceRam);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("http://www.example.com/1.gif", "device-ram", true, "4");
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(2048);
  ExpectHeader("http://www.example.com/1.gif", "device-ram", true, "2");
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(64385);
  ExpectHeader("http://www.example.com/1.gif", "device-ram", true, "64");
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(768);
  ExpectHeader("http://www.example.com/1.gif", "device-ram", true, "0.75");
  ExpectHeader("http://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorDPRHints) {
  ExpectHeader("http://www.example.com/1.gif", "DPR", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(kWebClientHintsTypeDpr);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("http://www.example.com/1.gif", "DPR", true, "1");
  dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(2.5);
  ExpectHeader("http://www.example.com/1.gif", "DPR", true, "2.5");
  ExpectHeader("http://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorResourceWidthHints) {
  ExpectHeader("http://www.example.com/1.gif", "Width", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(kWebClientHintsTypeResourceWidth);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("http://www.example.com/1.gif", "Width", true, "500", 500);
  ExpectHeader("http://www.example.com/1.gif", "Width", true, "667", 666.6666);
  ExpectHeader("http://www.example.com/1.gif", "DPR", false, "");
  dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(2.5);
  ExpectHeader("http://www.example.com/1.gif", "Width", true, "1250", 500);
  ExpectHeader("http://www.example.com/1.gif", "Width", true, "1667", 666.6666);
}

TEST_F(FrameFetchContextHintsTest, MonitorViewportWidthHints) {
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(kWebClientHintsTypeViewportWidth);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", true, "500");
  dummy_page_holder->GetFrameView().SetLayoutSizeFixedToFrameSize(false);
  dummy_page_holder->GetFrameView().SetLayoutSize(IntSize(800, 800));
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", true, "800");
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", true, "800",
               666.6666);
  ExpectHeader("http://www.example.com/1.gif", "DPR", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorAllHints) {
  ExpectHeader("http://www.example.com/1.gif", "device-ram", false, "");
  ExpectHeader("http://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Width", false, "");

  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(kWebClientHintsTypeDeviceRam);
  preferences.SetShouldSendForTesting(kWebClientHintsTypeDpr);
  preferences.SetShouldSendForTesting(kWebClientHintsTypeResourceWidth);
  preferences.SetShouldSendForTesting(kWebClientHintsTypeViewportWidth);
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(4096);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("http://www.example.com/1.gif", "device-ram", true, "4");
  ExpectHeader("http://www.example.com/1.gif", "DPR", true, "1");
  ExpectHeader("http://www.example.com/1.gif", "Width", true, "400", 400);
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", true, "500");
}

TEST_F(FrameFetchContextHintsTest, ClientHintsDeviceRAM) {
  EXPECT_EQ(0.125, FrameFetchContext::ClientHintsDeviceRAM(128));  // 128MB
  EXPECT_EQ(0.25, FrameFetchContext::ClientHintsDeviceRAM(256));   // 256MB
  EXPECT_EQ(0.5, FrameFetchContext::ClientHintsDeviceRAM(510));    // <512MB
  EXPECT_EQ(0.5, FrameFetchContext::ClientHintsDeviceRAM(512));    // 512MB
  EXPECT_EQ(0.5, FrameFetchContext::ClientHintsDeviceRAM(640));    // 512+128MB
  EXPECT_EQ(0.75, FrameFetchContext::ClientHintsDeviceRAM(768));   // 512+256MB
  EXPECT_EQ(1, FrameFetchContext::ClientHintsDeviceRAM(1000));     // <1GB
  EXPECT_EQ(1, FrameFetchContext::ClientHintsDeviceRAM(1024));     // 1GB
  EXPECT_EQ(1.5, FrameFetchContext::ClientHintsDeviceRAM(1536));   // 1.5GB
  EXPECT_EQ(2, FrameFetchContext::ClientHintsDeviceRAM(2000));     // <2GB
  EXPECT_EQ(2, FrameFetchContext::ClientHintsDeviceRAM(2048));     // 2GB
  EXPECT_EQ(3, FrameFetchContext::ClientHintsDeviceRAM(3000));     // <3GB
  EXPECT_EQ(4, FrameFetchContext::ClientHintsDeviceRAM(5120));     // 5GB
  EXPECT_EQ(8, FrameFetchContext::ClientHintsDeviceRAM(8192));     // 8GB
  EXPECT_EQ(16, FrameFetchContext::ClientHintsDeviceRAM(16384));   // 16GB
  EXPECT_EQ(32, FrameFetchContext::ClientHintsDeviceRAM(32768));   // 32GB
  EXPECT_EQ(64, FrameFetchContext::ClientHintsDeviceRAM(64385));   // <64GB
}

TEST_F(FrameFetchContextTest, MainResourceCachePolicy) {
  // Default case
  ResourceRequest request("http://www.example.com");
  EXPECT_EQ(WebCachePolicy::kUseProtocolCachePolicy,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Post
  ResourceRequest post_request("http://www.example.com");
  post_request.SetHTTPMethod(HTTPNames::POST);
  EXPECT_EQ(
      WebCachePolicy::kValidatingCacheData,
      fetch_context->ResourceRequestCachePolicy(
          post_request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Re-post
  document->Loader()->SetLoadType(kFrameLoadTypeBackForward);
  EXPECT_EQ(
      WebCachePolicy::kReturnCacheDataDontLoad,
      fetch_context->ResourceRequestCachePolicy(
          post_request, Resource::kMainResource, FetchParameters::kNoDefer));

  // FrameLoadTypeReload
  document->Loader()->SetLoadType(kFrameLoadTypeReload);
  EXPECT_EQ(WebCachePolicy::kValidatingCacheData,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Conditional request
  document->Loader()->SetLoadType(kFrameLoadTypeStandard);
  ResourceRequest conditional("http://www.example.com");
  conditional.SetHTTPHeaderField(HTTPNames::If_Modified_Since, "foo");
  EXPECT_EQ(
      WebCachePolicy::kValidatingCacheData,
      fetch_context->ResourceRequestCachePolicy(
          conditional, Resource::kMainResource, FetchParameters::kNoDefer));

  // FrameLoadTypeReloadBypassingCache
  document->Loader()->SetLoadType(kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(WebCachePolicy::kBypassingCache,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));

  // FrameLoadTypeReloadBypassingCache with a conditional request
  document->Loader()->SetLoadType(kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(
      WebCachePolicy::kBypassingCache,
      fetch_context->ResourceRequestCachePolicy(
          conditional, Resource::kMainResource, FetchParameters::kNoDefer));

  // FrameLoadTypeReloadBypassingCache with a post request
  document->Loader()->SetLoadType(kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(
      WebCachePolicy::kBypassingCache,
      fetch_context->ResourceRequestCachePolicy(
          post_request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Set up a child frame
  FrameFetchContext* child_fetch_context = CreateChildFrame();

  // Child frame as part of back/forward
  document->Loader()->SetLoadType(kFrameLoadTypeBackForward);
  EXPECT_EQ(WebCachePolicy::kReturnCacheDataElseLoad,
            child_fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Child frame as part of reload
  document->Loader()->SetLoadType(kFrameLoadTypeReload);
  EXPECT_EQ(WebCachePolicy::kUseProtocolCachePolicy,
            child_fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Child frame as part of reload bypassing cache
  document->Loader()->SetLoadType(kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(WebCachePolicy::kBypassingCache,
            child_fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Per-frame bypassing reload, but parent load type is different.
  // This is not the case users can trigger through user interfaces, but for
  // checking code correctness and consistency.
  document->Loader()->SetLoadType(kFrameLoadTypeReload);
  child_frame->Loader().GetDocumentLoader()->SetLoadType(
      kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(WebCachePolicy::kBypassingCache,
            child_fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));
}

TEST_F(FrameFetchContextTest, SubResourceCachePolicy) {
  // Reset load event state: if the load event is finished, we ignore the
  // DocumentLoader load type.
  document->open();
  ASSERT_FALSE(document->LoadEventFinished());

  // Default case
  ResourceRequest request("http://www.example.com/mock");
  EXPECT_EQ(WebCachePolicy::kUseProtocolCachePolicy,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMock, FetchParameters::kNoDefer));

  // FrameLoadTypeReload should not affect sub-resources
  document->Loader()->SetLoadType(kFrameLoadTypeReload);
  EXPECT_EQ(WebCachePolicy::kUseProtocolCachePolicy,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMock, FetchParameters::kNoDefer));

  // Conditional request
  document->Loader()->SetLoadType(kFrameLoadTypeStandard);
  ResourceRequest conditional("http://www.example.com/mock");
  conditional.SetHTTPHeaderField(HTTPNames::If_Modified_Since, "foo");
  EXPECT_EQ(WebCachePolicy::kValidatingCacheData,
            fetch_context->ResourceRequestCachePolicy(
                conditional, Resource::kMock, FetchParameters::kNoDefer));

  // FrameLoadTypeReloadBypassingCache
  document->Loader()->SetLoadType(kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(WebCachePolicy::kBypassingCache,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMock, FetchParameters::kNoDefer));

  // FrameLoadTypeReloadBypassingCache with a conditional request
  document->Loader()->SetLoadType(kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(WebCachePolicy::kBypassingCache,
            fetch_context->ResourceRequestCachePolicy(
                conditional, Resource::kMock, FetchParameters::kNoDefer));

  // Back/forward navigation
  document->Loader()->SetLoadType(kFrameLoadTypeBackForward);
  EXPECT_EQ(WebCachePolicy::kReturnCacheDataElseLoad,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMock, FetchParameters::kNoDefer));

  // Back/forward navigation with a conditional request
  document->Loader()->SetLoadType(kFrameLoadTypeBackForward);
  EXPECT_EQ(WebCachePolicy::kReturnCacheDataElseLoad,
            fetch_context->ResourceRequestCachePolicy(
                conditional, Resource::kMock, FetchParameters::kNoDefer));
}

TEST_F(FrameFetchContextTest, SetFirstPartyCookieAndRequestorOrigin) {
  struct TestCase {
    const char* document_url;
    bool document_sandboxed;
    const char* requestor_origin;  // "" => null
    WebURLRequest::FrameType frame_type;
    const char* serialized_origin;  // "" => null
  } cases[] = {
      // No document origin => unique request origin
      {"", false, "", WebURLRequest::kFrameTypeNone, "null"},
      {"", true, "", WebURLRequest::kFrameTypeNone, "null"},

      // Document origin => request origin
      {"http://example.test", false, "", WebURLRequest::kFrameTypeNone,
       "http://example.test"},
      {"http://example.test", true, "", WebURLRequest::kFrameTypeNone,
       "http://example.test"},

      // If the request already has a requestor origin, then
      // 'SetFirstPartyCookieAndRequestorOrigin' leaves it alone:
      {"http://example.test", false, "http://not-example.test",
       WebURLRequest::kFrameTypeNone, "http://not-example.test"},
      {"http://example.test", true, "http://not-example.test",
       WebURLRequest::kFrameTypeNone, "http://not-example.test"},
  };

  int index = 0;
  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message() << index++ << " " << test.document_url
                                      << " => " << test.serialized_origin);
    // Set up a new document to ensure sandbox flags are cleared:
    dummy_page_holder = DummyPageHolder::Create(IntSize(500, 500));
    dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(1.0);
    document = &dummy_page_holder->GetDocument();
    fetch_context =
        static_cast<FrameFetchContext*>(&document->Fetcher()->Context());
    FrameFetchContext::ProvideDocumentToContext(*fetch_context, document.Get());

    // Setup the test:
    document->SetURL(KURL(kParsedURLString, test.document_url));
    document->SetSecurityOrigin(SecurityOrigin::Create(document->Url()));

    if (test.document_sandboxed)
      document->EnforceSandboxFlags(kSandboxOrigin);

    ResourceRequest request("http://example.test/");
    request.SetFrameType(test.frame_type);
    if (strlen(test.requestor_origin) > 0) {
      request.SetRequestorOrigin(SecurityOrigin::Create(
          KURL(kParsedURLString, test.requestor_origin)));
    }

    // Compare the populated |requestorOrigin| against |test.serializedOrigin|
    fetch_context->SetFirstPartyCookieAndRequestorOrigin(request);
    if (strlen(test.serialized_origin) == 0) {
      EXPECT_TRUE(!request.RequestorOrigin());
    } else {
      EXPECT_EQ(String(test.serialized_origin),
                request.RequestorOrigin()->ToString());
    }

    EXPECT_EQ(document->FirstPartyForCookies(), request.FirstPartyForCookies());
  }
}

// Tests if "Save-Data" header is correctly added on the first load and reload.
TEST_F(FrameFetchContextTest, EnableDataSaver) {
  Settings* settings = document->GetFrame()->GetSettings();
  settings->SetDataSaverEnabled(true);
  ResourceRequest resource_request("http://www.example.com");
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ("on", resource_request.HttpHeaderField("Save-Data"));

  // Subsequent call to addAdditionalRequestHeaders should not append to the
  // save-data header.
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ("on", resource_request.HttpHeaderField("Save-Data"));
}

// Tests if "Save-Data" header is not added when the data saver is disabled.
TEST_F(FrameFetchContextTest, DisabledDataSaver) {
  ResourceRequest resource_request("http://www.example.com");
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ(String(), resource_request.HttpHeaderField("Save-Data"));
}

// Tests if reload variants can reflect the current data saver setting.
TEST_F(FrameFetchContextTest, ChangeDataSaverConfig) {
  Settings* settings = document->GetFrame()->GetSettings();
  settings->SetDataSaverEnabled(true);
  ResourceRequest resource_request("http://www.example.com");
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ("on", resource_request.HttpHeaderField("Save-Data"));

  settings->SetDataSaverEnabled(false);
  document->Loader()->SetLoadType(kFrameLoadTypeReload);
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ(String(), resource_request.HttpHeaderField("Save-Data"));

  settings->SetDataSaverEnabled(true);
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ("on", resource_request.HttpHeaderField("Save-Data"));

  settings->SetDataSaverEnabled(false);
  document->Loader()->SetLoadType(kFrameLoadTypeReload);
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ(String(), resource_request.HttpHeaderField("Save-Data"));
}

// Tests that the embedder gets correct notification when a resource is loaded
// from the memory cache.
TEST_F(FrameFetchContextMockedLocalFrameClientTest,
       DispatchDidLoadResourceFromMemoryCache) {
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextImage);
  resource_request.SetFetchCredentialsMode(
      WebURLRequest::kFetchCredentialsModeOmit);
  Resource* resource = MockResource::Create(resource_request);
  EXPECT_CALL(*client,
              DispatchDidLoadResourceFromMemoryCache(
                  ::testing::AllOf(
                      ::testing::Property(&ResourceRequest::Url, url),
                      ::testing::Property(&ResourceRequest::GetFrameType,
                                          WebURLRequest::kFrameTypeNone),
                      ::testing::Property(&ResourceRequest::GetRequestContext,
                                          WebURLRequest::kRequestContextImage)),
                  ResourceResponse()));
  fetch_context->DispatchDidLoadResourceFromMemoryCache(
      CreateUniqueIdentifier(), resource_request, resource->GetResponse());
}

// Tests that when a resource with certificate errors is loaded from the memory
// cache, the embedder is notified.
TEST_F(FrameFetchContextMockedLocalFrameClientTest,
       MemoryCacheCertificateError) {
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextImage);
  resource_request.SetFetchCredentialsMode(
      WebURLRequest::kFetchCredentialsModeOmit);
  ResourceResponse response;
  response.SetURL(url);
  response.SetHasMajorCertificateErrors(true);
  Resource* resource = MockResource::Create(resource_request);
  resource->SetResponse(response);
  EXPECT_CALL(*client, DidDisplayContentWithCertificateErrors(url));
  fetch_context->DispatchDidLoadResourceFromMemoryCache(
      CreateUniqueIdentifier(), resource_request, resource->GetResponse());
}

TEST_F(FrameFetchContextSubresourceFilterTest, Filter) {
  SetFilterPolicy(WebDocumentSubresourceFilter::kDisallow);

  EXPECT_EQ(ResourceRequestBlockedReason::kSubresourceFilter, CanRequest());
  EXPECT_EQ(1, GetFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::kSubresourceFilter, CanRequest());
  EXPECT_EQ(2, GetFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::kSubresourceFilter,
            CanRequestPreload());
  EXPECT_EQ(2, GetFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::kSubresourceFilter, CanRequest());
  EXPECT_EQ(3, GetFilteredLoadCallCount());
}

TEST_F(FrameFetchContextSubresourceFilterTest, Allow) {
  SetFilterPolicy(WebDocumentSubresourceFilter::kAllow);

  EXPECT_EQ(ResourceRequestBlockedReason::kNone, CanRequest());
  EXPECT_EQ(0, GetFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::kNone, CanRequestPreload());
  EXPECT_EQ(0, GetFilteredLoadCallCount());
}

TEST_F(FrameFetchContextSubresourceFilterTest, WouldDisallow) {
  SetFilterPolicy(WebDocumentSubresourceFilter::kWouldDisallow);

  EXPECT_EQ(ResourceRequestBlockedReason::kNone, CanRequest());
  EXPECT_EQ(0, GetFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::kNone, CanRequestPreload());
  EXPECT_EQ(0, GetFilteredLoadCallCount());
}

TEST_F(FrameFetchContextTest, AddAdditionalRequestHeadersWhenDetached) {
  const KURL document_url(NullURL(), "https://www2.example.com/fuga/hoge.html");
  const String origin = "https://www2.example.com";
  ResourceRequest request(KURL(NullURL(), "https://localhost/"));
  request.SetHTTPMethod("PUT");

  Settings* settings = document->GetFrame()->GetSettings();
  settings->SetDataSaverEnabled(true);
  document->SetSecurityOrigin(SecurityOrigin::Create(KURL(NullURL(), origin)));
  document->SetURL(document_url);
  document->SetReferrerPolicy(kReferrerPolicyOrigin);
  document->SetAddressSpace(kWebAddressSpacePublic);

  dummy_page_holder = nullptr;

  EXPECT_FALSE(request.IsExternalRequest());

  fetch_context->AddAdditionalRequestHeaders(request, kFetchSubresource);

  EXPECT_EQ(origin, request.HttpHeaderField(HTTPNames::Origin));
  EXPECT_EQ(String(origin + "/"), request.HttpHeaderField(HTTPNames::Referer));
  EXPECT_EQ(String(), request.HttpHeaderField("Save-Data"));
  EXPECT_TRUE(request.IsExternalRequest());
}

TEST_F(FrameFetchContextTest, ResourceRequestCachePolicyWhenDetached) {
  ResourceRequest request(KURL(NullURL(), "https://localhost/"));

  dummy_page_holder = nullptr;

  EXPECT_EQ(WebCachePolicy::kUseProtocolCachePolicy,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kRaw, FetchParameters::kNoDefer));
}

TEST_F(FrameFetchContextTest, DispatchDidChangePriorityWhenDetached) {
  dummy_page_holder = nullptr;

  fetch_context->DispatchDidChangeResourcePriority(2, kResourceLoadPriorityLow,
                                                   3);
  // Should not crash.
}

TEST_F(FrameFetchContextMockedLocalFrameClientTest,
       PrepareRequestWhenDetached) {
  Checkpoint checkpoint;

  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, UserAgent()).WillOnce(::testing::Return(String("hi")));
  EXPECT_CALL(checkpoint, Call(2));

  checkpoint.Call(1);
  dummy_page_holder = nullptr;
  checkpoint.Call(2);

  ResourceRequest request(KURL(NullURL(), "https://localhost/"));
  fetch_context->PrepareRequest(request,
                                FetchContext::RedirectType::kNotForRedirect);

  EXPECT_EQ("hi", request.HttpHeaderField(HTTPNames::User_Agent));
}

TEST_F(FrameFetchContextTest, DispatchWillSendRequestWhenDetached) {
  ResourceRequest request(KURL(NullURL(), "https://www.example.com/"));
  ResourceResponse response;
  FetchInitiatorInfo initiator_info;

  dummy_page_holder = nullptr;

  fetch_context->DispatchWillSendRequest(1, request, response, initiator_info);
  // Should not crash.
}

TEST_F(FrameFetchContextTest,
       DispatchDidLoadResourceFromMemoryCacheWhenDetached) {
  ResourceRequest request(KURL(NullURL(), "https://www.example.com/"));
  ResourceResponse response;
  FetchInitiatorInfo initiator_info;

  dummy_page_holder = nullptr;

  fetch_context->DispatchDidLoadResourceFromMemoryCache(8, request, response);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, DispatchDidReceiveResponseWhenDetached) {
  ResourceRequest request(KURL(NullURL(), "https://www.example.com/"));
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);
  Resource* resource = MockResource::Create(request);
  ResourceResponse response;

  dummy_page_holder = nullptr;

  fetch_context->DispatchDidReceiveResponse(
      3, response, WebURLRequest::kFrameTypeTopLevel,
      WebURLRequest::kRequestContextFetch, resource,
      FetchContext::ResourceResponseType::kNotFromMemoryCache);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, DispatchDidReceiveDataWhenDetached) {
  dummy_page_holder = nullptr;

  fetch_context->DispatchDidReceiveData(3, "abcd", 4);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, DispatchDidReceiveEncodedDataWhenDetached) {
  dummy_page_holder = nullptr;

  fetch_context->DispatchDidReceiveEncodedData(8, 9);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, DispatchDidDownloadDataWhenDetached) {
  dummy_page_holder = nullptr;

  fetch_context->DispatchDidDownloadData(4, 7, 9);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, DispatchDidFinishLoadingWhenDetached) {
  dummy_page_holder = nullptr;

  fetch_context->DispatchDidFinishLoading(4, 0.3, 8, 10);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, DispatchDidFailWhenDetached) {
  dummy_page_holder = nullptr;

  fetch_context->DispatchDidFail(8, ResourceError(), 5, false);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, ShouldLoadNewResourceWhenDetached) {
  dummy_page_holder = nullptr;

  EXPECT_FALSE(fetch_context->ShouldLoadNewResource(Resource::kImage));
  EXPECT_FALSE(fetch_context->ShouldLoadNewResource(Resource::kRaw));
  EXPECT_FALSE(fetch_context->ShouldLoadNewResource(Resource::kScript));
  EXPECT_FALSE(fetch_context->ShouldLoadNewResource(Resource::kMainResource));
}

TEST_F(FrameFetchContextTest, RecordLoadingActivityWhenDetached) {
  ResourceRequest request(KURL(NullURL(), "https://www.example.com/"));

  dummy_page_holder = nullptr;

  fetch_context->RecordLoadingActivity(4, request, Resource::kRaw,
                                       FetchInitiatorTypeNames::xmlhttprequest);
  // Should not crash.

  fetch_context->RecordLoadingActivity(8, request, Resource::kRaw,
                                       FetchInitiatorTypeNames::document);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, DidLoadResourceWhenDetached) {
  ResourceRequest request(KURL(NullURL(), "https://www.example.com/"));
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);
  Resource* resource = MockResource::Create(request);

  dummy_page_holder = nullptr;

  fetch_context->DidLoadResource(resource);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, AddResourceTimingWhenDetached) {
  RefPtr<ResourceTimingInfo> info =
      ResourceTimingInfo::Create("type", 0.3, false);

  dummy_page_holder = nullptr;

  fetch_context->AddResourceTiming(*info);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, AllowImageWhenDetached) {
  KURL url(NullURL(), "https://www.example.com/");

  dummy_page_holder = nullptr;

  EXPECT_TRUE(fetch_context->AllowImage(true, url));
  EXPECT_TRUE(fetch_context->AllowImage(false, url));
}

TEST_F(FrameFetchContextTest, IsControlledByServiceWorkerWhenDetached) {
  dummy_page_holder = nullptr;

  EXPECT_FALSE(fetch_context->IsControlledByServiceWorker());
}

TEST_F(FrameFetchContextTest, IsMainFrameWhenDetached) {
  FetchContext* child_fetch_context = CreateChildFrame();

  EXPECT_TRUE(fetch_context->IsMainFrame());
  EXPECT_FALSE(child_fetch_context->IsMainFrame());

  dummy_page_holder = nullptr;

  EXPECT_TRUE(fetch_context->IsMainFrame());
  EXPECT_FALSE(child_fetch_context->IsMainFrame());
}

TEST_F(FrameFetchContextTest, DefersLoadingWhenDetached) {
  EXPECT_FALSE(fetch_context->DefersLoading());
}

TEST_F(FrameFetchContextTest, IsLoadCompleteWhenDetached_1) {
  document->open();
  EXPECT_FALSE(fetch_context->IsLoadComplete());

  dummy_page_holder = nullptr;

  EXPECT_TRUE(fetch_context->IsLoadComplete());
}

TEST_F(FrameFetchContextTest, IsLoadCompleteWhenDetached_2) {
  EXPECT_TRUE(fetch_context->IsLoadComplete());

  dummy_page_holder = nullptr;

  EXPECT_TRUE(fetch_context->IsLoadComplete());
}

TEST_F(FrameFetchContextTest, PageDismissalEventBeingDispatchedWhenDetached) {
  dummy_page_holder = nullptr;

  EXPECT_FALSE(fetch_context->PageDismissalEventBeingDispatched());
}

TEST_F(FrameFetchContextTest, UpdateTimingInfoForIFrameNavigationWhenDetached) {
  RefPtr<ResourceTimingInfo> info =
      ResourceTimingInfo::Create("type", 0.3, false);

  dummy_page_holder = nullptr;

  fetch_context->UpdateTimingInfoForIFrameNavigation(info.Get());
  // Should not crash.
}

TEST_F(FrameFetchContextTest, SendImagePingWhenDetached) {
  KURL url(NullURL(), "https://www.example.com/");

  dummy_page_holder = nullptr;

  fetch_context->SendImagePing(url);
  // Should not crash. Nothing should be sent.
}

TEST_F(FrameFetchContextTest, AddConsoleMessageWhenDetached) {
  dummy_page_holder = nullptr;

  fetch_context->AddConsoleMessage("foobar");
  // Should not crash.
}

TEST_F(FrameFetchContextTest, GetSecurityOriginWhenDetached) {
  RefPtr<SecurityOrigin> origin =
      SecurityOrigin::Create(KURL(NullURL(), "https://www.example.com"));
  document->SetSecurityOrigin(origin);

  dummy_page_holder = nullptr;
  EXPECT_EQ(origin.Get(), fetch_context->GetSecurityOrigin());
}

TEST_F(FrameFetchContextTest, PopulateResourceRequestWhenDetached) {
  KURL url(NullURL(), "https://www.example.com/");
  ResourceRequest request(url);
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);

  ClientHintsPreferences client_hints_preferences;
  client_hints_preferences.SetShouldSendForTesting(
      kWebClientHintsTypeDeviceRam);
  client_hints_preferences.SetShouldSendForTesting(kWebClientHintsTypeDpr);
  client_hints_preferences.SetShouldSendForTesting(
      kWebClientHintsTypeResourceWidth);
  client_hints_preferences.SetShouldSendForTesting(
      kWebClientHintsTypeViewportWidth);

  FetchParameters::ResourceWidth resource_width;
  ResourceLoaderOptions options;

  document->GetClientHintsPreferences().SetShouldSendForTesting(
      kWebClientHintsTypeDeviceRam);
  document->GetClientHintsPreferences().SetShouldSendForTesting(
      kWebClientHintsTypeDpr);
  document->GetClientHintsPreferences().SetShouldSendForTesting(
      kWebClientHintsTypeResourceWidth);
  document->GetClientHintsPreferences().SetShouldSendForTesting(
      kWebClientHintsTypeViewportWidth);

  dummy_page_holder = nullptr;

  fetch_context->PopulateResourceRequest(
      url, Resource::kRaw, client_hints_preferences, resource_width, options,
      SecurityViolationReportingPolicy::kReport, request);
  // Should not crash.
}

TEST_F(FrameFetchContextTest,
       SetFirstPartyCookieAndRequestorOriginWhenDetached) {
  KURL url(NullURL(), "https://www.example.com/hoge/fuga");
  ResourceRequest request(url);
  KURL document_url(NullURL(), "https://www2.example.com/foo/bar");
  RefPtr<SecurityOrigin> origin = SecurityOrigin::Create(document_url);

  document->SetSecurityOrigin(origin);
  document->SetURL(document_url);

  dummy_page_holder = nullptr;

  fetch_context->SetFirstPartyCookieAndRequestorOrigin(request);

  EXPECT_EQ(document_url, request.FirstPartyForCookies());
  EXPECT_EQ(origin, request.RequestorOrigin());
}

TEST_F(FrameFetchContextTest, ArchiveWhenDetached) {
  FetchContext* child_fetch_context = CreateChildFrame();

  dummy_page_holder = nullptr;
  child_frame->Detach(FrameDetachType::kRemove);
  child_frame = nullptr;

  EXPECT_EQ(nullptr, child_fetch_context->Archive());
}

// Tests if "Intervention" header is added for frame with Client Lo-Fi enabled.
TEST_F(FrameFetchContextMockedLocalFrameClientTest,
       ClientLoFiInterventionHeader) {
  // Verify header not added if Lo-Fi not active.
  EXPECT_CALL(*client, IsClientLoFiActiveForFrame())
      .WillRepeatedly(::testing::Return(false));
  ResourceRequest resource_request("http://www.example.com/style.css");
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ(g_null_atom, resource_request.HttpHeaderField("Intervention"));

  // Verify header is added if Lo-Fi is active.
  EXPECT_CALL(*client, IsClientLoFiActiveForFrame())
      .WillRepeatedly(::testing::Return(true));
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchSubresource);
  EXPECT_EQ(
      "<https://www.chromestatus.com/features/6072546726248448>; "
      "level=\"warning\"",
      resource_request.HttpHeaderField("Intervention"));

  // Verify appended to an existing "Intervention" header value.
  ResourceRequest resource_request2("http://www.example.com/getad.js");
  resource_request2.SetHTTPHeaderField("Intervention",
                                       "<https://otherintervention.org>");
  fetch_context->AddAdditionalRequestHeaders(resource_request2,
                                             kFetchSubresource);
  EXPECT_EQ(
      "<https://otherintervention.org>, "
      "<https://www.chromestatus.com/features/6072546726248448>; "
      "level=\"warning\"",
      resource_request2.HttpHeaderField("Intervention"));
}

}  // namespace blink
