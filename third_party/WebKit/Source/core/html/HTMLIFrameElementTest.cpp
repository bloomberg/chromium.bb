// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElement.h"

#include "core/dom/Document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HTMLIFrameElementTest : public ::testing::Test {
 public:
  RefPtr<SecurityOrigin> GetOriginForFeaturePolicy(HTMLIFrameElement* element) {
    return element->GetOriginForFeaturePolicy();
  }
};

// Test setting feature policy via the Element attribute (HTML codepath).
TEST_F(HTMLIFrameElementTest, SetAllowAttribute) {
  Document* document = Document::Create();
  HTMLIFrameElement* iframe = HTMLIFrameElement::Create(*document);

  iframe->setAttribute(HTMLNames::allowAttr, "fullscreen");
  EXPECT_EQ("fullscreen", iframe->allow()->value());
  iframe->setAttribute(HTMLNames::allowAttr, "fullscreen vibrate");
  EXPECT_EQ("fullscreen vibrate", iframe->allow()->value());
}

// Test setting feature policy via the DOMTokenList (JS codepath).
TEST_F(HTMLIFrameElementTest, SetAllowAttributeJS) {
  Document* document = Document::Create();
  HTMLIFrameElement* iframe = HTMLIFrameElement::Create(*document);

  iframe->allow()->setValue("fullscreen");
  EXPECT_EQ("fullscreen", iframe->getAttribute(HTMLNames::allowAttr));
}

// Test that the correct origin is used when constructing the container policy,
// and that frames which should inherit their parent document's origin do so.
TEST_F(HTMLIFrameElementTest, FramesUseCorrectOrigin) {
  Document* document = Document::Create();
  KURL document_url = KURL(KURL(), "http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(HTMLNames::srcAttr, "about:blank");
  RefPtr<SecurityOrigin> effective_origin =
      GetOriginForFeaturePolicy(frame_element);
  EXPECT_TRUE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));

  frame_element->setAttribute(HTMLNames::srcAttr,
                              "data:text/html;base64,PHRpdGxlPkFCQzwvdGl0bGU+");
  effective_origin = GetOriginForFeaturePolicy(frame_element);
  EXPECT_FALSE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
  EXPECT_TRUE(effective_origin->IsUnique());

  frame_element->setAttribute(HTMLNames::srcAttr, "http://example.net/");
  effective_origin = GetOriginForFeaturePolicy(frame_element);
  EXPECT_FALSE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
  EXPECT_FALSE(effective_origin->IsUnique());
}

// Test that a unique origin is used when constructing the container policy in a
// sandboxed iframe.
TEST_F(HTMLIFrameElementTest, SandboxFramesUseCorrectOrigin) {
  Document* document = Document::Create();
  KURL document_url = KURL(KURL(), "http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(HTMLNames::sandboxAttr, "");
  frame_element->setAttribute(HTMLNames::srcAttr, "http://example.com/");
  RefPtr<SecurityOrigin> effective_origin =
      GetOriginForFeaturePolicy(frame_element);
  EXPECT_FALSE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
  EXPECT_TRUE(effective_origin->IsUnique());

  frame_element->setAttribute(HTMLNames::srcAttr, "http://example.net/");
  effective_origin = GetOriginForFeaturePolicy(frame_element);
  EXPECT_FALSE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
  EXPECT_TRUE(effective_origin->IsUnique());
}

// Test that a sandboxed iframe with the allow-same-origin sandbox flag uses the
// parent document's origin for the container policy.
TEST_F(HTMLIFrameElementTest, SameOriginSandboxFramesUseCorrectOrigin) {
  Document* document = Document::Create();
  KURL document_url = KURL(KURL(), "http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(HTMLNames::sandboxAttr, "allow-same-origin");
  frame_element->setAttribute(HTMLNames::srcAttr, "http://example.com/");
  RefPtr<SecurityOrigin> effective_origin =
      GetOriginForFeaturePolicy(frame_element);
  EXPECT_TRUE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
  EXPECT_FALSE(effective_origin->IsUnique());
}

// Test that the parent document's origin is used when constructing the
// container policy in a srcdoc iframe.
TEST_F(HTMLIFrameElementTest, SrcdocFramesUseCorrectOrigin) {
  Document* document = Document::Create();
  KURL document_url = KURL(KURL(), "http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(HTMLNames::srcdocAttr, "<title>title</title>");
  RefPtr<SecurityOrigin> effective_origin =
      GetOriginForFeaturePolicy(frame_element);
  EXPECT_TRUE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
}

// Test that a unique origin is used when constructing the container policy in a
// sandboxed iframe with a srcdoc.
TEST_F(HTMLIFrameElementTest, SandboxedSrcdocFramesUseCorrectOrigin) {
  Document* document = Document::Create();
  KURL document_url = KURL(KURL(), "http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(HTMLNames::sandboxAttr, "");
  frame_element->setAttribute(HTMLNames::srcdocAttr, "<title>title</title>");
  RefPtr<SecurityOrigin> effective_origin =
      GetOriginForFeaturePolicy(frame_element);
  EXPECT_FALSE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
  EXPECT_TRUE(effective_origin->IsUnique());
}

// Test that iframes with relative src urls correctly construct their origin
// relative to the parent document.
TEST_F(HTMLIFrameElementTest, RelativeURLsUseCorrectOrigin) {
  Document* document = Document::Create();
  KURL document_url = KURL(KURL(), "http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  // Host-relative URLs should resolve to the same domain as the parent.
  frame_element->setAttribute(HTMLNames::srcAttr, "index2.html");
  RefPtr<SecurityOrigin> effective_origin =
      GetOriginForFeaturePolicy(frame_element);
  EXPECT_TRUE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));

  // Scheme-relative URLs should not resolve to the same domain as the parent.
  frame_element->setAttribute(HTMLNames::srcAttr, "//example.net/index2.html");
  effective_origin = GetOriginForFeaturePolicy(frame_element);
  EXPECT_FALSE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
}

// Test that various iframe attribute configurations result in the correct
// container policies.

// Test that the correct container policy is constructed on an iframe element.
TEST_F(HTMLIFrameElementTest, DefaultContainerPolicy) {
  Document* document = Document::Create();
  KURL document_url = KURL(KURL(), "http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(HTMLNames::srcAttr, "http://example.net/");
  frame_element->UpdateContainerPolicyForTests();

  const WebParsedFeaturePolicy& container_policy =
      frame_element->ContainerPolicy();
  EXPECT_EQ(0UL, container_policy.size());
}

// Test that the allow attribute results in a container policy which is
// restricted to the domain in the src attribute.
TEST_F(HTMLIFrameElementTest, AllowAttributeContainerPolicy) {
  Document* document = Document::Create();
  KURL document_url = KURL(KURL(), "http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(HTMLNames::srcAttr, "http://example.net/");
  frame_element->setAttribute(HTMLNames::allowAttr, "fullscreen");
  frame_element->UpdateContainerPolicyForTests();

  const WebParsedFeaturePolicy& container_policy1 =
      frame_element->ContainerPolicy();

  EXPECT_EQ(1UL, container_policy1.size());
  EXPECT_EQ(WebFeaturePolicyFeature::kFullscreen, container_policy1[0].feature);
  EXPECT_FALSE(container_policy1[0].matches_all_origins);
  EXPECT_EQ(1UL, container_policy1[0].origins.size());
  EXPECT_EQ("http://example.net", container_policy1[0].origins[0].ToString());

  frame_element->setAttribute(HTMLNames::allowAttr, "payment fullscreen");
  frame_element->UpdateContainerPolicyForTests();

  const WebParsedFeaturePolicy& container_policy2 =
      frame_element->ContainerPolicy();
  EXPECT_EQ(2UL, container_policy2.size());
  EXPECT_TRUE(
      container_policy2[0].feature == WebFeaturePolicyFeature::kFullscreen ||
      container_policy2[1].feature == WebFeaturePolicyFeature::kFullscreen);
  EXPECT_TRUE(
      container_policy2[0].feature == WebFeaturePolicyFeature::kPayment ||
      container_policy2[1].feature == WebFeaturePolicyFeature::kPayment);
  EXPECT_FALSE(container_policy2[0].matches_all_origins);
  EXPECT_EQ(1UL, container_policy2[0].origins.size());
  EXPECT_EQ("http://example.net", container_policy2[0].origins[0].ToString());
  EXPECT_FALSE(container_policy2[1].matches_all_origins);
  EXPECT_EQ(1UL, container_policy2[1].origins.size());
  EXPECT_EQ("http://example.net", container_policy2[1].origins[0].ToString());
}

// Test that the allow attribute on a sandboxed frame results in a container
// policy which is restricted to a unique origin.
TEST_F(HTMLIFrameElementTest, SandboxAttributeContainerPolicy) {
  Document* document = Document::Create();
  KURL document_url = KURL(KURL(), "http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(HTMLNames::srcAttr, "http://example.net/");
  frame_element->setAttribute(HTMLNames::allowAttr, "fullscreen");
  frame_element->setAttribute(HTMLNames::sandboxAttr, "");
  frame_element->UpdateContainerPolicyForTests();

  const WebParsedFeaturePolicy& container_policy =
      frame_element->ContainerPolicy();

  EXPECT_EQ(1UL, container_policy.size());
  EXPECT_EQ(WebFeaturePolicyFeature::kFullscreen, container_policy[0].feature);
  EXPECT_FALSE(container_policy[0].matches_all_origins);
  EXPECT_EQ(1UL, container_policy[0].origins.size());
  EXPECT_TRUE(container_policy[0].origins[0].IsUnique());
}

// Test that the allow attribute on a sandboxed frame with the allow-same-origin
// flag results in a container policy which is restricted to the origin of the
// containing document.
TEST_F(HTMLIFrameElementTest, SameOriginSandboxAttributeContainerPolicy) {
  Document* document = Document::Create();
  KURL document_url = KURL(KURL(), "http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(HTMLNames::srcAttr, "http://example.net/");
  frame_element->setAttribute(HTMLNames::allowAttr, "fullscreen");
  frame_element->setAttribute(HTMLNames::sandboxAttr, "allow-same-origin");
  frame_element->UpdateContainerPolicyForTests();

  const WebParsedFeaturePolicy& container_policy =
      frame_element->ContainerPolicy();

  EXPECT_EQ(1UL, container_policy.size());
  EXPECT_EQ(WebFeaturePolicyFeature::kFullscreen, container_policy[0].feature);
  EXPECT_FALSE(container_policy[0].matches_all_origins);
  EXPECT_EQ(1UL, container_policy[0].origins.size());
  EXPECT_FALSE(container_policy[0].origins[0].IsUnique());
  EXPECT_EQ("http://example.net", container_policy[0].origins[0].ToString());
}

}  // namespace blink
