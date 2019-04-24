// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_iframe_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

constexpr size_t expected_number_of_sandbox_features = 8;

class HTMLIFrameElementTest : public testing::Test {
 public:
  scoped_refptr<const SecurityOrigin> GetOriginForFeaturePolicy(
      HTMLIFrameElement* element) {
    return element->GetOriginForFeaturePolicy();
  }

 protected:
  const PolicyValue min_value = PolicyValue(false);
  const PolicyValue max_value = PolicyValue(true);
};

// Test that the correct origin is used when constructing the container policy,
// and that frames which should inherit their parent document's origin do so.
TEST_F(HTMLIFrameElementTest, FramesUseCorrectOrigin) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(html_names::kSrcAttr, "about:blank");
  scoped_refptr<const SecurityOrigin> effective_origin =
      GetOriginForFeaturePolicy(frame_element);
  EXPECT_TRUE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));

  frame_element->setAttribute(html_names::kSrcAttr,
                              "data:text/html;base64,PHRpdGxlPkFCQzwvdGl0bGU+");
  effective_origin = GetOriginForFeaturePolicy(frame_element);
  EXPECT_FALSE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
  EXPECT_TRUE(effective_origin->IsOpaque());

  frame_element->setAttribute(html_names::kSrcAttr, "http://example.net/");
  effective_origin = GetOriginForFeaturePolicy(frame_element);
  EXPECT_FALSE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
  EXPECT_FALSE(effective_origin->IsOpaque());
}

// Test that a unique origin is used when constructing the container policy in a
// sandboxed iframe.
TEST_F(HTMLIFrameElementTest, SandboxFramesUseCorrectOrigin) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(html_names::kSandboxAttr, "");
  frame_element->setAttribute(html_names::kSrcAttr, "http://example.com/");
  scoped_refptr<const SecurityOrigin> effective_origin =
      GetOriginForFeaturePolicy(frame_element);
  EXPECT_FALSE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
  EXPECT_TRUE(effective_origin->IsOpaque());

  frame_element->setAttribute(html_names::kSrcAttr, "http://example.net/");
  effective_origin = GetOriginForFeaturePolicy(frame_element);
  EXPECT_FALSE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
  EXPECT_TRUE(effective_origin->IsOpaque());
}

// Test that a sandboxed iframe with the allow-same-origin sandbox flag uses the
// parent document's origin for the container policy.
TEST_F(HTMLIFrameElementTest, SameOriginSandboxFramesUseCorrectOrigin) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(html_names::kSandboxAttr, "allow-same-origin");
  frame_element->setAttribute(html_names::kSrcAttr, "http://example.com/");
  scoped_refptr<const SecurityOrigin> effective_origin =
      GetOriginForFeaturePolicy(frame_element);
  EXPECT_TRUE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
  EXPECT_FALSE(effective_origin->IsOpaque());
}

// Test that the parent document's origin is used when constructing the
// container policy in a srcdoc iframe.
TEST_F(HTMLIFrameElementTest, SrcdocFramesUseCorrectOrigin) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(html_names::kSrcdocAttr, "<title>title</title>");
  scoped_refptr<const SecurityOrigin> effective_origin =
      GetOriginForFeaturePolicy(frame_element);
  EXPECT_TRUE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
}

// Test that a unique origin is used when constructing the container policy in a
// sandboxed iframe with a srcdoc.
TEST_F(HTMLIFrameElementTest, SandboxedSrcdocFramesUseCorrectOrigin) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(html_names::kSandboxAttr, "");
  frame_element->setAttribute(html_names::kSrcdocAttr, "<title>title</title>");
  scoped_refptr<const SecurityOrigin> effective_origin =
      GetOriginForFeaturePolicy(frame_element);
  EXPECT_FALSE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
  EXPECT_TRUE(effective_origin->IsOpaque());
}

// Test that iframes with relative src urls correctly construct their origin
// relative to the parent document.
TEST_F(HTMLIFrameElementTest, RelativeURLsUseCorrectOrigin) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  // Host-relative URLs should resolve to the same domain as the parent.
  frame_element->setAttribute(html_names::kSrcAttr, "index2.html");
  scoped_refptr<const SecurityOrigin> effective_origin =
      GetOriginForFeaturePolicy(frame_element);
  EXPECT_TRUE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));

  // Scheme-relative URLs should not resolve to the same domain as the parent.
  frame_element->setAttribute(html_names::kSrcAttr,
                              "//example.net/index2.html");
  effective_origin = GetOriginForFeaturePolicy(frame_element);
  EXPECT_FALSE(
      effective_origin->IsSameSchemeHostPort(document->GetSecurityOrigin()));
}

// Test that various iframe attribute configurations result in the correct
// container policies.

// Test that the correct container policy is constructed on an iframe element.
TEST_F(HTMLIFrameElementTest, DefaultContainerPolicy) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(html_names::kSrcAttr, "http://example.net/");
  frame_element->UpdateContainerPolicyForTests();

  const ParsedFeaturePolicy& container_policy =
      frame_element->ContainerPolicy();
  EXPECT_EQ(0UL, container_policy.size());
}

// Test that the allow attribute results in a container policy which is
// restricted to the domain in the src attribute.
TEST_F(HTMLIFrameElementTest, AllowAttributeContainerPolicy) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(html_names::kSrcAttr, "http://example.net/");
  frame_element->setAttribute(html_names::kAllowAttr, "fullscreen");
  frame_element->UpdateContainerPolicyForTests();

  const ParsedFeaturePolicy& container_policy1 =
      frame_element->ContainerPolicy();

  EXPECT_EQ(1UL, container_policy1.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen,
            container_policy1[0].feature);
  EXPECT_GE(min_value, container_policy1[0].fallback_value);
  EXPECT_EQ(1UL, container_policy1[0].values.size());
  EXPECT_EQ("http://example.net",
            container_policy1[0].values.begin()->first.Serialize());

  frame_element->setAttribute(html_names::kAllowAttr, "payment; fullscreen");
  frame_element->UpdateContainerPolicyForTests();

  const ParsedFeaturePolicy& container_policy2 =
      frame_element->ContainerPolicy();
  EXPECT_EQ(2UL, container_policy2.size());
  EXPECT_TRUE(container_policy2[0].feature ==
                  mojom::FeaturePolicyFeature::kFullscreen ||
              container_policy2[1].feature ==
                  mojom::FeaturePolicyFeature::kFullscreen);
  EXPECT_TRUE(
      container_policy2[0].feature == mojom::FeaturePolicyFeature::kPayment ||
      container_policy2[1].feature == mojom::FeaturePolicyFeature::kPayment);
  EXPECT_EQ(1UL, container_policy2[0].values.size());
  EXPECT_EQ("http://example.net",
            container_policy2[0].values.begin()->first.Serialize());
  EXPECT_GE(min_value, container_policy2[1].fallback_value);
  EXPECT_EQ(1UL, container_policy2[1].values.size());
  EXPECT_EQ("http://example.net",
            container_policy2[1].values.begin()->first.Serialize());
}

// Sandboxing an iframe should result in a container policy with an item for
// each sandbox feature
TEST_F(HTMLIFrameElementTest, SandboxAttributeContainerPolicy) {
  // TODO(iclelland): Remove the runtime feature override when this feature
  // becomes experimental or stable.
  ASSERT_FALSE(RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled());
  RuntimeEnabledFeatures::SetFeaturePolicyForSandboxEnabled(true);

  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(html_names::kSandboxAttr, "");
  frame_element->UpdateContainerPolicyForTests();

  const ParsedFeaturePolicy& container_policy =
      frame_element->ContainerPolicy();

  EXPECT_EQ(expected_number_of_sandbox_features, container_policy.size());
  // TODO: Check that each item is present.
  RuntimeEnabledFeatures::SetFeaturePolicyForSandboxEnabled(false);
}

// Test that the allow attribute on a sandboxed frame results in a container
// policy which is restricted to a unique origin.
TEST_F(HTMLIFrameElementTest, CrossOriginSandboxAttributeContainerPolicy) {
  // TODO(iclelland): Remove the runtime feature override when this feature
  // becomes experimental or stable.
  ASSERT_FALSE(RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled());
  RuntimeEnabledFeatures::SetFeaturePolicyForSandboxEnabled(true);
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(html_names::kSrcAttr, "http://example.net/");
  frame_element->setAttribute(html_names::kAllowAttr, "fullscreen");
  frame_element->setAttribute(html_names::kSandboxAttr, "");
  frame_element->UpdateContainerPolicyForTests();

  const ParsedFeaturePolicy& container_policy =
      frame_element->ContainerPolicy();

  EXPECT_EQ(expected_number_of_sandbox_features + 1, container_policy.size());
  const auto& container_policy_item = std::find_if(
      container_policy.begin(), container_policy.end(),
      [](const auto& declaration) {
        return declaration.feature == mojom::FeaturePolicyFeature::kFullscreen;
      });
  EXPECT_NE(container_policy_item, container_policy.end())
      << "Fullscreen feature not found in container policy";

  const ParsedFeaturePolicyDeclaration item = *container_policy_item;
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, item.feature);
  EXPECT_GE(min_value, item.fallback_value);
  EXPECT_LE(max_value, item.opaque_value);
  EXPECT_EQ(0UL, item.values.size());
  RuntimeEnabledFeatures::SetFeaturePolicyForSandboxEnabled(false);
}

// Test that the allow attribute on a sandboxed frame with the allow-same-origin
// flag results in a container policy which is restricted to the origin of the
// containing document.
TEST_F(HTMLIFrameElementTest, SameOriginSandboxAttributeContainerPolicy) {
  // TODO(iclelland): Remove the runtime feature override when this feature
  // becomes experimental or stable.
  ASSERT_FALSE(RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled());
  RuntimeEnabledFeatures::SetFeaturePolicyForSandboxEnabled(true);
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  frame_element->setAttribute(html_names::kSrcAttr, "http://example.net/");
  frame_element->setAttribute(html_names::kAllowAttr, "fullscreen");
  frame_element->setAttribute(html_names::kSandboxAttr, "allow-same-origin");
  frame_element->UpdateContainerPolicyForTests();

  const ParsedFeaturePolicy& container_policy =
      frame_element->ContainerPolicy();

  EXPECT_EQ(expected_number_of_sandbox_features + 1, container_policy.size());
  const auto& container_policy_item = std::find_if(
      container_policy.begin(), container_policy.end(),
      [](const auto& declaration) {
        return declaration.feature == mojom::FeaturePolicyFeature::kFullscreen;
      });
  EXPECT_NE(container_policy_item, container_policy.end())
      << "Fullscreen feature not found in container policy";

  const ParsedFeaturePolicyDeclaration item = *container_policy_item;
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, item.feature);
  EXPECT_GE(min_value, item.fallback_value);
  EXPECT_GE(min_value, item.opaque_value);
  EXPECT_EQ(1UL, item.values.size());
  EXPECT_FALSE(item.values.begin()->first.opaque());
  EXPECT_EQ("http://example.net", item.values.begin()->first.Serialize());
  RuntimeEnabledFeatures::SetFeaturePolicyForSandboxEnabled(false);
}

// Test the ConstructContainerPolicy method when no attributes are set on the
// iframe element.
TEST_F(HTMLIFrameElementTest, ConstructEmptyContainerPolicy) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);

  ParsedFeaturePolicy container_policy =
      frame_element->ConstructContainerPolicy(nullptr);
  EXPECT_EQ(0UL, container_policy.size());
}

// Test the ConstructContainerPolicy method when the "allow" attribute is used
// to enable features in the frame.
TEST_F(HTMLIFrameElementTest, ConstructContainerPolicy) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);
  frame_element->setAttribute(html_names::kAllowAttr, "payment; usb");
  ParsedFeaturePolicy container_policy =
      frame_element->ConstructContainerPolicy(nullptr);
  EXPECT_EQ(2UL, container_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kPayment, container_policy[0].feature);
  EXPECT_GE(min_value, container_policy[0].fallback_value);
  EXPECT_EQ(1UL, container_policy[0].values.size());
  EXPECT_TRUE(container_policy[0].values.begin()->first.IsSameOriginWith(
      GetOriginForFeaturePolicy(frame_element)->ToUrlOrigin()));
  EXPECT_EQ(mojom::FeaturePolicyFeature::kUsb, container_policy[1].feature);
  EXPECT_EQ(1UL, container_policy[1].values.size());
  EXPECT_TRUE(container_policy[1].values.begin()->first.IsSameOriginWith(
      GetOriginForFeaturePolicy(frame_element)->ToUrlOrigin()));
}

// Test the ConstructContainerPolicy method when the "allowfullscreen" attribute
// is used to enable fullscreen in the frame.
TEST_F(HTMLIFrameElementTest, ConstructContainerPolicyWithAllowFullscreen) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);
  frame_element->SetBooleanAttribute(html_names::kAllowfullscreenAttr, true);

  ParsedFeaturePolicy container_policy =
      frame_element->ConstructContainerPolicy(nullptr);
  EXPECT_EQ(1UL, container_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen,
            container_policy[0].feature);
  EXPECT_LE(max_value, container_policy[0].fallback_value);
}

// Test the ConstructContainerPolicy method when the "allowpaymentrequest"
// attribute is used to enable the paymentrequest API in the frame.
TEST_F(HTMLIFrameElementTest, ConstructContainerPolicyWithAllowPaymentRequest) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);
  frame_element->setAttribute(html_names::kAllowAttr, "usb");
  frame_element->SetBooleanAttribute(html_names::kAllowpaymentrequestAttr,
                                     true);

  ParsedFeaturePolicy container_policy =
      frame_element->ConstructContainerPolicy(nullptr);
  EXPECT_EQ(2UL, container_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kUsb, container_policy[0].feature);
  EXPECT_GE(min_value, container_policy[0].fallback_value);
  EXPECT_EQ(1UL, container_policy[0].values.size());
  EXPECT_TRUE(container_policy[0].values.begin()->first.IsSameOriginWith(
      GetOriginForFeaturePolicy(frame_element)->ToUrlOrigin()));
  EXPECT_EQ(mojom::FeaturePolicyFeature::kPayment, container_policy[1].feature);
}

// Test the ConstructContainerPolicy method when both "allowfullscreen" and
// "allowpaymentrequest" attributes are set on the iframe element, and the
// "allow" attribute is also used to override the paymentrequest feature. In the
// resulting container policy, the payment and usb features should be enabled
// only for the frame's origin, (since the allow attribute overrides
// allowpaymentrequest,) while fullscreen should be enabled for all origins.
TEST_F(HTMLIFrameElementTest, ConstructContainerPolicyWithAllowAttributes) {
  Document* document = Document::CreateForTest();
  const KURL document_url("http://example.com");
  document->SetURL(document_url);
  document->UpdateSecurityOrigin(SecurityOrigin::Create(document_url));

  HTMLIFrameElement* frame_element = HTMLIFrameElement::Create(*document);
  frame_element->setAttribute(html_names::kAllowAttr, "payment; usb");
  frame_element->SetBooleanAttribute(html_names::kAllowfullscreenAttr, true);
  frame_element->SetBooleanAttribute(html_names::kAllowpaymentrequestAttr,
                                     true);

  ParsedFeaturePolicy container_policy =
      frame_element->ConstructContainerPolicy(nullptr);
  EXPECT_EQ(3UL, container_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kPayment, container_policy[0].feature);
  EXPECT_GE(min_value, container_policy[0].fallback_value);
  EXPECT_EQ(1UL, container_policy[0].values.size());
  EXPECT_TRUE(container_policy[0].values.begin()->first.IsSameOriginWith(
      GetOriginForFeaturePolicy(frame_element)->ToUrlOrigin()));
  EXPECT_EQ(mojom::FeaturePolicyFeature::kUsb, container_policy[1].feature);
  EXPECT_EQ(1UL, container_policy[1].values.size());
  EXPECT_TRUE(container_policy[1].values.begin()->first.IsSameOriginWith(
      GetOriginForFeaturePolicy(frame_element)->ToUrlOrigin()));
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen,
            container_policy[2].feature);
}

}  // namespace blink
