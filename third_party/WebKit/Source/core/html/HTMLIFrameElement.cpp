/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Ericsson AB. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/html/HTMLIFrameElement.h"

#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLDocument.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutIFrame.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

using namespace HTMLNames;

inline HTMLIFrameElement::HTMLIFrameElement(Document& document)
    : HTMLFrameElementBase(iframeTag, document),
      did_load_non_empty_document_(false),
      collapsed_by_client_(false),
      sandbox_(HTMLIFrameElementSandbox::Create(this)),
      allow_(HTMLIFrameElementAllow::Create(this)),
      referrer_policy_(kReferrerPolicyDefault) {}

DEFINE_NODE_FACTORY(HTMLIFrameElement)

DEFINE_TRACE(HTMLIFrameElement) {
  visitor->Trace(sandbox_);
  visitor->Trace(allow_);
  HTMLFrameElementBase::Trace(visitor);
  Supplementable<HTMLIFrameElement>::Trace(visitor);
}

HTMLIFrameElement::~HTMLIFrameElement() {}

void HTMLIFrameElement::SetCollapsed(bool collapse) {
  if (collapsed_by_client_ == collapse)
    return;

  collapsed_by_client_ = collapse;

  // This is always called in response to an IPC, so should not happen in the
  // middle of a style recalc.
  DCHECK(!GetDocument().InStyleRecalc());
  LazyReattachIfAttached();
}

DOMTokenList* HTMLIFrameElement::sandbox() const {
  return sandbox_.Get();
}

DOMTokenList* HTMLIFrameElement::allow() const {
  return allow_.Get();
}

bool HTMLIFrameElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == widthAttr || name == heightAttr || name == alignAttr ||
      name == frameborderAttr)
    return true;
  return HTMLFrameElementBase::IsPresentationAttribute(name);
}

void HTMLIFrameElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == widthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyWidth, value);
  } else if (name == heightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyHeight, value);
  } else if (name == alignAttr) {
    ApplyAlignmentAttributeToStyle(value, style);
  } else if (name == frameborderAttr) {
    // LocalFrame border doesn't really match the HTML4 spec definition for
    // iframes. It simply adds a presentational hint that the border should be
    // off if set to zero.
    if (!value.ToInt()) {
      // Add a rule that nulls out our border width.
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyBorderWidth, 0,
          CSSPrimitiveValue::UnitType::kPixels);
    }
  } else {
    HTMLFrameElementBase::CollectStyleForPresentationAttribute(name, value,
                                                               style);
  }
}

void HTMLIFrameElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == nameAttr) {
    if (IsInDocumentTree() && GetDocument().IsHTMLDocument()) {
      HTMLDocument& document = ToHTMLDocument(this->GetDocument());
      document.RemoveExtraNamedItem(name_);
      document.AddExtraNamedItem(value);
    }
    AtomicString old_name = name_;
    name_ = value;
    if (name_ != old_name)
      FrameOwnerPropertiesChanged();
  } else if (name == sandboxAttr) {
    sandbox_->DidUpdateAttributeValue(params.old_value, value);
    String invalid_tokens;
    SetSandboxFlags(value.IsNull() ? kSandboxNone
                                   : ParseSandboxPolicy(sandbox_->TokenSet(),
                                                        invalid_tokens));
    if (!invalid_tokens.IsNull()) {
      GetDocument().AddConsoleMessage(ConsoleMessage::Create(
          kOtherMessageSource, kErrorMessageLevel,
          "Error while parsing the 'sandbox' attribute: " + invalid_tokens));
    }
    UseCounter::Count(GetDocument(), WebFeature::kSandboxViaIFrame);
  } else if (name == referrerpolicyAttr) {
    referrer_policy_ = kReferrerPolicyDefault;
    if (!value.IsNull()) {
      SecurityPolicy::ReferrerPolicyFromString(
          value, kSupportReferrerPolicyLegacyKeywords, &referrer_policy_);
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLIFrameElementReferrerPolicyAttribute);
    }
  } else if (name == allowfullscreenAttr) {
    bool old_allow_fullscreen = allow_fullscreen_;
    allow_fullscreen_ = !value.IsNull();
    if (allow_fullscreen_ != old_allow_fullscreen) {
      // TODO(iclelland): Remove this use counter when the allowfullscreen
      // attribute state is snapshotted on document creation. crbug.com/682282
      if (allow_fullscreen_ && ContentFrame()) {
        UseCounter::Count(
            GetDocument(),
            WebFeature::
                kHTMLIFrameElementAllowfullscreenAttributeSetAfterContentLoad);
      }
      FrameOwnerPropertiesChanged();
      UpdateContainerPolicy();
    }
  } else if (name == allowpaymentrequestAttr) {
    bool old_allow_payment_request = allow_payment_request_;
    allow_payment_request_ = !value.IsNull();
    if (allow_payment_request_ != old_allow_payment_request) {
      FrameOwnerPropertiesChanged();
      UpdateContainerPolicy();
    }
  } else if (RuntimeEnabledFeatures::EmbedderCSPEnforcementEnabled() &&
             name == cspAttr) {
    if (!ContentSecurityPolicy::IsValidCSPAttr(value.GetString())) {
      csp_ = g_null_atom;
      GetDocument().AddConsoleMessage(ConsoleMessage::Create(
          kOtherMessageSource, kErrorMessageLevel,
          "'csp' attribute is not a valid policy: " + value));
      return;
    }
    AtomicString old_csp = csp_;
    csp_ = value;
    if (csp_ != old_csp)
      FrameOwnerPropertiesChanged();
  } else if (RuntimeEnabledFeatures::FeaturePolicyEnabled() &&
             name == allowAttr) {
    allow_->DidUpdateAttributeValue(params.old_value, value);
    String invalid_tokens;
    allowed_features_ = allow_->ParseAllowedFeatureNames(invalid_tokens);
    if (!invalid_tokens.IsNull()) {
      GetDocument().AddConsoleMessage(ConsoleMessage::Create(
          kOtherMessageSource, kErrorMessageLevel,
          "Error while parsing the 'allow' attribute: " + invalid_tokens));
    }
    FrameOwnerPropertiesChanged();
    UpdateContainerPolicy();
    UseCounter::Count(GetDocument(), WebFeature::kFeaturePolicyAllowAttribute);
  } else {
    if (name == srcAttr)
      LogUpdateAttributeIfIsolatedWorldAndInDocument("iframe", params);
    HTMLFrameElementBase::ParseAttribute(params);
  }
}

Vector<WebParsedFeaturePolicyDeclaration>
HTMLIFrameElement::ConstructContainerPolicy() const {
  RefPtr<SecurityOrigin> origin = GetOriginForFeaturePolicy();
  Vector<WebParsedFeaturePolicyDeclaration> container_policy;

  // Populate the initial container policy from the allow attribute.
  for (const WebFeaturePolicyFeature feature : AllowedFeatures()) {
    WebParsedFeaturePolicyDeclaration whitelist;
    whitelist.feature = feature;
    whitelist.origins = Vector<WebSecurityOrigin>(1UL, {origin});
    container_policy.push_back(whitelist);
  }

  // If allowfullscreen attribute is present and no fullscreen policy is set,
  // enable the feature for all origins.
  if (AllowFullscreen()) {
    bool has_fullscreen_policy = false;
    for (const auto& declaration : container_policy) {
      if (declaration.feature == WebFeaturePolicyFeature::kFullscreen) {
        has_fullscreen_policy = true;
        break;
      }
    }
    if (!has_fullscreen_policy) {
      WebParsedFeaturePolicyDeclaration whitelist;
      whitelist.feature = WebFeaturePolicyFeature::kFullscreen;
      whitelist.matches_all_origins = true;
      whitelist.origins = Vector<WebSecurityOrigin>(0UL);
      container_policy.push_back(whitelist);
    }
  }
  // If the allowpaymentrequest attribute is present and no 'payment' policy is
  // set, enable the feature for all origins.
  if (AllowPaymentRequest()) {
    bool has_payment_policy = false;
    for (const auto& declaration : container_policy) {
      if (declaration.feature == WebFeaturePolicyFeature::kPayment) {
        has_payment_policy = true;
        break;
      }
    }
    if (!has_payment_policy) {
      WebParsedFeaturePolicyDeclaration whitelist;
      whitelist.feature = WebFeaturePolicyFeature::kPayment;
      whitelist.matches_all_origins = true;
      whitelist.origins = Vector<WebSecurityOrigin>(0UL);
      container_policy.push_back(whitelist);
    }
  }

  return container_policy;
}

bool HTMLIFrameElement::LayoutObjectIsNeeded(const ComputedStyle& style) {
  return ContentFrame() && !collapsed_by_client_ &&
         HTMLElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLIFrameElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutIFrame(this);
}

Node::InsertionNotificationRequest HTMLIFrameElement::InsertedInto(
    ContainerNode* insertion_point) {
  InsertionNotificationRequest result =
      HTMLFrameElementBase::InsertedInto(insertion_point);
  if (insertion_point->IsInDocumentTree() && GetDocument().IsHTMLDocument())
    ToHTMLDocument(GetDocument()).AddExtraNamedItem(name_);
  LogAddElementIfIsolatedWorldAndInDocument("iframe", srcAttr);
  return result;
}

void HTMLIFrameElement::RemovedFrom(ContainerNode* insertion_point) {
  HTMLFrameElementBase::RemovedFrom(insertion_point);
  if (insertion_point->IsInDocumentTree() && GetDocument().IsHTMLDocument())
    ToHTMLDocument(GetDocument()).RemoveExtraNamedItem(name_);
}

bool HTMLIFrameElement::IsInteractiveContent() const {
  return true;
}

ReferrerPolicy HTMLIFrameElement::ReferrerPolicyAttribute() {
  return referrer_policy_;
}

}  // namespace blink
