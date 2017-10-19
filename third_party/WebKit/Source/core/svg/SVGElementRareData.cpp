// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGElementRareData.h"

#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/svg/SVGElementProxy.h"

namespace blink {

MutableStylePropertySet*
SVGElementRareData::EnsureAnimatedSMILStyleProperties() {
  if (!animated_smil_style_properties_)
    animated_smil_style_properties_ =
        MutableStylePropertySet::Create(kSVGAttributeMode);
  return animated_smil_style_properties_.Get();
}

ComputedStyle* SVGElementRareData::OverrideComputedStyle(
    Element* element,
    const ComputedStyle* parent_style) {
  DCHECK(element);
  if (!use_override_computed_style_)
    return nullptr;
  if (!override_computed_style_ || needs_override_computed_style_update_) {
    // The style computed here contains no CSS Animations/Transitions or SMIL
    // induced rules - this is needed to compute the "base value" for the SMIL
    // animation sandwhich model.
    override_computed_style_ =
        element->GetDocument().EnsureStyleResolver().StyleForElement(
            element, parent_style, parent_style, kMatchAllRulesExcludingSMIL);
    needs_override_computed_style_update_ = false;
  }
  DCHECK(override_computed_style_);
  return override_computed_style_.get();
}

void SVGElementRareData::Trace(blink::Visitor* visitor) {
  visitor->Trace(outgoing_references_);
  visitor->Trace(incoming_references_);
  visitor->Trace(element_proxy_set_);
  visitor->Trace(animated_smil_style_properties_);
  visitor->Trace(element_instances_);
  visitor->Trace(corresponding_element_);
  visitor->Trace(owner_);
}

AffineTransform* SVGElementRareData::AnimateMotionTransform() {
  return &animate_motion_transform_;
}

SVGElementProxySet& SVGElementRareData::EnsureElementProxySet() {
  if (!element_proxy_set_)
    element_proxy_set_ = new SVGElementProxySet;
  return *element_proxy_set_;
}

}  // namespace blink
