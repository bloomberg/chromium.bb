// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_document.h"

#include <string>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/web/web_origin_trials.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using blink::FrameTestHelpers::WebViewHelper;
using blink::URLTestHelpers::ToKURL;

const char kDefaultOrigin[] = "https://example.test/";
const char kManifestDummyFilePath[] = "manifest-dummy.html";
const char kOriginTrialDummyFilePath[] = "origin-trial-dummy.html";
const char kNoOriginTrialDummyFilePath[] = "simple_div.html";

class WebDocumentTest : public testing::Test {
 protected:
  static void SetUpTestCase();

  void LoadURL(const std::string& url);
  Document* TopDocument() const;
  WebDocument TopWebDocument() const;

  WebViewHelper web_view_helper_;
};

void WebDocumentTest::SetUpTestCase() {
  URLTestHelpers::RegisterMockedURLLoad(
      ToKURL(std::string(kDefaultOrigin) + kManifestDummyFilePath),
      test::CoreTestDataPath(kManifestDummyFilePath));
  URLTestHelpers::RegisterMockedURLLoad(
      ToKURL(std::string(kDefaultOrigin) + kNoOriginTrialDummyFilePath),
      test::CoreTestDataPath(kNoOriginTrialDummyFilePath));
  URLTestHelpers::RegisterMockedURLLoad(
      ToKURL(std::string(kDefaultOrigin) + kOriginTrialDummyFilePath),
      test::CoreTestDataPath(kOriginTrialDummyFilePath));
}

void WebDocumentTest::LoadURL(const std::string& url) {
  web_view_helper_.InitializeAndLoad(url);
}

Document* WebDocumentTest::TopDocument() const {
  return ToLocalFrame(web_view_helper_.GetWebView()->GetPage()->MainFrame())
      ->GetDocument();
}

WebDocument WebDocumentTest::TopWebDocument() const {
  return web_view_helper_.LocalMainFrame()->GetDocument();
}

TEST_F(WebDocumentTest, InsertAndRemoveStyleSheet) {
  LoadURL("about:blank");

  WebDocument web_doc = TopWebDocument();
  Document* core_doc = TopDocument();

  unsigned start_count = core_doc->GetStyleEngine().StyleForElementCount();

  WebStyleSheetKey style_sheet_key =
      web_doc.InsertStyleSheet("body { color: green }");

  // Check insertStyleSheet did not cause a synchronous style recalc.
  unsigned element_count =
      core_doc->GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  HTMLElement* body_element = core_doc->body();
  DCHECK(body_element);

  const ComputedStyle& style_before_insertion =
      body_element->ComputedStyleRef();

  // Inserted style sheet not yet applied.
  ASSERT_EQ(Color(0, 0, 0), style_before_insertion.VisitedDependentColor(
                                GetCSSPropertyColor()));

  // Apply inserted style sheet.
  core_doc->UpdateStyleAndLayoutTree();

  const ComputedStyle& style_after_insertion = body_element->ComputedStyleRef();

  // Inserted style sheet applied.
  ASSERT_EQ(Color(0, 128, 0),
            style_after_insertion.VisitedDependentColor(GetCSSPropertyColor()));

  start_count = core_doc->GetStyleEngine().StyleForElementCount();

  // Check RemoveInsertedStyleSheet did not cause a synchronous style recalc.
  web_doc.RemoveInsertedStyleSheet(style_sheet_key);
  element_count =
      core_doc->GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  const ComputedStyle& style_before_removing = body_element->ComputedStyleRef();

  // Removed style sheet not yet applied.
  ASSERT_EQ(Color(0, 128, 0),
            style_before_removing.VisitedDependentColor(GetCSSPropertyColor()));

  // Apply removed style sheet.
  core_doc->UpdateStyleAndLayoutTree();

  const ComputedStyle& style_after_removing = body_element->ComputedStyleRef();
  ASSERT_EQ(Color(0, 0, 0),
            style_after_removing.VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(WebDocumentTest, ManifestURL) {
  LoadURL(std::string(kDefaultOrigin) + kManifestDummyFilePath);

  WebDocument web_doc = TopWebDocument();
  Document* document = TopDocument();
  HTMLLinkElement* link_manifest = document->LinkManifest();

  // No href attribute was set.
  ASSERT_EQ(link_manifest->Href(), static_cast<KURL>(web_doc.ManifestURL()));

  // Set to some absolute url.
  link_manifest->setAttribute(HTMLNames::hrefAttr,
                              "http://example.com/manifest.json");
  ASSERT_EQ(link_manifest->Href(), static_cast<KURL>(web_doc.ManifestURL()));

  // Set to some relative url.
  link_manifest->setAttribute(HTMLNames::hrefAttr, "static/manifest.json");
  ASSERT_EQ(link_manifest->Href(), static_cast<KURL>(web_doc.ManifestURL()));
}

TEST_F(WebDocumentTest, ManifestUseCredentials) {
  LoadURL(std::string(kDefaultOrigin) + kManifestDummyFilePath);

  WebDocument web_doc = TopWebDocument();
  Document* document = TopDocument();
  HTMLLinkElement* link_manifest = document->LinkManifest();

  // No crossorigin attribute was set so credentials shouldn't be used.
  ASSERT_FALSE(link_manifest->FastHasAttribute(HTMLNames::crossoriginAttr));
  ASSERT_FALSE(web_doc.ManifestUseCredentials());

  // Crossorigin set to a random string shouldn't trigger using credentials.
  link_manifest->setAttribute(HTMLNames::crossoriginAttr, "foobar");
  ASSERT_FALSE(web_doc.ManifestUseCredentials());

  // Crossorigin set to 'anonymous' shouldn't trigger using credentials.
  link_manifest->setAttribute(HTMLNames::crossoriginAttr, "anonymous");
  ASSERT_FALSE(web_doc.ManifestUseCredentials());

  // Crossorigin set to 'use-credentials' should trigger using credentials.
  link_manifest->setAttribute(HTMLNames::crossoriginAttr, "use-credentials");
  ASSERT_TRUE(web_doc.ManifestUseCredentials());
}

// Origin Trial Policy which vends the test public key so that the token
// can be validated.
class TestOriginTrialPolicy : public blink::OriginTrialPolicy {
  bool IsOriginTrialsSupported() const override { return true; }
  base::StringPiece GetPublicKey() const override {
    // This is the public key which the test below will use to enable origin
    // trial features. Trial tokens for use in tests can be created with the
    // tool in /tools/origin_trials/generate_token.py, using the private key
    // contained in /tools/origin_trials/eftest.key.
    static const uint8_t kOriginTrialPublicKey[] = {
        0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
        0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
        0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
    };
    return base::StringPiece(
        reinterpret_cast<const char*>(kOriginTrialPublicKey),
        base::size(kOriginTrialPublicKey));
  }
  bool IsOriginSecure(const GURL& url) const override { return true; }
};

TEST_F(WebDocumentTest, OriginTrialDisabled) {
  // Set an origin trial policy.
  TestOriginTrialPolicy policy;
  blink::TrialTokenValidator::SetOriginTrialPolicyGetter(WTF::BindRepeating(
      [](TestOriginTrialPolicy* policy_ptr) -> blink::OriginTrialPolicy* {
        return policy_ptr;
      },
      base::Unretained(&policy)));

  // Load a document with no origin trial token.
  LoadURL(std::string(kDefaultOrigin) + kNoOriginTrialDummyFilePath);
  WebDocument web_doc = TopWebDocument();
  EXPECT_FALSE(WebOriginTrials::isTrialEnabled(&web_doc, "Frobulate"));
  // Reset the origin trial policy.
  TrialTokenValidator::ResetOriginTrialPolicyGetter();
}

TEST_F(WebDocumentTest, OriginTrialEnabled) {
  // Set an origin trial policy.
  TestOriginTrialPolicy policy;
  blink::TrialTokenValidator::SetOriginTrialPolicyGetter(WTF::BindRepeating(
      [](TestOriginTrialPolicy* policy_ptr) -> blink::OriginTrialPolicy* {
        return policy_ptr;
      },
      base::Unretained(&policy)));

  // Load a document with a valid origin trial token for the test trial.
  LoadURL(std::string(kDefaultOrigin) + kOriginTrialDummyFilePath);
  WebDocument web_doc = TopWebDocument();
  EXPECT_TRUE(WebOriginTrials::isTrialEnabled(&web_doc, "Frobulate"));
  // Ensure that other trials are not also enabled
  EXPECT_FALSE(WebOriginTrials::isTrialEnabled(&web_doc, "NotATrial"));
  // Reset the origin trial policy.
  TrialTokenValidator::ResetOriginTrialPolicyGetter();
}

namespace {

const char* g_base_url_origin_a = "http://example.test:0/";
const char* g_base_url_origin_sub_a = "http://subdomain.example.test:0/";
const char* g_base_url_origin_secure_a = "https://example.test:0/";
const char* g_base_url_origin_b = "http://not-example.test:0/";
const char* g_empty_file = "first_party/empty.html";
const char* g_nested_data = "first_party/nested-data.html";
const char* g_nested_origin_a = "first_party/nested-originA.html";
const char* g_nested_origin_sub_a = "first_party/nested-originSubA.html";
const char* g_nested_origin_secure_a = "first_party/nested-originSecureA.html";
const char* g_nested_origin_a_in_origin_a =
    "first_party/nested-originA-in-originA.html";
const char* g_nested_origin_a_in_origin_b =
    "first_party/nested-originA-in-originB.html";
const char* g_nested_origin_b = "first_party/nested-originB.html";
const char* g_nested_origin_b_in_origin_a =
    "first_party/nested-originB-in-originA.html";
const char* g_nested_origin_b_in_origin_b =
    "first_party/nested-originB-in-originB.html";
const char* g_nested_src_doc = "first_party/nested-srcdoc.html";

KURL ToOriginA(const char* file) {
  return ToKURL(std::string(g_base_url_origin_a) + file);
}

KURL ToOriginSubA(const char* file) {
  return ToKURL(std::string(g_base_url_origin_sub_a) + file);
}

KURL ToOriginSecureA(const char* file) {
  return ToKURL(std::string(g_base_url_origin_secure_a) + file);
}

KURL ToOriginB(const char* file) {
  return ToKURL(std::string(g_base_url_origin_b) + file);
}

void RegisterMockedURLLoad(const KURL& url, const char* path) {
  URLTestHelpers::RegisterMockedURLLoad(url, test::CoreTestDataPath(path));
}

}  // anonymous namespace

class WebDocumentFirstPartyTest : public WebDocumentTest {
 public:
  static void SetUpTestCase();

 protected:
  void Load(const char*);
  Document* NestedDocument() const;
  Document* NestedNestedDocument() const;
};

void WebDocumentFirstPartyTest::SetUpTestCase() {
  RegisterMockedURLLoad(ToOriginA(g_empty_file), g_empty_file);
  RegisterMockedURLLoad(ToOriginA(g_nested_data), g_nested_data);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_a), g_nested_origin_a);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_sub_a),
                        g_nested_origin_sub_a);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_secure_a),
                        g_nested_origin_secure_a);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_a_in_origin_a),
                        g_nested_origin_a_in_origin_a);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_a_in_origin_b),
                        g_nested_origin_a_in_origin_b);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_b), g_nested_origin_b);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_b_in_origin_a),
                        g_nested_origin_b_in_origin_a);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_b_in_origin_b),
                        g_nested_origin_b_in_origin_b);
  RegisterMockedURLLoad(ToOriginA(g_nested_src_doc), g_nested_src_doc);

  RegisterMockedURLLoad(ToOriginSubA(g_empty_file), g_empty_file);
  RegisterMockedURLLoad(ToOriginSecureA(g_empty_file), g_empty_file);

  RegisterMockedURLLoad(ToOriginB(g_empty_file), g_empty_file);
  RegisterMockedURLLoad(ToOriginB(g_nested_origin_a), g_nested_origin_a);
  RegisterMockedURLLoad(ToOriginB(g_nested_origin_b), g_nested_origin_b);
}

void WebDocumentFirstPartyTest::Load(const char* file) {
  web_view_helper_.InitializeAndLoad(std::string(g_base_url_origin_a) + file);
}

Document* WebDocumentFirstPartyTest::NestedDocument() const {
  return ToLocalFrame(web_view_helper_.GetWebView()
                          ->GetPage()
                          ->MainFrame()
                          ->Tree()
                          .FirstChild())
      ->GetDocument();
}

Document* WebDocumentFirstPartyTest::NestedNestedDocument() const {
  return ToLocalFrame(web_view_helper_.GetWebView()
                          ->GetPage()
                          ->MainFrame()
                          ->Tree()
                          .FirstChild()
                          ->Tree()
                          .FirstChild())
      ->GetDocument();
}

TEST_F(WebDocumentFirstPartyTest, Empty) {
  Load(g_empty_file);

  ASSERT_EQ(ToOriginA(g_empty_file), TopDocument()->SiteForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginA) {
  Load(g_nested_origin_a);

  ASSERT_EQ(ToOriginA(g_nested_origin_a), TopDocument()->SiteForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_a), NestedDocument()->SiteForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginSubA) {
  Load(g_nested_origin_sub_a);

  ASSERT_EQ(ToOriginA(g_nested_origin_sub_a), TopDocument()->SiteForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_sub_a),
            NestedDocument()->SiteForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginSecureA) {
  Load(g_nested_origin_secure_a);

  ASSERT_EQ(ToOriginA(g_nested_origin_secure_a),
            TopDocument()->SiteForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_secure_a),
            NestedDocument()->SiteForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginAInOriginA) {
  Load(g_nested_origin_a_in_origin_a);

  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_a),
            TopDocument()->SiteForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_a),
            NestedDocument()->SiteForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_a),
            NestedNestedDocument()->SiteForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginAInOriginB) {
  Load(g_nested_origin_a_in_origin_b);

  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_b),
            TopDocument()->SiteForCookies());
  ASSERT_EQ(NullURL(), NestedDocument()->SiteForCookies());
  ASSERT_EQ(NullURL(), NestedNestedDocument()->SiteForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginB) {
  Load(g_nested_origin_b);

  ASSERT_EQ(ToOriginA(g_nested_origin_b), TopDocument()->SiteForCookies());
  ASSERT_EQ(NullURL(), NestedDocument()->SiteForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginBInOriginA) {
  Load(g_nested_origin_b_in_origin_a);

  ASSERT_EQ(ToOriginA(g_nested_origin_b_in_origin_a),
            TopDocument()->SiteForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_b_in_origin_a),
            NestedDocument()->SiteForCookies());
  ASSERT_EQ(NullURL(), NestedNestedDocument()->SiteForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginBInOriginB) {
  Load(g_nested_origin_b_in_origin_b);

  ASSERT_EQ(ToOriginA(g_nested_origin_b_in_origin_b),
            TopDocument()->SiteForCookies());
  ASSERT_EQ(NullURL(), NestedDocument()->SiteForCookies());
  ASSERT_EQ(NullURL(), NestedNestedDocument()->SiteForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedSrcdoc) {
  Load(g_nested_src_doc);

  ASSERT_EQ(ToOriginA(g_nested_src_doc), TopDocument()->SiteForCookies());
  ASSERT_EQ(ToOriginA(g_nested_src_doc), NestedDocument()->SiteForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedData) {
  Load(g_nested_data);

  ASSERT_EQ(ToOriginA(g_nested_data), TopDocument()->SiteForCookies());
  ASSERT_EQ(NullURL(), NestedDocument()->SiteForCookies());
}

TEST_F(WebDocumentFirstPartyTest,
       NestedOriginAInOriginBWithFirstPartyOverride) {
  Load(g_nested_origin_a_in_origin_b);

  SchemeRegistry::RegisterURLSchemeAsFirstPartyWhenTopLevel("http");

  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_b),
            TopDocument()->SiteForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_b),
            NestedDocument()->SiteForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_b),
            NestedNestedDocument()->SiteForCookies());
}

}  // namespace blink
