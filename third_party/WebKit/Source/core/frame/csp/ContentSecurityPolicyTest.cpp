// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/ContentSecurityPolicy.h"

#include "core/frame/csp/CSPDirectiveList.h"
#include "core/html/HTMLScriptElement.h"
#include "core/testing/NullExecutionContext.h"
#include "platform/Crypto.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ContentSecurityPolicyTest : public ::testing::Test {
 public:
  ContentSecurityPolicyTest()
      : csp(ContentSecurityPolicy::Create()),
        secure_url(kParsedURLString, "https://example.test/image.png"),
        secure_origin(SecurityOrigin::Create(secure_url)) {}

 protected:
  virtual void SetUp() { execution_context = CreateExecutionContext(); }

  NullExecutionContext* CreateExecutionContext() {
    NullExecutionContext* context = new NullExecutionContext();
    context->SetUpSecurityContext();
    context->SetSecurityOrigin(secure_origin);
    return context;
  }

  Persistent<ContentSecurityPolicy> csp;
  KURL secure_url;
  RefPtr<SecurityOrigin> secure_origin;
  Persistent<NullExecutionContext> execution_context;
};

TEST_F(ContentSecurityPolicyTest, ParseInsecureRequestPolicy) {
  struct TestCase {
    const char* header;
    WebInsecureRequestPolicy expected_policy;
  } cases[] = {{"default-src 'none'", kLeaveInsecureRequestsAlone},
               {"upgrade-insecure-requests", kUpgradeInsecureRequests},
               {"block-all-mixed-content", kBlockAllMixedContent},
               {"upgrade-insecure-requests; block-all-mixed-content",
                kUpgradeInsecureRequests | kBlockAllMixedContent},
               {"upgrade-insecure-requests, block-all-mixed-content",
                kUpgradeInsecureRequests | kBlockAllMixedContent}};

  // Enforced
  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "[Enforce] Header: `" << test.header << "`");
    csp = ContentSecurityPolicy::Create();
    csp->DidReceiveHeader(test.header, kContentSecurityPolicyHeaderTypeEnforce,
                          kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.expected_policy, csp->GetInsecureRequestPolicy());

    execution_context = CreateExecutionContext();
    execution_context->SetSecurityOrigin(secure_origin);
    execution_context->SetURL(secure_url);
    csp->BindToExecutionContext(execution_context.Get());
    EXPECT_EQ(test.expected_policy,
              execution_context->GetInsecureRequestPolicy());
    bool expect_upgrade = test.expected_policy & kUpgradeInsecureRequests;
    EXPECT_EQ(expect_upgrade,
              execution_context->InsecureNavigationsToUpgrade()->Contains(
                  execution_context->Url().Host().Impl()->GetHash()));
  }

  // Report-Only
  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "[Report-Only] Header: `" << test.header << "`");
    csp = ContentSecurityPolicy::Create();
    csp->DidReceiveHeader(test.header, kContentSecurityPolicyHeaderTypeReport,
                          kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(kLeaveInsecureRequestsAlone, csp->GetInsecureRequestPolicy());

    execution_context = CreateExecutionContext();
    execution_context->SetSecurityOrigin(secure_origin);
    csp->BindToExecutionContext(execution_context.Get());
    EXPECT_EQ(kLeaveInsecureRequestsAlone,
              execution_context->GetInsecureRequestPolicy());
    EXPECT_FALSE(execution_context->InsecureNavigationsToUpgrade()->Contains(
        secure_origin->Host().Impl()->GetHash()));
  }
}

TEST_F(ContentSecurityPolicyTest, ParseEnforceTreatAsPublicAddressDisabled) {
  RuntimeEnabledFeatures::SetCorsRFC1918Enabled(false);
  execution_context->SetAddressSpace(kWebAddressSpacePrivate);
  EXPECT_EQ(kWebAddressSpacePrivate, execution_context->AddressSpace());

  csp->DidReceiveHeader("treat-as-public-address",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  csp->BindToExecutionContext(execution_context.Get());
  EXPECT_EQ(kWebAddressSpacePrivate, execution_context->AddressSpace());
}

TEST_F(ContentSecurityPolicyTest, ParseEnforceTreatAsPublicAddressEnabled) {
  RuntimeEnabledFeatures::SetCorsRFC1918Enabled(true);
  execution_context->SetAddressSpace(kWebAddressSpacePrivate);
  EXPECT_EQ(kWebAddressSpacePrivate, execution_context->AddressSpace());

  csp->DidReceiveHeader("treat-as-public-address",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  csp->BindToExecutionContext(execution_context.Get());
  EXPECT_EQ(kWebAddressSpacePublic, execution_context->AddressSpace());
}

TEST_F(ContentSecurityPolicyTest, CopyStateFrom) {
  csp->DidReceiveHeader("script-src 'none'; plugin-types application/x-type-1",
                        kContentSecurityPolicyHeaderTypeReport,
                        kContentSecurityPolicyHeaderSourceHTTP);
  csp->DidReceiveHeader("img-src http://example.com",
                        kContentSecurityPolicyHeaderTypeReport,
                        kContentSecurityPolicyHeaderSourceHTTP);

  KURL example_url(NullURL(), "http://example.com");
  KURL not_example_url(NullURL(), "http://not-example.com");

  ContentSecurityPolicy* csp2 = ContentSecurityPolicy::Create();
  csp2->CopyStateFrom(csp.Get());
  EXPECT_FALSE(csp2->AllowScriptFromSource(
      example_url, String(), IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
  EXPECT_TRUE(csp2->AllowPluginType(
      "application/x-type-1", "application/x-type-1", example_url,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(csp2->AllowImageFromSource(
      example_url, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
  EXPECT_FALSE(csp2->AllowImageFromSource(
      not_example_url, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
  EXPECT_FALSE(csp2->AllowPluginType(
      "application/x-type-2", "application/x-type-2", example_url,
      SecurityViolationReportingPolicy::kSuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, CopyPluginTypesFrom) {
  csp->DidReceiveHeader("script-src 'none'; plugin-types application/x-type-1",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  csp->DidReceiveHeader("img-src http://example.com",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);

  KURL example_url(NullURL(), "http://example.com");
  KURL not_example_url(NullURL(), "http://not-example.com");

  ContentSecurityPolicy* csp2 = ContentSecurityPolicy::Create();
  csp2->CopyPluginTypesFrom(csp.Get());
  EXPECT_TRUE(csp2->AllowScriptFromSource(
      example_url, String(), IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(csp2->AllowPluginType(
      "application/x-type-1", "application/x-type-1", example_url,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(csp2->AllowImageFromSource(
      example_url, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(csp2->AllowImageFromSource(
      not_example_url, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(csp2->AllowPluginType(
      "application/x-type-2", "application/x-type-2", example_url,
      SecurityViolationReportingPolicy::kSuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, IsFrameAncestorsEnforced) {
  csp->DidReceiveHeader("script-src 'none';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->IsFrameAncestorsEnforced());

  csp->DidReceiveHeader("frame-ancestors 'self'",
                        kContentSecurityPolicyHeaderTypeReport,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->IsFrameAncestorsEnforced());

  csp->DidReceiveHeader("frame-ancestors 'self'",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->IsFrameAncestorsEnforced());
}

// Tests that frame-ancestors directives are discarded from policies
// delivered in <meta> elements.
TEST_F(ContentSecurityPolicyTest, FrameAncestorsInMeta) {
  csp->BindToExecutionContext(execution_context.Get());
  csp->DidReceiveHeader("frame-ancestors 'none';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(csp->IsFrameAncestorsEnforced());
  csp->DidReceiveHeader("frame-ancestors 'none';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->IsFrameAncestorsEnforced());
}

// Tests that sandbox directives are discarded from policies
// delivered in <meta> elements.
TEST_F(ContentSecurityPolicyTest, SandboxInMeta) {
  csp->BindToExecutionContext(execution_context.Get());
  csp->DidReceiveHeader("sandbox;", kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(execution_context->GetSecurityOrigin()->IsUnique());
  csp->DidReceiveHeader("sandbox;", kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(execution_context->GetSecurityOrigin()->IsUnique());
}

// Tests that report-uri directives are discarded from policies
// delivered in <meta> elements.
TEST_F(ContentSecurityPolicyTest, ReportURIInMeta) {
  String policy = "img-src 'none'; report-uri http://foo.test";
  Vector<UChar> characters;
  policy.AppendTo(characters);
  const UChar* begin = characters.data();
  const UChar* end = begin + characters.size();
  CSPDirectiveList* directive_list(CSPDirectiveList::Create(
      csp, begin, end, kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceMeta));
  EXPECT_TRUE(directive_list->ReportEndpoints().IsEmpty());
  directive_list = CSPDirectiveList::Create(
      csp, begin, end, kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(directive_list->ReportEndpoints().IsEmpty());
}

// Tests that object-src directives are applied to a request to load a
// plugin, but not to subresource requests that the plugin itself
// makes. https://crbug.com/603952
TEST_F(ContentSecurityPolicyTest, ObjectSrc) {
  KURL url(NullURL(), "https://example.test");
  csp->BindToExecutionContext(execution_context.Get());
  csp->DidReceiveHeader("object-src 'none';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(
      csp->AllowRequest(WebURLRequest::kRequestContextObject, url, String(),
                        IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(
      csp->AllowRequest(WebURLRequest::kRequestContextEmbed, url, String(),
                        IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(
      csp->AllowRequest(WebURLRequest::kRequestContextPlugin, url, String(),
                        IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, ConnectSrc) {
  KURL url(NullURL(), "https://example.test");
  csp->BindToExecutionContext(execution_context.Get());
  csp->DidReceiveHeader("connect-src 'none';",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(
      csp->AllowRequest(WebURLRequest::kRequestContextSubresource, url,
                        String(), IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(
      csp->AllowRequest(WebURLRequest::kRequestContextXMLHttpRequest, url,
                        String(), IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(
      csp->AllowRequest(WebURLRequest::kRequestContextBeacon, url, String(),
                        IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(
      csp->AllowRequest(WebURLRequest::kRequestContextFetch, url, String(),
                        IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(
      csp->AllowRequest(WebURLRequest::kRequestContextPlugin, url, String(),
                        IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));
}
// Tests that requests for scripts and styles are blocked
// if `require-sri-for` delivered in HTTP header requires integrity be present
TEST_F(ContentSecurityPolicyTest, RequireSRIForInHeaderMissingIntegrity) {
  KURL url(NullURL(), "https://example.test");
  // Enforce
  Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::Create();
  policy->BindToExecutionContext(execution_context.Get());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeEnforce,
                           kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(policy->AllowRequest(
      WebURLRequest::kRequestContextScript, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      WebURLRequest::kRequestContextImport, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      WebURLRequest::kRequestContextStyle, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      WebURLRequest::kRequestContextServiceWorker, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      WebURLRequest::kRequestContextSharedWorker, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      WebURLRequest::kRequestContextWorker, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImage, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  // Report
  policy = ContentSecurityPolicy::Create();
  policy->BindToExecutionContext(execution_context.Get());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeReport,
                           kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextScript, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImport, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextStyle, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextServiceWorker, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextSharedWorker, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextWorker, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImage, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
}

// Tests that requests for scripts and styles are allowed
// if `require-sri-for` delivered in HTTP header requires integrity be present
TEST_F(ContentSecurityPolicyTest, RequireSRIForInHeaderPresentIntegrity) {
  KURL url(NullURL(), "https://example.test");
  IntegrityMetadataSet integrity_metadata;
  integrity_metadata.insert(
      IntegrityMetadata("1234", IntegrityAlgorithm::kSha384).ToPair());
  csp->BindToExecutionContext(execution_context.Get());
  // Enforce
  Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::Create();
  policy->BindToExecutionContext(execution_context.Get());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeEnforce,
                           kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextScript, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImport, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextStyle, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextServiceWorker, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextSharedWorker, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextWorker, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImage, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  // Content-Security-Policy-Report-Only is not supported in meta element,
  // so nothing should be blocked
  policy = ContentSecurityPolicy::Create();
  policy->BindToExecutionContext(execution_context.Get());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeReport,
                           kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextScript, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImport, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextStyle, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextServiceWorker, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextSharedWorker, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextWorker, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImage, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
}

// Tests that requests for scripts and styles are blocked
// if `require-sri-for` delivered in meta tag requires integrity be present
TEST_F(ContentSecurityPolicyTest, RequireSRIForInMetaMissingIntegrity) {
  KURL url(NullURL(), "https://example.test");
  // Enforce
  Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::Create();
  policy->BindToExecutionContext(execution_context.Get());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeEnforce,
                           kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_FALSE(policy->AllowRequest(
      WebURLRequest::kRequestContextScript, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      WebURLRequest::kRequestContextImport, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      WebURLRequest::kRequestContextStyle, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      WebURLRequest::kRequestContextServiceWorker, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      WebURLRequest::kRequestContextSharedWorker, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_FALSE(policy->AllowRequest(
      WebURLRequest::kRequestContextWorker, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImage, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  // Content-Security-Policy-Report-Only is not supported in meta element,
  // so nothing should be blocked
  policy = ContentSecurityPolicy::Create();
  policy->BindToExecutionContext(execution_context.Get());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeReport,
                           kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextScript, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImport, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextStyle, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextServiceWorker, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextSharedWorker, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextWorker, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImage, url, String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
}

// Tests that requests for scripts and styles are allowed
// if `require-sri-for` delivered meta tag requires integrity be present
TEST_F(ContentSecurityPolicyTest, RequireSRIForInMetaPresentIntegrity) {
  KURL url(NullURL(), "https://example.test");
  IntegrityMetadataSet integrity_metadata;
  integrity_metadata.insert(
      IntegrityMetadata("1234", IntegrityAlgorithm::kSha384).ToPair());
  csp->BindToExecutionContext(execution_context.Get());
  // Enforce
  Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::Create();
  policy->BindToExecutionContext(execution_context.Get());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeEnforce,
                           kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextScript, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImport, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextStyle, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextServiceWorker, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextSharedWorker, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextWorker, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImage, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  // Content-Security-Policy-Report-Only is not supported in meta element,
  // so nothing should be blocked
  policy = ContentSecurityPolicy::Create();
  policy->BindToExecutionContext(execution_context.Get());
  policy->DidReceiveHeader("require-sri-for script style",
                           kContentSecurityPolicyHeaderTypeReport,
                           kContentSecurityPolicyHeaderSourceMeta);
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextScript, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImport, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextStyle, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextServiceWorker, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextSharedWorker, url, String(),
      integrity_metadata, kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextWorker, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
  EXPECT_TRUE(policy->AllowRequest(
      WebURLRequest::kRequestContextImage, url, String(), integrity_metadata,
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));
}

TEST_F(ContentSecurityPolicyTest, NonceSinglePolicy) {
  struct TestCase {
    const char* policy;
    const char* url;
    const char* nonce;
    bool allowed;
  } cases[] = {
      {"script-src 'nonce-yay'", "https://example.com/js", "", false},
      {"script-src 'nonce-yay'", "https://example.com/js", "yay", true},
      {"script-src https://example.com", "https://example.com/js", "", true},
      {"script-src https://example.com", "https://example.com/js", "yay", true},
      {"script-src https://example.com 'nonce-yay'",
       "https://not.example.com/js", "", false},
      {"script-src https://example.com 'nonce-yay'",
       "https://not.example.com/js", "yay", true},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "Policy: `" << test.policy << "`, URL: `" << test.url
                 << "`, Nonce: `" << test.nonce << "`");
    KURL resource = KURL(NullURL(), test.url);

    unsigned expected_reports = test.allowed ? 0u : 1u;

    // Single enforce-mode policy should match `test.expected`:
    Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::Create();
    policy->BindToExecutionContext(execution_context.Get());
    policy->DidReceiveHeader(test.policy,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed, policy->AllowScriptFromSource(
                                resource, String(test.nonce),
                                IntegrityMetadataSet(), kParserInserted));
    // If this is expected to generate a violation, we should have sent a
    // report.
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Single report-mode policy should always be `true`:
    policy = ContentSecurityPolicy::Create();
    policy->BindToExecutionContext(execution_context.Get());
    policy->DidReceiveHeader(test.policy,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->AllowScriptFromSource(
        resource, String(test.nonce), IntegrityMetadataSet(), kParserInserted,
        ResourceRequest::RedirectStatus::kNoRedirect,
        SecurityViolationReportingPolicy::kReport,
        ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
    // If this is expected to generate a violation, we should have sent a
    // report, even though we don't deny access in `allowScriptFromSource`:
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());
  }
}

TEST_F(ContentSecurityPolicyTest, NonceInline) {
  struct TestCase {
    const char* policy;
    const char* nonce;
    bool allowed;
  } cases[] = {
      {"'unsafe-inline'", "", true},
      {"'unsafe-inline'", "yay", true},
      {"'nonce-yay'", "", false},
      {"'nonce-yay'", "yay", true},
      {"'unsafe-inline' 'nonce-yay'", "", false},
      {"'unsafe-inline' 'nonce-yay'", "yay", true},
  };

  String context_url;
  String content;
  WTF::OrdinalNumber context_line;

  // We need document for HTMLScriptElement tests.
  Document* document = Document::CreateForTest();
  document->SetSecurityOrigin(secure_origin);

  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message() << "Policy: `" << test.policy
                                      << "`, Nonce: `" << test.nonce << "`");

    unsigned expected_reports = test.allowed ? 0u : 1u;
    HTMLScriptElement* element = HTMLScriptElement::Create(*document, true);

    // Enforce 'script-src'
    Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::Create();
    policy->BindToExecutionContext(document);
    policy->DidReceiveHeader(String("script-src ") + test.policy,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed,
              policy->AllowInlineScript(
                  element, context_url, String(test.nonce), context_line,
                  content, ContentSecurityPolicy::InlineType::kBlock));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Enforce 'style-src'
    policy = ContentSecurityPolicy::Create();
    policy->BindToExecutionContext(document);
    policy->DidReceiveHeader(String("style-src ") + test.policy,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed,
              policy->AllowInlineStyle(
                  element, context_url, String(test.nonce), context_line,
                  content, ContentSecurityPolicy::InlineType::kBlock));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Report 'script-src'
    policy = ContentSecurityPolicy::Create();
    policy->BindToExecutionContext(document);
    policy->DidReceiveHeader(String("script-src ") + test.policy,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->AllowInlineScript(
        element, context_url, String(test.nonce), context_line, content,
        ContentSecurityPolicy::InlineType::kBlock));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Report 'style-src'
    policy = ContentSecurityPolicy::Create();
    policy->BindToExecutionContext(document);
    policy->DidReceiveHeader(String("style-src ") + test.policy,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->AllowInlineStyle(
        element, context_url, String(test.nonce), context_line, content,
        ContentSecurityPolicy::InlineType::kBlock));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());
  }
}

TEST_F(ContentSecurityPolicyTest, NonceMultiplePolicy) {
  struct TestCase {
    const char* policy1;
    const char* policy2;
    const char* url;
    const char* nonce;
    bool allowed1;
    bool allowed2;
  } cases[] = {
      // Passes both:
      {"script-src 'nonce-yay'", "script-src 'nonce-yay'",
       "https://example.com/js", "yay", true, true},
      {"script-src https://example.com", "script-src 'nonce-yay'",
       "https://example.com/js", "yay", true, true},
      {"script-src 'nonce-yay'", "script-src https://example.com",
       "https://example.com/js", "yay", true, true},
      {"script-src https://example.com 'nonce-yay'",
       "script-src https://example.com 'nonce-yay'", "https://example.com/js",
       "yay", true, true},
      {"script-src https://example.com 'nonce-yay'",
       "script-src https://example.com 'nonce-yay'", "https://example.com/js",
       "", true, true},
      {"script-src https://example.com",
       "script-src https://example.com 'nonce-yay'", "https://example.com/js",
       "yay", true, true},
      {"script-src https://example.com 'nonce-yay'",
       "script-src https://example.com", "https://example.com/js", "yay", true,
       true},

      // Fails one:
      {"script-src 'nonce-yay'", "script-src https://example.com",
       "https://example.com/js", "", false, true},
      {"script-src 'nonce-yay'", "script-src 'none'", "https://example.com/js",
       "yay", true, false},
      {"script-src 'nonce-yay'", "script-src https://not.example.com",
       "https://example.com/js", "yay", true, false},

      // Fails both:
      {"script-src 'nonce-yay'", "script-src https://example.com",
       "https://not.example.com/js", "", false, false},
      {"script-src https://example.com", "script-src 'nonce-yay'",
       "https://not.example.com/js", "", false, false},
      {"script-src 'nonce-yay'", "script-src 'none'",
       "https://not.example.com/js", "boo", false, false},
      {"script-src 'nonce-yay'", "script-src https://not.example.com",
       "https://example.com/js", "", false, false},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message() << "Policy: `" << test.policy1 << "`/`"
                                      << test.policy2 << "`, URL: `" << test.url
                                      << "`, Nonce: `" << test.nonce << "`");
    KURL resource = KURL(NullURL(), test.url);

    unsigned expected_reports =
        test.allowed1 != test.allowed2 ? 1u : (test.allowed1 ? 0u : 2u);

    // Enforce / Report
    Persistent<ContentSecurityPolicy> policy = ContentSecurityPolicy::Create();
    policy->BindToExecutionContext(execution_context.Get());
    policy->DidReceiveHeader(test.policy1,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    policy->DidReceiveHeader(test.policy2,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed1,
              policy->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kReport,
                  ContentSecurityPolicy::CheckHeaderType::kCheckEnforce));
    EXPECT_TRUE(policy->AllowScriptFromSource(
        resource, String(test.nonce), IntegrityMetadataSet(), kParserInserted,
        ResourceRequest::RedirectStatus::kNoRedirect,
        SecurityViolationReportingPolicy::kReport,
        ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Report / Enforce
    policy = ContentSecurityPolicy::Create();
    policy->BindToExecutionContext(execution_context.Get());
    policy->DidReceiveHeader(test.policy1,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    policy->DidReceiveHeader(test.policy2,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->AllowScriptFromSource(
        resource, String(test.nonce), IntegrityMetadataSet(), kParserInserted,
        ResourceRequest::RedirectStatus::kNoRedirect,
        SecurityViolationReportingPolicy::kReport,
        ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
    EXPECT_EQ(test.allowed2,
              policy->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kReport,
                  ContentSecurityPolicy::CheckHeaderType::kCheckEnforce));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Enforce / Enforce
    policy = ContentSecurityPolicy::Create();
    policy->BindToExecutionContext(execution_context.Get());
    policy->DidReceiveHeader(test.policy1,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    policy->DidReceiveHeader(test.policy2,
                             kContentSecurityPolicyHeaderTypeEnforce,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(test.allowed1 && test.allowed2,
              policy->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kReport,
                  ContentSecurityPolicy::CheckHeaderType::kCheckEnforce));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());

    // Report / Report
    policy = ContentSecurityPolicy::Create();
    policy->BindToExecutionContext(execution_context.Get());
    policy->DidReceiveHeader(test.policy1,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    policy->DidReceiveHeader(test.policy2,
                             kContentSecurityPolicyHeaderTypeReport,
                             kContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(policy->AllowScriptFromSource(
        resource, String(test.nonce), IntegrityMetadataSet(), kParserInserted,
        ResourceRequest::RedirectStatus::kNoRedirect,
        SecurityViolationReportingPolicy::kReport,
        ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly));
    EXPECT_EQ(expected_reports, policy->violation_reports_sent_.size());
  }
}

TEST_F(ContentSecurityPolicyTest, ShouldEnforceEmbeddersPolicy) {
  struct TestCase {
    const char* resource_url;
    const bool inherits;
  } cases[] = {
      // Same-origin
      {"https://example.test/index.html", true},
      // Cross-origin
      {"http://example.test/index.html", false},
      {"http://example.test:8443/index.html", false},
      {"https://example.test:8443/index.html", false},
      {"http://not.example.test/index.html", false},
      {"https://not.example.test/index.html", false},
      {"https://not.example.test:8443/index.html", false},

      // Inherit
      {"about:blank", true},
      {"data:text/html,yay", true},
      {"blob:https://example.test/bbe708f3-defd-4852-93b6-cf94e032f08d", true},
      {"filesystem:http://example.test/temporary/index.html", true},
  };

  for (const auto& test : cases) {
    ResourceResponse response;
    response.SetURL(KURL(kParsedURLString, test.resource_url));
    EXPECT_EQ(ContentSecurityPolicy::ShouldEnforceEmbeddersPolicy(
                  response, secure_origin.Get()),
              test.inherits);

    response.SetHTTPHeaderField(HTTPNames::Allow_CSP_From, AtomicString("*"));
    EXPECT_TRUE(ContentSecurityPolicy::ShouldEnforceEmbeddersPolicy(
        response, secure_origin.Get()));

    response.SetHTTPHeaderField(HTTPNames::Allow_CSP_From,
                                AtomicString("* not a valid header"));
    EXPECT_EQ(ContentSecurityPolicy::ShouldEnforceEmbeddersPolicy(
                  response, secure_origin.Get()),
              test.inherits);

    response.SetHTTPHeaderField(HTTPNames::Allow_CSP_From,
                                AtomicString("http://example.test"));
    EXPECT_EQ(ContentSecurityPolicy::ShouldEnforceEmbeddersPolicy(
                  response, secure_origin.Get()),
              test.inherits);

    response.SetHTTPHeaderField(HTTPNames::Allow_CSP_From,
                                AtomicString("https://example.test"));
    EXPECT_TRUE(ContentSecurityPolicy::ShouldEnforceEmbeddersPolicy(
        response, secure_origin.Get()));
  }
}

TEST_F(ContentSecurityPolicyTest, DirectiveType) {
  struct TestCase {
    ContentSecurityPolicy::DirectiveType type;
    const String& name;
  } cases[] = {
      {ContentSecurityPolicy::DirectiveType::kBaseURI, "base-uri"},
      {ContentSecurityPolicy::DirectiveType::kBlockAllMixedContent,
       "block-all-mixed-content"},
      {ContentSecurityPolicy::DirectiveType::kChildSrc, "child-src"},
      {ContentSecurityPolicy::DirectiveType::kConnectSrc, "connect-src"},
      {ContentSecurityPolicy::DirectiveType::kDefaultSrc, "default-src"},
      {ContentSecurityPolicy::DirectiveType::kFrameAncestors,
       "frame-ancestors"},
      {ContentSecurityPolicy::DirectiveType::kFrameSrc, "frame-src"},
      {ContentSecurityPolicy::DirectiveType::kFontSrc, "font-src"},
      {ContentSecurityPolicy::DirectiveType::kFormAction, "form-action"},
      {ContentSecurityPolicy::DirectiveType::kImgSrc, "img-src"},
      {ContentSecurityPolicy::DirectiveType::kManifestSrc, "manifest-src"},
      {ContentSecurityPolicy::DirectiveType::kMediaSrc, "media-src"},
      {ContentSecurityPolicy::DirectiveType::kObjectSrc, "object-src"},
      {ContentSecurityPolicy::DirectiveType::kPluginTypes, "plugin-types"},
      {ContentSecurityPolicy::DirectiveType::kReportURI, "report-uri"},
      {ContentSecurityPolicy::DirectiveType::kRequireSRIFor, "require-sri-for"},
      {ContentSecurityPolicy::DirectiveType::kSandbox, "sandbox"},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc, "script-src"},
      {ContentSecurityPolicy::DirectiveType::kStyleSrc, "style-src"},
      {ContentSecurityPolicy::DirectiveType::kTreatAsPublicAddress,
       "treat-as-public-address"},
      {ContentSecurityPolicy::DirectiveType::kUpgradeInsecureRequests,
       "upgrade-insecure-requests"},
      {ContentSecurityPolicy::DirectiveType::kWorkerSrc, "worker-src"},
  };

  EXPECT_EQ(ContentSecurityPolicy::DirectiveType::kUndefined,
            ContentSecurityPolicy::GetDirectiveType("random"));

  for (const auto& test : cases) {
    const String& name_from_type =
        ContentSecurityPolicy::GetDirectiveName(test.type);
    ContentSecurityPolicy::DirectiveType type_from_name =
        ContentSecurityPolicy::GetDirectiveType(test.name);
    EXPECT_EQ(name_from_type, test.name);
    EXPECT_EQ(type_from_name, test.type);
    EXPECT_EQ(test.type,
              ContentSecurityPolicy::GetDirectiveType(name_from_type));
    EXPECT_EQ(test.name,
              ContentSecurityPolicy::GetDirectiveName(type_from_name));
  }
}

TEST_F(ContentSecurityPolicyTest, Subsumes) {
  ContentSecurityPolicy* other = ContentSecurityPolicy::Create();
  EXPECT_TRUE(csp->Subsumes(*other));
  EXPECT_TRUE(other->Subsumes(*csp));

  csp->DidReceiveHeader("default-src http://example.com;",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);
  // If this CSP is not empty, the other must not be empty either.
  EXPECT_FALSE(csp->Subsumes(*other));
  EXPECT_TRUE(other->Subsumes(*csp));

  // Report-only policies do not impact subsumption.
  other->DidReceiveHeader("default-src http://example.com;",
                          kContentSecurityPolicyHeaderTypeReport,
                          kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->Subsumes(*other));

  // CSPDirectiveLists have to subsume.
  other->DidReceiveHeader("default-src http://example.com https://another.com;",
                          kContentSecurityPolicyHeaderTypeEnforce,
                          kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_FALSE(csp->Subsumes(*other));

  // `other` is stricter than `this`.
  other->DidReceiveHeader("default-src https://example.com;",
                          kContentSecurityPolicyHeaderTypeEnforce,
                          kContentSecurityPolicyHeaderSourceHTTP);
  EXPECT_TRUE(csp->Subsumes(*other));
}

TEST_F(ContentSecurityPolicyTest, RequestsAllowedWhenBypassingCSP) {
  KURL base;
  execution_context = CreateExecutionContext();
  execution_context->SetSecurityOrigin(secure_origin);  // https://example.com
  execution_context->SetURL(secure_url);                // https://example.com
  csp->BindToExecutionContext(execution_context.Get());
  csp->DidReceiveHeader("default-src https://example.com",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);

  EXPECT_TRUE(csp->AllowRequest(
      WebURLRequest::kRequestContextObject, KURL(base, "https://example.com/"),
      String(), IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  EXPECT_FALSE(csp->AllowRequest(
      WebURLRequest::kRequestContextObject,
      KURL(base, "https://not-example.com/"), String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  // Register "https" as bypassing CSP, which should now bypass it entirely
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(csp->AllowRequest(
      WebURLRequest::kRequestContextObject, KURL(base, "https://example.com/"),
      String(), IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  EXPECT_TRUE(csp->AllowRequest(
      WebURLRequest::kRequestContextObject,
      KURL(base, "https://not-example.com/"), String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}
TEST_F(ContentSecurityPolicyTest, FilesystemAllowedWhenBypassingCSP) {
  KURL base;
  execution_context = CreateExecutionContext();
  execution_context->SetSecurityOrigin(secure_origin);  // https://example.com
  execution_context->SetURL(secure_url);                // https://example.com
  csp->BindToExecutionContext(execution_context.Get());
  csp->DidReceiveHeader("default-src https://example.com",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);

  EXPECT_FALSE(
      csp->AllowRequest(WebURLRequest::kRequestContextObject,
                        KURL(base, "filesystem:https://example.com/file.txt"),
                        String(), IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));

  EXPECT_FALSE(csp->AllowRequest(
      WebURLRequest::kRequestContextObject,
      KURL(base, "filesystem:https://not-example.com/file.txt"), String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  // Register "https" as bypassing CSP, which should now bypass it entirely
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(
      csp->AllowRequest(WebURLRequest::kRequestContextObject,
                        KURL(base, "filesystem:https://example.com/file.txt"),
                        String(), IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));

  EXPECT_TRUE(csp->AllowRequest(
      WebURLRequest::kRequestContextObject,
      KURL(base, "filesystem:https://not-example.com/file.txt"), String(),
      IntegrityMetadataSet(), kParserInserted,
      ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}

TEST_F(ContentSecurityPolicyTest, BlobAllowedWhenBypassingCSP) {
  KURL base;
  execution_context = CreateExecutionContext();
  execution_context->SetSecurityOrigin(secure_origin);  // https://example.com
  execution_context->SetURL(secure_url);                // https://example.com
  csp->BindToExecutionContext(execution_context.Get());
  csp->DidReceiveHeader("default-src https://example.com",
                        kContentSecurityPolicyHeaderTypeEnforce,
                        kContentSecurityPolicyHeaderSourceHTTP);

  EXPECT_FALSE(csp->AllowRequest(
      WebURLRequest::kRequestContextObject,
      KURL(base, "blob:https://example.com/"), String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  EXPECT_FALSE(
      csp->AllowRequest(WebURLRequest::kRequestContextObject,
                        KURL(base, "blob:https://not-example.com/"), String(),
                        IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));

  // Register "https" as bypassing CSP, which should now bypass it entirely
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(csp->AllowRequest(
      WebURLRequest::kRequestContextObject,
      KURL(base, "blob:https://example.com/"), String(), IntegrityMetadataSet(),
      kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy::kSuppressReporting));

  EXPECT_TRUE(
      csp->AllowRequest(WebURLRequest::kRequestContextObject,
                        KURL(base, "blob:https://not-example.com/"), String(),
                        IntegrityMetadataSet(), kParserInserted,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));

  SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}

TEST_F(ContentSecurityPolicyTest, IsValidCSPAttrTest) {
  // Empty string is invalid
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(""));

  // Policy with single directive
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("base-uri http://example.com"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "invalid-policy-name http://example.com"));

  // Policy with multiple directives
  EXPECT_TRUE(ContentSecurityPolicy::IsValidCSPAttr(
      "base-uri http://example.com 'self'; child-src http://example.com; "
      "default-src http://example.com"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "default-src http://example.com; "
      "invalid-policy-name http://example.com"));

  // 'self', 'none'
  EXPECT_TRUE(ContentSecurityPolicy::IsValidCSPAttr("script-src 'self'"));
  EXPECT_TRUE(ContentSecurityPolicy::IsValidCSPAttr("default-src 'none'"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr("script-src 'slef'"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr("default-src 'non'"));

  // invalid ascii character
  EXPECT_FALSE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src https:  \x08"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1%2F%DFisnotSorB%2F"));

  // paths on script-src
  EXPECT_TRUE(ContentSecurityPolicy::IsValidCSPAttr("script-src 127.0.0.1:*/"));
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src 127.0.0.1:*/path"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1:*/path?query=string"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1:*/path#anchor"));
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src 127.0.0.1:8000/"));
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src 127.0.0.1:8000/path"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1:8000/path?query=string"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1:8000/path#anchor"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 127.0.0.1:8000/thisisa;pathwithasemicolon"));

  // script-src invalid hosts
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr("script-src http:/"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr("script-src http://"));
  EXPECT_FALSE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src http:/127.0.0.1"));
  EXPECT_FALSE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src http:///127.0.0.1"));
  EXPECT_FALSE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src http://127.0.0.1:/"));
  EXPECT_FALSE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src https://127.?.0.1:*"));
  EXPECT_FALSE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src https://127.0.0.1:"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src https://127.0.0.1:\t*   "));

  // script-src host wildcards
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src http://*.0.1:8000"));
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src http://*.0.1:8000/"));
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src http://*.0.1:*"));
  EXPECT_TRUE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src http://*.0.1:*/"));

  // missing semicolon
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "default-src 'self' script-src example.com"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 'self' object-src 'self' style-src *"));

  // 'none' with other sources
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src http://127.0.0.1:8000 'none'"));
  EXPECT_FALSE(
      ContentSecurityPolicy::IsValidCSPAttr("script-src 'none' 'none' 'none'"));

  // comma separated
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 'none', object-src 'none'"));

  // reporting not allowed
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "script-src 'none'; report-uri http://example.com/reporting"));
  EXPECT_FALSE(ContentSecurityPolicy::IsValidCSPAttr(
      "report-uri relative-path/reporting;"
      "base-uri http://example.com 'self'"));
  // TODO(andypaicu): when `report-to` is implemented, add tests here.
}

}  // namespace blink
