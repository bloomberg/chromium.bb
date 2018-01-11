/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/svg/properties/SVGAnimatedProperty.h"

#include "core/svg/SVGElement.h"

namespace blink {

SVGAnimatedPropertyBase::SVGAnimatedPropertyBase(
    AnimatedPropertyType type,
    SVGElement* context_element,
    const QualifiedName& attribute_name,
    CSSPropertyID css_property_id)
    : type_(type),
      // Cast to avoid warnings about unsafe bitfield truncations of the CSS
      // property enum. CSS properties that don't fit in this bitfield are never
      // used here. See static_assert in header.
      css_property_id_(static_cast<unsigned>(css_property_id)),
      context_element_(context_element),
      attribute_name_(attribute_name) {
  DCHECK(context_element_);
  DCHECK(attribute_name_ != QualifiedName::Null());
  DCHECK_EQ(this->GetType(), type);
  DCHECK_EQ(this->CssPropertyId(), css_property_id);
}

SVGAnimatedPropertyBase::~SVGAnimatedPropertyBase() = default;

void SVGAnimatedPropertyBase::AnimationEnded() {
  SynchronizeAttribute();
}

void SVGAnimatedPropertyBase::SynchronizeAttribute() {
  AtomicString value(CurrentValueBase()->ValueAsString());
  context_element_->SetSynchronizedLazyAttribute(attribute_name_, value);
}

bool SVGAnimatedPropertyBase::IsSpecified() const {
  return IsAnimating() || contextElement()->hasAttribute(AttributeName());
}

}  // namespace blink
