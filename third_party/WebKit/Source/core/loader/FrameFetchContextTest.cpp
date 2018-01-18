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
#include "common/net/ip_address_space.mojom-blink.h"
#include "core/dom/Document.h"
#include "core/frame/FrameOwner.h"
#include "core/frame/FrameTypes.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/SubresourceFilter.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/loader/fetch/UniqueIdentifier.h"
#include "platform/loader/fetch/fetch_initiator_type_names.h"
#include "platform/loader/testing/MockResource.h"
#include "platform/network/NetworkStateNotifier.h"
#include "platform/testing/HistogramTester.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "public/platform/WebClientHintsType.h"
#include "public/platform/WebDocumentSubresourceFilter.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "services/network/public/interfaces/request_context_frame_type.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/common/device_memory/approximated_device_memory.h"

namespace blink {

using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;

class StubLocalFrameClientWithParent final : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClientWithParent* Create(Frame* parent) {
    return new StubLocalFrameClientWithParent(parent);
  }

  void Trace(blink::Visitor* visitor) override {
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
  MOCK_METHOD0(DidDisplayContentWithCertificateErrors, void());
  MOCK_METHOD2(DispatchDidLoadResourceFromMemoryCache,
               void(const ResourceRequest&, const ResourceResponse&));
  MOCK_METHOD0(UserAgent, String());
  MOCK_METHOD0(MayUseClientLoFiForImageRequests, bool());
  MOCK_CONST_METHOD0(GetPreviewsStateForFrame, WebURLRequest::PreviewsState());
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
  void SetUp() override { RecreateFetchContext(); }

  void RecreateFetchContext() {
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
        *document, std::make_unique<FixedPolicySubresourceFilter>(
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
    const KURL input_url("http://example.com/");
    ResourceRequest resource_request(input_url);
    resource_request.SetFetchCredentialsMode(
        network::mojom::FetchCredentialsMode::kOmit);
    ResourceLoaderOptions options;
    return fetch_context->CanRequest(
        Resource::kImage, resource_request, input_url, options,
        reporting_policy, FetchParameters::kUseDefaultOriginRestrictionForType,
        ResourceRequest::RedirectStatus::kNoRedirect);
  }

  int filtered_load_callback_counter_;
};

// This test class sets up a mock frame loader client.
class FrameFetchContextMockedLocalFrameClientTest
    : public FrameFetchContextTest {
 protected:
  void SetUp() override {
    url = KURL("https://example.test/foo");
    http_url = KURL("http://example.test/foo");
    main_resource_url = KURL("https://www.example.test");
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
  KURL http_url;
  KURL main_resource_url;

  Persistent<::testing::NiceMock<FrameFetchContextMockLocalFrameClient>> client;
};

class FrameFetchContextModifyRequestTest : public FrameFetchContextTest {
 public:
  FrameFetchContextModifyRequestTest()
      : example_origin(SecurityOrigin::Create(KURL("https://example.test/"))),
        secure_origin(SecurityOrigin::Create(
            KURL("https://secureorigin.test/image.png"))) {}

 protected:
  void ExpectUpgrade(const char* input, const char* expected) {
    ExpectUpgrade(input, WebURLRequest::kRequestContextScript,
                  network::mojom::RequestContextFrameType::kNone, expected);
  }

  void ExpectUpgrade(const char* input,
                     WebURLRequest::RequestContext request_context,
                     network::mojom::RequestContextFrameType frame_type,
                     const char* expected) {
    const KURL input_url(input);
    const KURL expected_url(expected);

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

  void ExpectUpgradeInsecureRequestHeader(
      const char* input,
      network::mojom::RequestContextFrameType frame_type,
      bool should_prefer) {
    const KURL input_url(input);

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
      network::mojom::RequestContextFrameType frame_type,
      const AtomicString& expected_required_csp) {
    const KURL input_url(input);
    ResourceRequest resource_request(input_url);
    resource_request.SetRequestContext(WebURLRequest::kRequestContextScript);
    resource_request.SetFrameType(frame_type);

    fetch_context->ModifyRequestForCSP(resource_request);

    EXPECT_EQ(expected_required_csp,
              resource_request.HttpHeaderField(HTTPNames::Sec_Required_CSP));
  }

  void SetFrameOwnerBasedOnFrameType(
      network::mojom::RequestContextFrameType frame_type,
      HTMLIFrameElement* iframe,
      const AtomicString& potential_value) {
    if (frame_type != network::mojom::RequestContextFrameType::kNested) {
      document->GetFrame()->SetOwner(nullptr);
      return;
    }

    iframe->setAttribute(HTMLNames::cspAttr, potential_value);
    document->GetFrame()->SetOwner(iframe);
  }

  scoped_refptr<const SecurityOrigin> example_origin;
  scoped_refptr<SecurityOrigin> secure_origin;
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

    // We always upgrade for FrameTypeNone.
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextScript,
                  network::mojom::RequestContextFrameType::kNone,
                  test.upgraded);

    // We never upgrade for FrameTypeNested. This is done on the browser
    // process.
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextScript,
                  network::mojom::RequestContextFrameType::kNested,
                  test.original);

    // We do not upgrade for FrameTypeTopLevel or FrameTypeAuxiliary...
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextScript,
                  network::mojom::RequestContextFrameType::kTopLevel,
                  test.original);
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextScript,
                  network::mojom::RequestContextFrameType::kAuxiliary,
                  test.original);

    // unless the request context is RequestContextForm.
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextForm,
                  network::mojom::RequestContextFrameType::kTopLevel,
                  test.upgraded);
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextForm,
                  network::mojom::RequestContextFrameType::kAuxiliary,
                  test.upgraded);

    // Or unless the host of the resource is in the document's
    // InsecureNavigationsSet:
    document->AddInsecureNavigationUpgrade(
        example_origin->Host().Impl()->GetHash());
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextScript,
                  network::mojom::RequestContextFrameType::kTopLevel,
                  test.upgraded);
    ExpectUpgrade(test.original, WebURLRequest::kRequestContextScript,
                  network::mojom::RequestContextFrameType::kAuxiliary,
                  test.upgraded);
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
    network::mojom::RequestContextFrameType frame_type;
    bool should_prefer;
  } tests[] = {{"http://example.test/page.html",
                network::mojom::RequestContextFrameType::kAuxiliary, true},
               {"http://example.test/page.html",
                network::mojom::RequestContextFrameType::kNested, true},
               {"http://example.test/page.html",
                network::mojom::RequestContextFrameType::kNone, false},
               {"http://example.test/page.html",
                network::mojom::RequestContextFrameType::kTopLevel, true},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kAuxiliary, true},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kNested, true},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kNone, false},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kTopLevel, true}};

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
    network::mojom::RequestContextFrameType frame_type;
  } tests[] = {{"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kAuxiliary},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kNested},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kNone},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kTopLevel}};

  HTMLIFrameElement* iframe = HTMLIFrameElement::Create(*document);
  const AtomicString& required_csp = AtomicString("default-src 'none'");
  const AtomicString& another_required_csp = AtomicString("default-src 'self'");

  for (const auto& test : tests) {
    SetFrameOwnerBasedOnFrameType(test.frame_type, iframe, required_csp);
    ExpectSetRequiredCSPRequestHeader(
        test.to_request, test.frame_type,
        test.frame_type == network::mojom::RequestContextFrameType::kNested
            ? required_csp
            : g_null_atom);

    SetFrameOwnerBasedOnFrameType(test.frame_type, iframe,
                                  another_required_csp);
    ExpectSetRequiredCSPRequestHeader(
        test.to_request, test.frame_type,
        test.frame_type == network::mojom::RequestContextFrameType::kNested
            ? another_required_csp
            : g_null_atom);
  }
}

class FrameFetchContextHintsTest : public FrameFetchContextTest {
 public:
  FrameFetchContextHintsTest() = default;

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

    const KURL input_url(input);
    ResourceRequest resource_request(input_url);

    fetch_context->AddClientHintsIfNecessary(hints_preferences, resource_width,
                                             resource_request);

    EXPECT_EQ(is_present ? String(header_value) : String(),
              resource_request.HttpHeaderField(header_name));
  }
};

// Verify that the client hints should be attached for subresources fetched
// over secure transport.
TEST_F(FrameFetchContextHintsTest, MonitorDeviceMemorySecureTransport) {
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "4");
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
}

// Verify that client hints are not attched when the resources do not belong to
// a secure context.
TEST_F(FrameFetchContextHintsTest, MonitorDeviceMemoryHintsInsecureContext) {
  ExpectHeader("http://www.example.com/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("http://www.example.com/1.gif", "Device-Memory", true, "4");
  ExpectHeader("http://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
}

// Verify that client hints are attched when the resources belong to a local
// context.
TEST_F(FrameFetchContextHintsTest, MonitorDeviceMemoryHintsLocalContext) {
  ExpectHeader("http://localhost/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("http://localhost/1.gif", "Device-Memory", true, "4");
  ExpectHeader("http://localhost/1.gif", "DPR", false, "");
  ExpectHeader("http://localhost/1.gif", "Width", false, "");
  ExpectHeader("http://localhost/1.gif", "Viewport-Width", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorDeviceMemoryHints) {
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "4");
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(2048);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "2");
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(64385);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "8");
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(768);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "0.5");
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorDPRHints) {
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDpr);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("https://www.example.com/1.gif", "DPR", true, "1");
  dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(2.5);
  ExpectHeader("https://www.example.com/1.gif", "DPR", true, "2.5");
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorDPRHintsInsecureTransport) {
  ExpectHeader("http://www.example.com/1.gif", "DPR", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDpr);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("http://www.example.com/1.gif", "DPR", true, "1");
  dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(2.5);
  ExpectHeader("http://www.example.com/1.gif", "DPR", true, "2.5");
  ExpectHeader("http://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorResourceWidthHints) {
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kResourceWidth);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "500", 500);
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "667", 666.6666);
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(2.5);
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "1250", 500);
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "1667",
               666.6666);
}

TEST_F(FrameFetchContextHintsTest, MonitorViewportWidthHints) {
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kViewportWidth);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", true, "500");
  dummy_page_holder->GetFrameView().SetLayoutSizeFixedToFrameSize(false);
  dummy_page_holder->GetFrameView().SetLayoutSize(IntSize(800, 800));
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", true, "800");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", true, "800",
               666.6666);
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorAllHints) {
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", false, "");
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");

  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDpr);
  preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kResourceWidth);
  preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kViewportWidth);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  document->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "4");
  ExpectHeader("https://www.example.com/1.gif", "DPR", true, "1");
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "400", 400);
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", true, "500");
}

TEST_F(FrameFetchContextTest, MainResourceCachePolicy) {
  // Default case
  ResourceRequest request("http://www.example.com");
  EXPECT_EQ(mojom::FetchCacheMode::kDefault,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Post
  ResourceRequest post_request("http://www.example.com");
  post_request.SetHTTPMethod(HTTPNames::POST);
  EXPECT_EQ(
      mojom::FetchCacheMode::kValidateCache,
      fetch_context->ResourceRequestCachePolicy(
          post_request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Re-post
  document->Loader()->SetLoadType(kFrameLoadTypeBackForward);
  EXPECT_EQ(
      mojom::FetchCacheMode::kOnlyIfCached,
      fetch_context->ResourceRequestCachePolicy(
          post_request, Resource::kMainResource, FetchParameters::kNoDefer));

  // FrameLoadTypeReload
  document->Loader()->SetLoadType(kFrameLoadTypeReload);
  EXPECT_EQ(mojom::FetchCacheMode::kValidateCache,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Conditional request
  document->Loader()->SetLoadType(kFrameLoadTypeStandard);
  ResourceRequest conditional("http://www.example.com");
  conditional.SetHTTPHeaderField(HTTPNames::If_Modified_Since, "foo");
  EXPECT_EQ(
      mojom::FetchCacheMode::kValidateCache,
      fetch_context->ResourceRequestCachePolicy(
          conditional, Resource::kMainResource, FetchParameters::kNoDefer));

  // FrameLoadTypeReloadBypassingCache
  document->Loader()->SetLoadType(kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(mojom::FetchCacheMode::kBypassCache,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));

  // FrameLoadTypeReloadBypassingCache with a conditional request
  document->Loader()->SetLoadType(kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(
      mojom::FetchCacheMode::kBypassCache,
      fetch_context->ResourceRequestCachePolicy(
          conditional, Resource::kMainResource, FetchParameters::kNoDefer));

  // FrameLoadTypeReloadBypassingCache with a post request
  document->Loader()->SetLoadType(kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(
      mojom::FetchCacheMode::kBypassCache,
      fetch_context->ResourceRequestCachePolicy(
          post_request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Set up a child frame
  FrameFetchContext* child_fetch_context = CreateChildFrame();

  // Child frame as part of back/forward
  document->Loader()->SetLoadType(kFrameLoadTypeBackForward);
  EXPECT_EQ(mojom::FetchCacheMode::kForceCache,
            child_fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Child frame as part of reload
  document->Loader()->SetLoadType(kFrameLoadTypeReload);
  EXPECT_EQ(mojom::FetchCacheMode::kDefault,
            child_fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Child frame as part of reload bypassing cache
  document->Loader()->SetLoadType(kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(mojom::FetchCacheMode::kBypassCache,
            child_fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMainResource, FetchParameters::kNoDefer));

  // Per-frame bypassing reload, but parent load type is different.
  // This is not the case users can trigger through user interfaces, but for
  // checking code correctness and consistency.
  document->Loader()->SetLoadType(kFrameLoadTypeReload);
  child_frame->Loader().GetDocumentLoader()->SetLoadType(
      kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(mojom::FetchCacheMode::kBypassCache,
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
  EXPECT_EQ(mojom::FetchCacheMode::kDefault,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMock, FetchParameters::kNoDefer));

  // FrameLoadTypeReload should not affect sub-resources
  document->Loader()->SetLoadType(kFrameLoadTypeReload);
  EXPECT_EQ(mojom::FetchCacheMode::kDefault,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMock, FetchParameters::kNoDefer));

  // Conditional request
  document->Loader()->SetLoadType(kFrameLoadTypeStandard);
  ResourceRequest conditional("http://www.example.com/mock");
  conditional.SetHTTPHeaderField(HTTPNames::If_Modified_Since, "foo");
  EXPECT_EQ(mojom::FetchCacheMode::kValidateCache,
            fetch_context->ResourceRequestCachePolicy(
                conditional, Resource::kMock, FetchParameters::kNoDefer));

  // FrameLoadTypeReloadBypassingCache
  document->Loader()->SetLoadType(kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(mojom::FetchCacheMode::kBypassCache,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMock, FetchParameters::kNoDefer));

  // FrameLoadTypeReloadBypassingCache with a conditional request
  document->Loader()->SetLoadType(kFrameLoadTypeReloadBypassingCache);
  EXPECT_EQ(mojom::FetchCacheMode::kBypassCache,
            fetch_context->ResourceRequestCachePolicy(
                conditional, Resource::kMock, FetchParameters::kNoDefer));

  // Back/forward navigation
  document->Loader()->SetLoadType(kFrameLoadTypeBackForward);
  EXPECT_EQ(mojom::FetchCacheMode::kForceCache,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kMock, FetchParameters::kNoDefer));

  // Back/forward navigation with a conditional request
  document->Loader()->SetLoadType(kFrameLoadTypeBackForward);
  EXPECT_EQ(mojom::FetchCacheMode::kForceCache,
            fetch_context->ResourceRequestCachePolicy(
                conditional, Resource::kMock, FetchParameters::kNoDefer));
}

TEST_F(FrameFetchContextTest, SetFirstPartyCookieAndRequestorOrigin) {
  struct TestCase {
    const char* document_url;
    bool document_sandboxed;
    const char* requestor_origin;  // "" => null
    network::mojom::RequestContextFrameType frame_type;
    const char* serialized_origin;  // "" => null
  } cases[] = {
      // No document origin => unique request origin
      {"", false, "", network::mojom::RequestContextFrameType::kNone, "null"},
      {"", true, "", network::mojom::RequestContextFrameType::kNone, "null"},

      // Document origin => request origin
      {"http://example.test", false, "",
       network::mojom::RequestContextFrameType::kNone, "http://example.test"},
      {"http://example.test", true, "",
       network::mojom::RequestContextFrameType::kNone, "http://example.test"},

      // If the request already has a requestor origin, then
      // 'SetFirstPartyCookieAndRequestorOrigin' leaves it alone:
      {"http://example.test", false, "http://not-example.test",
       network::mojom::RequestContextFrameType::kNone,
       "http://not-example.test"},
      {"http://example.test", true, "http://not-example.test",
       network::mojom::RequestContextFrameType::kNone,
       "http://not-example.test"},
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
    document->SetURL(KURL(test.document_url));
    document->SetSecurityOrigin(SecurityOrigin::Create(document->Url()));

    if (test.document_sandboxed)
      document->EnforceSandboxFlags(kSandboxOrigin);

    ResourceRequest request("http://example.test/");
    request.SetFrameType(test.frame_type);
    if (strlen(test.requestor_origin) > 0) {
      request.SetRequestorOrigin(
          SecurityOrigin::Create(KURL(test.requestor_origin)));
    }

    // Compare the populated |requestorOrigin| against |test.serializedOrigin|
    fetch_context->SetFirstPartyCookieAndRequestorOrigin(request);
    if (strlen(test.serialized_origin) == 0) {
      EXPECT_TRUE(!request.RequestorOrigin());
    } else {
      EXPECT_EQ(String(test.serialized_origin),
                request.RequestorOrigin()->ToString());
    }

    EXPECT_EQ(document->SiteForCookies(), request.SiteForCookies());
  }
}

TEST_F(FrameFetchContextTest, ModifyPriorityForLowPriorityIframes) {
  Settings* settings = document->GetSettings();
  FrameFetchContext* childFetchContext = CreateChildFrame();
  GetNetworkStateNotifier().SetNetworkQualityInfoOverride(
      WebEffectiveConnectionType::kType3G, 1 /* transport_rtt_msec */,
      10000 /* downlink_throughput_mbps */);

  // Experiment is not enabled, expect default values.
  EXPECT_EQ(ResourceLoadPriority::kVeryHigh,
            fetch_context->ModifyPriorityForExperiments(
                ResourceLoadPriority::kVeryHigh));
  EXPECT_EQ(ResourceLoadPriority::kVeryHigh,
            childFetchContext->ModifyPriorityForExperiments(
                ResourceLoadPriority::kVeryHigh));
  EXPECT_EQ(ResourceLoadPriority::kMedium,
            childFetchContext->ModifyPriorityForExperiments(
                ResourceLoadPriority::kMedium));

  // Low priority iframes enabled but network is not slow enough. Expect default
  // values.
  settings->SetLowPriorityIframesThreshold(WebEffectiveConnectionType::kType2G);
  EXPECT_EQ(ResourceLoadPriority::kVeryHigh,
            fetch_context->ModifyPriorityForExperiments(
                ResourceLoadPriority::kVeryHigh));
  EXPECT_EQ(ResourceLoadPriority::kVeryHigh,
            childFetchContext->ModifyPriorityForExperiments(
                ResourceLoadPriority::kVeryHigh));
  EXPECT_EQ(ResourceLoadPriority::kMedium,
            childFetchContext->ModifyPriorityForExperiments(
                ResourceLoadPriority::kMedium));

  // Low priority iframes enabled and network is slow, main frame request's
  // priorities should not change.
  GetNetworkStateNotifier().SetNetworkQualityInfoOverride(
      WebEffectiveConnectionType::kType2G, 1 /* transport_rtt_msec */,
      10000 /* downlink_throughput_mbps */);
  EXPECT_EQ(ResourceLoadPriority::kVeryHigh,
            fetch_context->ModifyPriorityForExperiments(
                ResourceLoadPriority::kVeryHigh));
  // Low priority iframes enabled, everything in child frame should be low
  // priority.
  EXPECT_EQ(ResourceLoadPriority::kVeryLow,
            childFetchContext->ModifyPriorityForExperiments(
                ResourceLoadPriority::kVeryHigh));
  EXPECT_EQ(ResourceLoadPriority::kVeryLow,
            childFetchContext->ModifyPriorityForExperiments(
                ResourceLoadPriority::kMedium));
}

// Tests if "Save-Data" header is correctly added on the first load and reload.
TEST_F(FrameFetchContextTest, EnableDataSaver) {
  GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);
  // Recreate the fetch context so that the updated save data settings are read.
  RecreateFetchContext();

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
  GetNetworkStateNotifier().SetSaveDataEnabledOverride(false);
  // Recreate the fetch context so that the updated save data settings are read.
  RecreateFetchContext();

  ResourceRequest resource_request("http://www.example.com");
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ(String(), resource_request.HttpHeaderField("Save-Data"));
}

// Tests if reload variants can reflect the current data saver setting.
TEST_F(FrameFetchContextTest, ChangeDataSaverConfig) {
  GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);
  // Recreate the fetch context so that the updated save data settings are read.
  RecreateFetchContext();
  ResourceRequest resource_request("http://www.example.com");
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ("on", resource_request.HttpHeaderField("Save-Data"));

  GetNetworkStateNotifier().SetSaveDataEnabledOverride(false);
  RecreateFetchContext();
  document->Loader()->SetLoadType(kFrameLoadTypeReload);
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ(String(), resource_request.HttpHeaderField("Save-Data"));

  GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);
  RecreateFetchContext();
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ("on", resource_request.HttpHeaderField("Save-Data"));

  GetNetworkStateNotifier().SetSaveDataEnabledOverride(false);
  RecreateFetchContext();
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
      network::mojom::FetchCredentialsMode::kOmit);
  Resource* resource = MockResource::Create(resource_request);
  EXPECT_CALL(*client,
              DispatchDidLoadResourceFromMemoryCache(
                  ::testing::AllOf(
                      ::testing::Property(&ResourceRequest::Url, url),
                      ::testing::Property(
                          &ResourceRequest::GetFrameType,
                          network::mojom::RequestContextFrameType::kNone),
                      ::testing::Property(&ResourceRequest::GetRequestContext,
                                          WebURLRequest::kRequestContextImage)),
                  ResourceResponse()));
  fetch_context->DispatchDidLoadResourceFromMemoryCache(
      CreateUniqueIdentifier(), resource_request, resource->GetResponse());
}

// Tests that the client hints lifetime header is parsed correctly only when the
// frame belongs to a secure context.
TEST_F(FrameFetchContextMockedLocalFrameClientTest,
       PersistClientHintsSecureContext) {
  HistogramTester histogram_tester;

  {
    ResourceRequest resource_request(url);
    resource_request.SetRequestContext(WebURLRequest::kRequestContextImage);
    resource_request.SetFetchCredentialsMode(
        network::mojom::FetchCredentialsMode::kOmit);
    ResourceResponse response(url);
    response.SetHTTPHeaderField("accept-ch", "dpr");
    response.SetHTTPHeaderField("accept-ch-lifetime", "3600");
    Resource* resource = MockResource::Create(resource_request);
    resource->SetResponse(response);
    fetch_context->DispatchDidReceiveResponse(
        CreateUniqueIdentifier(), response, resource_request.GetFrameType(),
        resource_request.GetRequestContext(), resource,
        FetchContext::ResourceResponseType::kNotFromMemoryCache);

    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        static_cast<int>(WebFeature::kPersistentClientHintHeader), 1);
  }

  {
    // Next, try with a HTTP URL.
    ResourceRequest resource_request(http_url);
    resource_request.SetRequestContext(WebURLRequest::kRequestContextImage);
    resource_request.SetFetchCredentialsMode(
        network::mojom::FetchCredentialsMode::kOmit);

    ResourceResponse response(http_url);
    response.SetHTTPHeaderField("accept-ch", "dpr");
    response.SetHTTPHeaderField("accept-ch-lifetime", "3600");
    Resource* resource = MockResource::Create(resource_request);
    resource->SetResponse(response);
    fetch_context->DispatchDidReceiveResponse(
        CreateUniqueIdentifier(), response, resource_request.GetFrameType(),
        resource_request.GetRequestContext(), resource,
        FetchContext::ResourceResponseType::kNotFromMemoryCache);

    // There should not be a change in the usage count.
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        static_cast<int>(WebFeature::kPersistentClientHintHeader), 1);
  }
}

// Tests that when a resource with certificate errors is loaded from the memory
// cache, the embedder is notified.
TEST_F(FrameFetchContextMockedLocalFrameClientTest,
       MemoryCacheCertificateError) {
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextImage);
  resource_request.SetFetchCredentialsMode(
      network::mojom::FetchCredentialsMode::kOmit);
  ResourceResponse response(url);
  response.SetHasMajorCertificateErrors(true);
  Resource* resource = MockResource::Create(resource_request);
  resource->SetResponse(response);
  EXPECT_CALL(*client, DidDisplayContentWithCertificateErrors());
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
  const KURL document_url("https://www2.example.com/fuga/hoge.html");
  const String origin = "https://www2.example.com";
  ResourceRequest request(KURL("https://localhost/"));
  request.SetHTTPMethod("PUT");

  GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);
  document->SetSecurityOrigin(SecurityOrigin::Create(KURL(origin)));
  document->SetURL(document_url);
  document->SetReferrerPolicy(kReferrerPolicyOrigin);
  document->SetAddressSpace(mojom::IPAddressSpace::kPublic);

  dummy_page_holder = nullptr;

  fetch_context->AddAdditionalRequestHeaders(request, kFetchSubresource);

  EXPECT_EQ(origin, request.HttpHeaderField(HTTPNames::Origin));
  EXPECT_EQ(String(origin + "/"), request.HttpHeaderField(HTTPNames::Referer));
  EXPECT_EQ(String(), request.HttpHeaderField("Save-Data"));
}

TEST_F(FrameFetchContextTest, ResourceRequestCachePolicyWhenDetached) {
  ResourceRequest request(KURL("https://localhost/"));

  dummy_page_holder = nullptr;

  EXPECT_EQ(mojom::FetchCacheMode::kDefault,
            fetch_context->ResourceRequestCachePolicy(
                request, Resource::kRaw, FetchParameters::kNoDefer));
}

TEST_F(FrameFetchContextTest, DispatchDidChangePriorityWhenDetached) {
  dummy_page_holder = nullptr;

  fetch_context->DispatchDidChangeResourcePriority(
      2, ResourceLoadPriority::kLow, 3);
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

  ResourceRequest request(KURL("https://localhost/"));
  fetch_context->PrepareRequest(request,
                                FetchContext::RedirectType::kNotForRedirect);

  EXPECT_EQ("hi", request.HttpHeaderField(HTTPNames::User_Agent));
}

TEST_F(FrameFetchContextTest, DispatchWillSendRequestWhenDetached) {
  ResourceRequest request(KURL("https://www.example.com/"));
  ResourceResponse response;
  FetchInitiatorInfo initiator_info;

  dummy_page_holder = nullptr;

  fetch_context->DispatchWillSendRequest(1, request, response, Resource::kRaw,
                                         initiator_info);
  // Should not crash.
}

TEST_F(FrameFetchContextTest,
       DispatchDidLoadResourceFromMemoryCacheWhenDetached) {
  ResourceRequest request(KURL("https://www.example.com/"));
  ResourceResponse response;
  FetchInitiatorInfo initiator_info;

  dummy_page_holder = nullptr;

  fetch_context->DispatchDidLoadResourceFromMemoryCache(8, request, response);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, DispatchDidReceiveResponseWhenDetached) {
  ResourceRequest request(KURL("https://www.example.com/"));
  request.SetFetchCredentialsMode(network::mojom::FetchCredentialsMode::kOmit);
  Resource* resource = MockResource::Create(request);
  ResourceResponse response;

  dummy_page_holder = nullptr;

  fetch_context->DispatchDidReceiveResponse(
      3, response, network::mojom::RequestContextFrameType::kTopLevel,
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

  fetch_context->DispatchDidFinishLoading(4, 0.3, 8, 10, false);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, DispatchDidFailWhenDetached) {
  dummy_page_holder = nullptr;

  fetch_context->DispatchDidFail(8, ResourceError::Failure(NullURL()), 5,
                                 false);
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
  ResourceRequest request(KURL("https://www.example.com/"));

  dummy_page_holder = nullptr;

  fetch_context->RecordLoadingActivity(request, Resource::kRaw,
                                       FetchInitiatorTypeNames::xmlhttprequest);
  // Should not crash.

  fetch_context->RecordLoadingActivity(request, Resource::kRaw,
                                       FetchInitiatorTypeNames::document);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, DidLoadResourceWhenDetached) {
  ResourceRequest request(KURL("https://www.example.com/"));
  request.SetFetchCredentialsMode(network::mojom::FetchCredentialsMode::kOmit);
  Resource* resource = MockResource::Create(request);

  dummy_page_holder = nullptr;

  fetch_context->DidLoadResource(resource);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, AddResourceTimingWhenDetached) {
  scoped_refptr<ResourceTimingInfo> info =
      ResourceTimingInfo::Create("type", 0.3, false);

  dummy_page_holder = nullptr;

  fetch_context->AddResourceTiming(*info);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, AllowImageWhenDetached) {
  const KURL url("https://www.example.com/");

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

TEST_F(FrameFetchContextTest, UpdateTimingInfoForIFrameNavigationWhenDetached) {
  scoped_refptr<ResourceTimingInfo> info =
      ResourceTimingInfo::Create("type", 0.3, false);

  dummy_page_holder = nullptr;

  fetch_context->UpdateTimingInfoForIFrameNavigation(info.get());
  // Should not crash.
}

TEST_F(FrameFetchContextTest, AddConsoleMessageWhenDetached) {
  dummy_page_holder = nullptr;

  fetch_context->AddWarningConsoleMessage("foobar", FetchContext::kJSSource);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, GetSecurityOriginWhenDetached) {
  scoped_refptr<SecurityOrigin> origin =
      SecurityOrigin::Create(KURL("https://www.example.com"));
  document->SetSecurityOrigin(origin);

  dummy_page_holder = nullptr;
  EXPECT_EQ(origin.get(), fetch_context->GetSecurityOrigin());
}

TEST_F(FrameFetchContextTest, PopulateResourceRequestWhenDetached) {
  const KURL url("https://www.example.com/");
  ResourceRequest request(url);
  request.SetFetchCredentialsMode(network::mojom::FetchCredentialsMode::kOmit);

  ClientHintsPreferences client_hints_preferences;
  client_hints_preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kDeviceMemory);
  client_hints_preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kDpr);
  client_hints_preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kResourceWidth);
  client_hints_preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kViewportWidth);

  FetchParameters::ResourceWidth resource_width;
  ResourceLoaderOptions options;

  document->GetClientHintsPreferences().SetShouldSendForTesting(
      mojom::WebClientHintsType::kDeviceMemory);
  document->GetClientHintsPreferences().SetShouldSendForTesting(
      mojom::WebClientHintsType::kDpr);
  document->GetClientHintsPreferences().SetShouldSendForTesting(
      mojom::WebClientHintsType::kResourceWidth);
  document->GetClientHintsPreferences().SetShouldSendForTesting(
      mojom::WebClientHintsType::kViewportWidth);

  dummy_page_holder = nullptr;

  fetch_context->PopulateResourceRequest(
      Resource::kRaw, client_hints_preferences, resource_width, request);
  // Should not crash.
}

TEST_F(FrameFetchContextTest,
       SetFirstPartyCookieAndRequestorOriginWhenDetached) {
  const KURL url("https://www.example.com/hoge/fuga");
  ResourceRequest request(url);
  const KURL document_url("https://www2.example.com/foo/bar");
  scoped_refptr<SecurityOrigin> origin = SecurityOrigin::Create(document_url);

  document->SetSecurityOrigin(origin);
  document->SetURL(document_url);

  dummy_page_holder = nullptr;

  fetch_context->SetFirstPartyCookieAndRequestorOrigin(request);

  EXPECT_EQ(document_url, request.SiteForCookies());
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
  EXPECT_CALL(*client, GetPreviewsStateForFrame())
      .WillRepeatedly(::testing::Return(WebURLRequest::kPreviewsOff));
  ResourceRequest resource_request("http://www.example.com/style.css");
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ(g_null_atom, resource_request.HttpHeaderField("Intervention"));

  // Verify header is added if Lo-Fi is active.
  EXPECT_CALL(*client, GetPreviewsStateForFrame())
      .WillRepeatedly(::testing::Return(WebURLRequest::kClientLoFiOn));
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

// Tests if "Intervention" header is added for frame with NoScript enabled.
TEST_F(FrameFetchContextMockedLocalFrameClientTest,
       NoScriptInterventionHeader) {
  // Verify header not added if NoScript not active.
  EXPECT_CALL(*client, GetPreviewsStateForFrame())
      .WillRepeatedly(::testing::Return(WebURLRequest::kPreviewsOff));
  ResourceRequest resource_request("http://www.example.com/style.css");
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchMainResource);
  EXPECT_EQ(g_null_atom, resource_request.HttpHeaderField("Intervention"));

  // Verify header is added if NoScript is active.
  EXPECT_CALL(*client, GetPreviewsStateForFrame())
      .WillRepeatedly(::testing::Return(WebURLRequest::kNoScriptOn));
  fetch_context->AddAdditionalRequestHeaders(resource_request,
                                             kFetchSubresource);
  EXPECT_EQ(
      "<https://www.chromestatus.com/features/4775088607985664>; "
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
      "<https://www.chromestatus.com/features/4775088607985664>; "
      "level=\"warning\"",
      resource_request2.HttpHeaderField("Intervention"));
}

}  // namespace blink
