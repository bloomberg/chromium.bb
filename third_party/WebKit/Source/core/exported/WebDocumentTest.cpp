// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebDocument.h"

#include <string>

#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/page/Page.h"
#include "core/style/ComputedStyle.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/Color.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using blink::FrameTestHelpers::WebViewHelper;
using blink::URLTestHelpers::ToKURL;

const char kDefaultOrigin[] = "https://example.test/";
const char kManifestDummyFilePath[] = "manifest-dummy.html";

class WebDocumentTest : public ::testing::Test {
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
      testing::CoreTestDataPath(kManifestDummyFilePath));
}

void WebDocumentTest::LoadURL(const std::string& url) {
  web_view_helper_.InitializeAndLoad(url);
}

Document* WebDocumentTest::TopDocument() const {
  return ToLocalFrame(web_view_helper_.WebView()->GetPage()->MainFrame())
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

  WebStyleSheetId stylesheet_id =
      web_doc.InsertStyleSheet("body { color: green }");

  // Check insertStyleSheet did not cause a synchronous style recalc.
  unsigned element_count =
      core_doc->GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  HTMLElement* body_element = core_doc->body();
  DCHECK(body_element);

  const ComputedStyle& style_before_insertion =
      body_element->ComputedStyleRef();

  // Inserted stylesheet not yet applied.
  ASSERT_EQ(Color(0, 0, 0),
            style_before_insertion.VisitedDependentColor(CSSPropertyColor));

  // Apply inserted stylesheet.
  core_doc->UpdateStyleAndLayoutTree();

  const ComputedStyle& style_after_insertion = body_element->ComputedStyleRef();

  // Inserted stylesheet applied.
  ASSERT_EQ(Color(0, 128, 0),
            style_after_insertion.VisitedDependentColor(CSSPropertyColor));

  start_count = core_doc->GetStyleEngine().StyleForElementCount();

  // Check RemoveInsertedStyleSheet did not cause a synchronous style recalc.
  web_doc.RemoveInsertedStyleSheet(stylesheet_id);
  element_count =
      core_doc->GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  const ComputedStyle& style_before_removing = body_element->ComputedStyleRef();

  // Removed stylesheet not yet applied.
  ASSERT_EQ(Color(0, 128, 0),
            style_before_removing.VisitedDependentColor(CSSPropertyColor));

  // Apply removed stylesheet.
  core_doc->UpdateStyleAndLayoutTree();

  const ComputedStyle& style_after_removing = body_element->ComputedStyleRef();
  ASSERT_EQ(Color(0, 0, 0),
            style_after_removing.VisitedDependentColor(CSSPropertyColor));
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
  URLTestHelpers::RegisterMockedURLLoad(url, testing::CoreTestDataPath(path));
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
  return ToLocalFrame(web_view_helper_.WebView()
                          ->GetPage()
                          ->MainFrame()
                          ->Tree()
                          .FirstChild())
      ->GetDocument();
}

Document* WebDocumentFirstPartyTest::NestedNestedDocument() const {
  return ToLocalFrame(web_view_helper_.WebView()
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

  ASSERT_EQ(ToOriginA(g_empty_file), TopDocument()->FirstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginA) {
  Load(g_nested_origin_a);

  ASSERT_EQ(ToOriginA(g_nested_origin_a),
            TopDocument()->FirstPartyForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_a),
            NestedDocument()->FirstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginSubA) {
  Load(g_nested_origin_sub_a);

  ASSERT_EQ(ToOriginA(g_nested_origin_sub_a),
            TopDocument()->FirstPartyForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_sub_a),
            NestedDocument()->FirstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginSecureA) {
  Load(g_nested_origin_secure_a);

  ASSERT_EQ(ToOriginA(g_nested_origin_secure_a),
            TopDocument()->FirstPartyForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_secure_a),
            NestedDocument()->FirstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginAInOriginA) {
  Load(g_nested_origin_a_in_origin_a);

  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_a),
            TopDocument()->FirstPartyForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_a),
            NestedDocument()->FirstPartyForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_a),
            NestedNestedDocument()->FirstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginAInOriginB) {
  Load(g_nested_origin_a_in_origin_b);

  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_b),
            TopDocument()->FirstPartyForCookies());
  ASSERT_EQ(SecurityOrigin::UrlWithUniqueSecurityOrigin(),
            NestedDocument()->FirstPartyForCookies());
  ASSERT_EQ(SecurityOrigin::UrlWithUniqueSecurityOrigin(),
            NestedNestedDocument()->FirstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginB) {
  Load(g_nested_origin_b);

  ASSERT_EQ(ToOriginA(g_nested_origin_b),
            TopDocument()->FirstPartyForCookies());
  ASSERT_EQ(SecurityOrigin::UrlWithUniqueSecurityOrigin(),
            NestedDocument()->FirstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginBInOriginA) {
  Load(g_nested_origin_b_in_origin_a);

  ASSERT_EQ(ToOriginA(g_nested_origin_b_in_origin_a),
            TopDocument()->FirstPartyForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_b_in_origin_a),
            NestedDocument()->FirstPartyForCookies());
  ASSERT_EQ(SecurityOrigin::UrlWithUniqueSecurityOrigin(),
            NestedNestedDocument()->FirstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginBInOriginB) {
  Load(g_nested_origin_b_in_origin_b);

  ASSERT_EQ(ToOriginA(g_nested_origin_b_in_origin_b),
            TopDocument()->FirstPartyForCookies());
  ASSERT_EQ(SecurityOrigin::UrlWithUniqueSecurityOrigin(),
            NestedDocument()->FirstPartyForCookies());
  ASSERT_EQ(SecurityOrigin::UrlWithUniqueSecurityOrigin(),
            NestedNestedDocument()->FirstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedSrcdoc) {
  Load(g_nested_src_doc);

  ASSERT_EQ(ToOriginA(g_nested_src_doc), TopDocument()->FirstPartyForCookies());
  ASSERT_EQ(ToOriginA(g_nested_src_doc),
            NestedDocument()->FirstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedData) {
  Load(g_nested_data);

  ASSERT_EQ(ToOriginA(g_nested_data), TopDocument()->FirstPartyForCookies());
  ASSERT_EQ(SecurityOrigin::UrlWithUniqueSecurityOrigin(),
            NestedDocument()->FirstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest,
       NestedOriginAInOriginBWithFirstPartyOverride) {
  Load(g_nested_origin_a_in_origin_b);

  SchemeRegistry::RegisterURLSchemeAsFirstPartyWhenTopLevel("http");

  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_b),
            TopDocument()->FirstPartyForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_b),
            NestedDocument()->FirstPartyForCookies());
  ASSERT_EQ(ToOriginA(g_nested_origin_a_in_origin_b),
            NestedNestedDocument()->FirstPartyForCookies());
}

}  // namespace blink
